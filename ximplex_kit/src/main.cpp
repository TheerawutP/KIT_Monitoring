
// Libraries
#include <WiFi.h>
#include "Arduino.h"
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include "wifi_credentials.h"
#include "WebSocketsServer.h"
#include "DNSServer.h"
#include <PubSubClient.h>
#include <ModbusMaster.h>

#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>

#include <Preferences.h>

const char *firmwareURL = "https://raw.githubusercontent.com/TheerawutP/test_OTA/main/MCU.ino.bin";
bool shouldUpdateFirmware = false; 
TaskHandle_t pollingTaskHandle = NULL;
TaskHandle_t publishTaskHandle = NULL;

ModbusMaster node;

#define PIN_RX 16
#define PIN_TX 17
#define WIFI_READY 22
#define slaveNum 2
#define PLC_slaveID 1
#define SERVO_slaveID 2
#define subTopicNum 10
#define X0_ADD 0
#define Y0_ADD 4

#define X_size 8 // 8*8 input
#define Y_size 8 // 8*8 output

uint16_t hreg[8][16];

// credential
const char *ssid = "Flinkone 1-2.4G";
const char *password = "ff112335";
const char *mqtt_broker = "kit.flinkone.com";
const int mqtt_port = 1883; // unencrypt

// topics
char *KIT_topic = "kit";
char *UT_case = "/UT_0000";
char *system_status = "/sys_v1";
char *X_status = "/sys_v1/X_status";
char *Y_status = "/sys_v1/Y_status";
char *SERVO_status = "/servo/status";
char *SERVO_ALM = "/servo/alarm";

char x_mqtt_topic[64];
char y_mqtt_topic[64];

// subscription topics
char *KIT_listenToAll = "kit/#";

// char* SERVO_toque =
// char* SERVO_speed =

// typedef struct {
//   char from[8];
//   uint16_t* data;
//   size_t len;
// } msg;
// msg sub_buff[subTopicNum];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

SemaphoreHandle_t mqttMutex; // Mutex to protect MQTT client
SemaphoreHandle_t hasChangedMutex;

enum read_state
{
  PLC,
  SERVO
};

read_state curr_slave = PLC;
read_state last_slave = PLC;

// String X_status_payload;
// String Y_status_payload;
// String X_prev;
// String Y_prev;
char X_status_payload[64];
char Y_status_payload[64];
char X_prev[64] = "";
char Y_prev[64] = "";

bool XY_hasChanged = true;
uint32_t ChangeSlaveInterval = 500;

// create webserver object for website:
AsyncWebServer server(80); //

WebSocketsServer m_websocketserver = WebSocketsServer(81);
String m_websocketserver_text_to_send = "";
String m_websocketserver_text_to_send_2 = "";
int m_time_sent_websocketserver_text = millis();
int m_microsbefore_websocketsendcalled = micros();
int last_time_loop_called = millis();
int last_time_sent_websocket_server = millis();
float m_websocket_send_rate = 1.0; // Hz, how often to send a test point to websocket...
bool m_send_websocket_test_data_in_loop = false;

// WifiTool object
int WAIT_FOR_WIFI_TIME_OUT = 6000;
const char *PARAM_MESSAGE = "message"; // message server receives from client
std::unique_ptr<DNSServer> dnsServer;
std::unique_ptr<AsyncWebServer> m_wifitools_server;
const byte DNS_PORT = 53;
bool restartSystem = false;
String temp_json_string = "";

Preferences preferences;

int SET_StopPoints = 2;        // รับจาก name="floor_param"
float SET_UpDuration = 18.5;   // รับจาก name="up_param"
float SET_DownDuration = 18.0; // รับจาก name="down_param"
int SET_Param4 = 0;            // รับจาก name="alice_param"
int SET_Param5 = 0;            // รับจาก name="bob_param"



void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String message = "";
  for (int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == "kit/print")
  {
    Serial.print("Print command received: ");
    Serial.println(message);
  }


}

void setupMQTT()
{
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(callback);
  mqttClient.setBufferSize(512);
}

void isChange(const char *from, uint16_t *data, bool *flag)
{
  // We use temporary buffers to format the new data
  char X_temp[64];
  char Y_temp[64];

  if (strcmp(from, "PLC") == 0)
  { // strcmp is the C way to compare char arrays

    // 1. Format X Data (Indices 0-3)
    // %d is a placeholder for an integer.
    // This line replaces the entire first loop.
    snprintf(X_temp, sizeof(X_temp), "%d,%d,%d,%d",
             data[0], data[1], data[2], data[3]);

    // 2. Format Y Data (Indices 4-7)
    // This line replaces the entire second loop.
    snprintf(Y_temp, sizeof(Y_temp), "%d,%d,%d,%d",
             data[4], data[5], data[6], data[7]);

    // 3. Compare with previous values
    // strcmp returns 0 if strings are identical
    if (strcmp(X_temp, X_prev) != 0 || strcmp(Y_temp, Y_prev) != 0)
    {
      *flag = true;

      // Copy new values to "prev" history
      // strncpy is safer than strcpy because it respects size
      strncpy(X_prev, X_temp, sizeof(X_prev));
      strncpy(Y_prev, Y_temp, sizeof(Y_prev));

      // Update the global payloads
      strncpy(X_status_payload, X_temp, sizeof(X_status_payload));
      strncpy(Y_status_payload, Y_temp, sizeof(Y_status_payload));

      Serial.println("Change Detected!");
      Serial.print("X: ");
      Serial.println(X_status_payload);
      Serial.print("Y: ");
      Serial.println(Y_status_payload);
    }
  }
}

void publishMqtt(const char *topic, const char *msg)
{
  if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE)
  {
    if (mqttClient.connected())
    {
      bool result = mqttClient.publish(topic, msg, true);
      if (result)
      {
        Serial.println("publish success");
      }
      else
      {
        Serial.println("publish fail");
      }
      // Serial.printf("Pub: %s -> %s\n", topic, msg);
    }
    xSemaphoreGive(mqttMutex);
  }
}

void vReconnectTask(void *pvParams)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      // Protect Check/Connect with Mutex
      if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE)
      {
        if (!mqttClient.connected())
        {
          Serial.print("MQTT connecting...");

          String clientId = "ESP32-" + String(random(0xffff), HEX); // Unique ID

          if (mqttClient.connect(clientId.c_str()))
          {
            Serial.println("connected");
            mqttClient.subscribe(KIT_listenToAll);
            // Re-subscribe here if needed
          }
          else
          {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
          }
        }

        // IMPORTANT: loop() must be called frequently to maintain connection
        if (mqttClient.connected())
        {
          mqttClient.loop();
        }

        xSemaphoreGive(mqttMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
  }
}

void vPollingTask(void *pvParams)
{
  for (;;)
  {
    uint32_t result;
    // if (xSemaphoreTake(hregMutex, portMAX_DELAY) == pdTRUE) {
    digitalWrite(WIFI_READY, HIGH);
    switch (curr_slave)
    {
    case PLC:
      node.begin(PLC_slaveID, Serial1);
      result = node.readHoldingRegisters(X0_ADD, 8); // start hreg address, num of read
      if (result == node.ku8MBSuccess)
      {
        hreg[PLC_slaveID][0] = node.getResponseBuffer(0);
        hreg[PLC_slaveID][1] = node.getResponseBuffer(1);
        hreg[PLC_slaveID][2] = node.getResponseBuffer(2);
        hreg[PLC_slaveID][3] = node.getResponseBuffer(3);

        hreg[PLC_slaveID][4] = node.getResponseBuffer(4);
        hreg[PLC_slaveID][5] = node.getResponseBuffer(5);
        hreg[PLC_slaveID][6] = node.getResponseBuffer(6);
        hreg[PLC_slaveID][7] = node.getResponseBuffer(7);

        if (xSemaphoreTake(hasChangedMutex, portMAX_DELAY) == pdTRUE)
        {
          isChange("PLC", hreg[PLC_slaveID], &XY_hasChanged);
          xSemaphoreGive(hasChangedMutex);
        }
      }
      else
      {
        Serial.println(result); // Check this code for timeouts (226) or invalid data (227)
      }
      last_slave = PLC;
      // curr_slave = SERVO;
      break;
      // case SERVO:
      //   break;
    }

    //   xSemaphoreGive(hregMutex);
    // }
    vTaskDelay(pdMS_TO_TICKS(ChangeSlaveInterval));
  }
}

void vPublishTask(void *pvParams)
{
  for (;;)
  {
    if (xSemaphoreTake(hasChangedMutex, portMAX_DELAY) == pdTRUE)
    {
      if (XY_hasChanged == true)
      {
        publishMqtt(x_mqtt_topic, X_status_payload);
        publishMqtt(y_mqtt_topic, Y_status_payload);
        XY_hasChanged = false;
      }
      xSemaphoreGive(hasChangedMutex);
    }
  }
}

boolean connectAttempt(String ssid, String password)
{
  boolean isWiFiConnected = false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  if (ssid == "")
  {
    WiFi.begin();
    Serial.print(F("Connecting to last known WiFi..."));
  }
  else
  {
    int ssidSize = ssid.length() + 1;
    int passwordSize = password.length() + 1;
    char ssidArray[ssidSize] = {0};
    char passwordArray[passwordSize] = {0};
    ssid.toCharArray(ssidArray, ssidSize);
    password.toCharArray(passwordArray, passwordSize);
    WiFi.begin(ssidArray, passwordArray);

    Serial.print(F("Connecting to SSID: "));
    Serial.println(ssid);
  }

  unsigned long now = millis();
  while (WiFi.status() != WL_CONNECTED && millis() < now + WAIT_FOR_WIFI_TIME_OUT)
  {
    Serial.print(".");
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("\nWiFi connected"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    isWiFiConnected = true;
  }
  else
  {
    Serial.println(F("\nWiFi connection failed."));
  }

  return isWiFiConnected;
}

bool readSSIDPWDfile(String m_pwd_filename_to_read)
{
  File m_pwd_file_to_read = SPIFFS.open(m_pwd_filename_to_read);

  if (!m_pwd_file_to_read)
  {
    Serial.println("Failed to open PWD file file for reading");
    return false;
  }

  if (!SPIFFS.exists(m_pwd_filename_to_read))
  {
    Serial.print(m_pwd_filename_to_read);
    Serial.println("  does not exist.");
    return false;
  }

  String m_pwd_file_string;
  while (m_pwd_file_to_read.available()) // read json from file
  {
    m_pwd_file_string += char(m_pwd_file_to_read.read());
  } // end while
  // Serial.print("m_pwd_file_string = ");
  // Serial.println(m_pwd_file_string);
  m_pwd_file_to_read.close();

  // parse
  StaticJsonDocument<1000> m_JSONdoc_from_pwd_file;
  DeserializationError m_error = deserializeJson(m_JSONdoc_from_pwd_file, m_pwd_file_string); // m_JSONdoc is now a json object
  if (m_error)
  {
    Serial.println("deserializeJson() failed with code ");
    Serial.println(m_error.c_str());
  }
  // m_JSONdoc_from_pwd_file is the JSON object now we can use it.
  String m_SSID1_name = m_JSONdoc_from_pwd_file["SSID1"];
  String m_SSID2_name = m_JSONdoc_from_pwd_file["SSID2"];
  String m_SSID3_name = m_JSONdoc_from_pwd_file["SSID3"];
  String m_PWD1_name = m_JSONdoc_from_pwd_file["PWD1"];
  String m_PWD2_name = m_JSONdoc_from_pwd_file["PWD1"];
  String m_PWD3_name = m_JSONdoc_from_pwd_file["PWD1"];
  // Serial.print("m_SSID1_name = ");
  // Serial.print(m_SSID1_name);
  // Serial.print(F("\t")); // tab
  // Serial.print("m_PWD1_name = ");
  // Serial.print(F("\t")); // tab
  // Serial.print("m_SSID2_name = ");
  // Serial.print(m_SSID2_name);
  // Serial.print(F("\t")); // tab
  // Serial.print("m_PWD2_name = ");
  // Serial.print(m_PWD2_name);
  // Serial.print(F("\t")); // tab
  // Serial.print("m_SSID3_name = ");
  // Serial.print(m_SSID3_name);
  // Serial.print(F("\t")); // tab
  // Serial.print("m_PWD3_name = ");
  // Serial.println(m_PWD3_name);

  // Try connecting:
  //****************************8
  if (connectAttempt(m_SSID1_name, m_PWD1_name))
  {
    return true;
  }
  Serial.println("Failed to connect.");
  if (connectAttempt(m_SSID2_name, m_PWD2_name))
  {
    return true;
  }
  Serial.println("Failed to connect.");

  if (connectAttempt(m_SSID3_name, m_PWD3_name))
  {
    return true;
  }
  Serial.println("Failed to connect.");

  return false;
}

void setUpAPService()
{
  Serial.println(F("Starting Access Point server."));

  // DNSServer dnsServer;
  // dnsServer.reset(new DNSServer());
  WiFi.mode(WIFI_AP);
  // WiFi.softAPConfig(IPAddress(172, 217, 28, 1), IPAddress(172, 217, 28, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP("Ximplex_KIT");
  delay(500);
}

void process()
{
  /// DNS
  // dnsServer->processNextRequest();
  // yield
  yield();
  delay(10);
  // Reset flag/timer
  if (restartSystem)
  {
    if (restartSystem + 1000 < millis())
    {
      ESP.restart();
    } // end if
  } // end if
}

void handleGetSavSecreteJson(AsyncWebServerRequest *request)
{
  String message;

  String m_SSID1_name;
  String m_SSID2_name;
  String m_SSID3_name;
  String m_PWD1_name;
  String m_PWD2_name;
  String m_PWD3_name;
  String m_temp_string;
  int params = request->params();
  for (int i = 0; i < params; i++)
  {
    AsyncWebParameter *p = request->getParam(i);
    if (p->isPost())
    {
      Serial.print(i);
      Serial.print(F("\t"));
      Serial.print(p->name().c_str());
      Serial.print(F("\t"));
      Serial.println(p->value().c_str());
      m_temp_string = p->name().c_str();
      if (m_temp_string == "ssid1")
      {
        m_SSID1_name = p->value().c_str();
      }
      else if (m_temp_string == "pass1")
      {
        m_PWD1_name = p->value().c_str();
      }
      else if (m_temp_string == "ssid2")
      {
        m_SSID2_name = p->value().c_str();
      }
      else if (m_temp_string == "pass2")
      {
        m_PWD2_name = p->value().c_str();
      }
      else if (m_temp_string == "ssid3")
      {
        m_SSID3_name = p->value().c_str();
      }
      else if (m_temp_string == "pass3")
      {
        m_PWD3_name = p->value().c_str();
      }
    }
  }
  if (request->hasParam(PARAM_MESSAGE, true))
  {
    message = request->getParam(PARAM_MESSAGE, true)->value();
    Serial.println(message);
  }
  else
  {
    message = "No message sent";
  }
  request->send(200, "text/HTML", "Credentials saved. Rebooting...");

  // {"SSID1":"myssid1xyz","PWD1":"mypwd1xyz",
  //     "SSID2":"myssid2xyz","PWD2":"mypwd2xyz",
  //     "SSID3":"myssid3xyz","PWD3":"mypwd3xyz"}

  String SSID_and_pwd_JSON = "";
  SSID_and_pwd_JSON += "{\"SSID1\":\"";
  SSID_and_pwd_JSON += m_SSID1_name;
  SSID_and_pwd_JSON += "\",\"PWD1\":\"";
  SSID_and_pwd_JSON += m_PWD1_name;

  SSID_and_pwd_JSON += "\",\"SSID2\":\"";
  SSID_and_pwd_JSON += m_SSID2_name;
  SSID_and_pwd_JSON += "\",\"PWD2\":\"";
  SSID_and_pwd_JSON += m_PWD2_name;

  SSID_and_pwd_JSON += "\",\"SSID3\":\"";
  SSID_and_pwd_JSON += m_SSID3_name;
  SSID_and_pwd_JSON += "\",\"PWD3\":\"";
  SSID_and_pwd_JSON += m_PWD3_name;

  SSID_and_pwd_JSON += "\"}";

  Serial.println("JSON string to write to file = ");
  Serial.println(SSID_and_pwd_JSON);

  Serial.print("m_SSID1_name = ");
  Serial.print(m_SSID1_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_PWD1_name = ");
  Serial.print(m_PWD1_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_SSID2_name = ");
  Serial.print(m_SSID2_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_PWD2_name = ");
  Serial.print(m_PWD2_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_SSID3_name = ");
  Serial.print(m_SSID3_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_PWD3_name = ");
  Serial.println(m_PWD3_name);

  File m_ssid_pwd_file_to_write_name = SPIFFS.open("/credentials.JSON", FILE_WRITE);

  if (!m_ssid_pwd_file_to_write_name)
  {
    Serial.println("There was an error opening the pwd/ssid file for writing");
    return;
  }

  Serial.println("Writing this JSON string to pwd/ssid file:");
  Serial.println(SSID_and_pwd_JSON);

  if (!m_ssid_pwd_file_to_write_name.println(SSID_and_pwd_JSON))
  {
    Serial.println("File write failed");
  }
  m_ssid_pwd_file_to_write_name.close();

  request->send(200, "text/html", "<h1>Restarting .....</h1>");
  restartSystem = millis();
}

void handleGetSavSecreteJsonNoReboot(AsyncWebServerRequest *request)
{
  String message;

  String m_SSID1_name;
  String m_SSID2_name;
  String m_SSID3_name;
  String m_PWD1_name;
  String m_PWD2_name;
  String m_PWD3_name;
  String m_temp_string;
  int params = request->params();
  for (int i = 0; i < params; i++)
  {
    AsyncWebParameter *p = request->getParam(i);
    if (p->isPost())
    {
      Serial.print(i);
      Serial.print(F("\t"));
      Serial.print(p->name().c_str());
      Serial.print(F("\t"));
      Serial.println(p->value().c_str());
      m_temp_string = p->name().c_str();
      if (m_temp_string == "ssid1")
      {
        m_SSID1_name = p->value().c_str();
      }
      else if (m_temp_string == "pass1")
      {
        m_PWD1_name = p->value().c_str();
      }
      else if (m_temp_string == "ssid2")
      {
        m_SSID2_name = p->value().c_str();
      }
      else if (m_temp_string == "pass2")
      {
        m_PWD2_name = p->value().c_str();
      }
      else if (m_temp_string == "ssid3")
      {
        m_SSID3_name = p->value().c_str();
      }
      else if (m_temp_string == "pass3")
      {
        m_PWD3_name = p->value().c_str();
      }
    }
  }
  if (request->hasParam(PARAM_MESSAGE, true))
  {
    message = request->getParam(PARAM_MESSAGE, true)->value();
    Serial.println(message);
  }
  else
  {
    message = "No message sent";
  }
  // request->send(200, "text/HTML", "bla bla bla bla bla xyz xyz xyz xyz xyz ");

  // {"SSID1":"myssid1xyz","PWD1":"mypwd1xyz",
  //     "SSID2":"myssid2xyz","PWD2":"mypwd2xyz",
  //     "SSID3":"myssid3xyz","PWD3":"mypwd3xyz"}

  String SSID_and_pwd_JSON = "";
  SSID_and_pwd_JSON += "{\"SSID1\":\"";
  SSID_and_pwd_JSON += m_SSID1_name;
  SSID_and_pwd_JSON += "\",\"PWD1\":\"";
  SSID_and_pwd_JSON += m_PWD1_name;

  SSID_and_pwd_JSON += "\",\"SSID2\":\"";
  SSID_and_pwd_JSON += m_SSID2_name;
  SSID_and_pwd_JSON += "\",\"PWD2\":\"";
  SSID_and_pwd_JSON += m_PWD2_name;

  SSID_and_pwd_JSON += "\",\"SSID3\":\"";
  SSID_and_pwd_JSON += m_SSID3_name;
  SSID_and_pwd_JSON += "\",\"PWD3\":\"";
  SSID_and_pwd_JSON += m_PWD3_name;

  SSID_and_pwd_JSON += "\"}";

  Serial.println("JSON string to write to file = ");
  Serial.println(SSID_and_pwd_JSON);

  Serial.print("m_SSID1_name = ");
  Serial.print(m_SSID1_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_PWD1_name = ");
  Serial.print(m_PWD1_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_SSID2_name = ");
  Serial.print(m_SSID2_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_PWD2_name = ");
  Serial.print(m_PWD2_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_SSID3_name = ");
  Serial.print(m_SSID3_name);
  Serial.print(F("\t")); // tab
  Serial.print("m_PWD3_name = ");
  Serial.println(m_PWD3_name);

  File m_ssid_pwd_file_to_write_name = SPIFFS.open("/credentials.JSON", FILE_WRITE);

  if (!m_ssid_pwd_file_to_write_name)
  {
    Serial.println("There was an error opening the pwd/ssid file for writing");
    return;
  }

  Serial.println("Writing this JSON string to pwd/ssid file:");
  Serial.println(SSID_and_pwd_JSON);

  if (!m_ssid_pwd_file_to_write_name.println(SSID_and_pwd_JSON))
  {
    Serial.println("File write failed");
  }
  m_ssid_pwd_file_to_write_name.close();

  request->send(200, "text/html", "  <head> <meta http-equiv=\"refresh\" content=\"2; URL=wifi.html\" /> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> </head> <body> <h1> Credentials stored to flash on NanoStat. </h1>  </body>");
  restartSystem = millis();
}

void getWifiScanJson(AsyncWebServerRequest *request)
{
  String json = "{\"scan_result\":[";
  int n = WiFi.scanComplete();
  if (n == -2)
  {
    WiFi.scanNetworks(true);
  }
  else if (n)
  {
    for (int i = 0; i < n; ++i)
    {
      if (i)
        json += ",";
      json += "{";
      json += "\"RSSI\":" + String(WiFi.RSSI(i));
      json += ",\"SSID\":\"" + WiFi.SSID(i) + "\"";
      json += "}";
    }
    WiFi.scanDelete();
    if (WiFi.scanComplete() == -2)
    {
      WiFi.scanNetworks(true);
    }
  }
  json += "]}";
  request->send(200, "application/json", json);
  json = String();
}

void runWifiPortal()
{

  m_wifitools_server.reset(new AsyncWebServer(80));

  IPAddress myIP;
  myIP = WiFi.softAPIP();
  // myIP = WiFi.localIP();
  Serial.print(F("AP IP address: "));
  Serial.println(myIP);

  // Need to tell server to accept packets from any source with any header via http methods GET, PUT:
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  // m_wifitools_server->serveStatic("/", SPIFFS, "/").setDefaultFile("wifi_index.html");
  m_wifitools_server->serveStatic("/", SPIFFS, "/").setDefaultFile("setup.html");

  // m_wifitools_server->on("/saveSecret/", HTTP_ANY, [&, this](AsyncWebServerRequest *request) {
  //   handleGetSavSecreteJson(request);
  // });

  m_wifitools_server->on("/saveSecret", HTTP_POST, [](AsyncWebServerRequest *request)
                         { handleGetSavSecreteJson(request); });

  // m_wifitools_server->on("/list", HTTP_ANY, [](AsyncWebServerRequest *request)
  //                        { handleFileList(request); });

  m_wifitools_server->on("/wifiScan.json", HTTP_GET, [](AsyncWebServerRequest *request)
                         { getWifiScanJson(request); });

  Serial.println(F("HTTP server started"));
  m_wifitools_server->begin();
  if (!MDNS.begin("keepintouch")) // see https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
  {
    Serial.println("Error setting up MDNS responder !");
    while (1)
      ;
    {
      delay(1000);
    }
  }
  Serial.println("MDNS started.");

  unsigned long apModeStartTime = millis();
  const unsigned long AP_TIMEOUT = 180000; // 30000 ms = 30 วินาที
  // MDNS.begin("nanostat");
  while (1) // loop until user hits restart... Once credentials saved, won't end up here again unless wifi not connecting!
  {
    process();
    if (millis() - apModeStartTime > AP_TIMEOUT)
    {
      Serial.println("AP Mode Timeout (3mins exceeded). Restarting system...");
      delay(500);    
      ESP.restart(); 
    }
  }
}

void runWifiPortal_after_connected_to_WIFI()
{
  // Don't run this after starting server or ESP32 will crash!!!
  server.on("/saveSecret/", HTTP_POST, [](AsyncWebServerRequest *request)
            { handleGetSavSecreteJsonNoReboot(request); });

  Serial.println(F("HTTP server started"));
  m_wifitools_server->begin();
  if (!MDNS.begin("keepintouch")) // see https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
  {
    Serial.println("Error setting up MDNS responder !");
    while (1)
      ;
    {
      delay(1000);
    }
  }
  Serial.println("MDNS started.");
  // MDNS.begin("nanostat");
  while (1) // loop until user hits restart... Once credentials saved, won't end up here again unless wifi not connecting!
  {
    process();
  }
}

void handle_websocket_text(uint8_t *payload)
{
  // do something...
  Serial.printf("handle_websocket_text called for: %s\n", payload);

  // test JSON parsing...
  // char m_JSONMessage[] = "{\"Key1\":123,\"Key2\",345}";
  // StaticJsonDocument<1000> m_JSONdoc;
  // deserializeJson(m_JSONdoc, m_JSONMessage); // m_JSONdoc is now a json object
  // int m_key1_value = m_JSONdoc["Key1"];
  // Serial.println(m_key1_value);

  // Parse JSON payload
  StaticJsonDocument<1000> m_JSONdoc_from_payload;
  DeserializationError m_error = deserializeJson(m_JSONdoc_from_payload, payload); // m_JSONdoc is now a json object
  if (m_error)
  {
    Serial.println("deserializeJson() failed with code ");
    Serial.println(m_error.c_str());
  }
  // Serial.println(m_key2_value);
  // now to iterate over (unknown) keys, we have to cast the StaticJsonDocument object into a JsonObject:
  // see https://techtutorialsx.com/2019/07/09/esp32-arduinojson-printing-the-keys-of-the-jsondocument/
  JsonObject m_JsonObject_from_payload = m_JSONdoc_from_payload.as<JsonObject>();
  // Iterate and print to serial:
  //   uint8_t LMPgain_control_panel = 6; // Feedback resistor of TIA.
  // int num_adc_readings_to_average_control_panel = 1;
  // int sweep_param_delayTime_ms_control_panel = 50;
  // int cell_voltage_control_panel = 100;

  // if (true == false) // if key = "change_cell_voltage_to"
  // {
  //   m_websocket_send_rate = (float)atof((const char *)&payload[0]); // adjust data send rate used in loop
  // }
  // deserializeJson(m_JSONdoc, payload); // m_JSONdoc is now a json object that was payload delivered by websocket message
}

void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t *payload,
                      size_t length)
{
  // Serial.println("onWebSocketEvent called");
  // Figure out the type of WebSocket event
  switch (type)
  {

  // Client has disconnected
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\n", num);
    break;

  // New client has connected
  case WStype_CONNECTED:
  {
    IPAddress ip = m_websocketserver.remoteIP(num);
    Serial.printf("[%u] Connection from ", num);
    Serial.println(ip.toString());
  }
  break;

  // Echo text message back to client
  case WStype_TEXT:
    // Serial.println(payload[0,length-1]); // this doesn't work....
    Serial.printf("[%u] Received text: %s\n", num, payload);
    // m_websocketserver.sendTXT(num, payload);
    // if (true == false) // later change to if message has certain format:
    // {
    //   m_websocket_send_rate = (float)atof((const char *)&payload[0]); // adjust data send rate used in loop
    // }
    handle_websocket_text(payload);

    break;

  // For everything else: do nothing
  case WStype_BIN:
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
  default:
    break;
  }
}

void configureserver()
// configures server
{
  // Need to tell server to accept packets from any source with any header via http methods GET, PUT:
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  // Button #1
  server.addHandler(new AsyncCallbackJsonWebHandler("/on_Button_UP_pressed", [](AsyncWebServerRequest *request1, JsonVariant &json1)
                                                    {
                                                      const JsonObject &jsonObj1 = json1.as<JsonObject>();
                                                      if (jsonObj1["on"])
                                                      {
                                                        // Serial.println("Up button pressed.");
                                                        // Serial.println("------------------");
                                                        // ws_cmd = true;
                                                        // ws_cmd_value = toFloor2;
                                                      }
                                                      request1->send(200, "OK"); }));

  // Button #2
  server.addHandler(new AsyncCallbackJsonWebHandler("/on_Button_DOWN_pressed", [](AsyncWebServerRequest *request2, JsonVariant &json2)
                                                    {
                                                      const JsonObject &jsonObj2 = json2.as<JsonObject>();
                                                      if (jsonObj2["on"])
                                                      {
                                                        // Serial.println("Down button pressed.");
                                                        // Serial.println("------------------");
                                                        // ws_cmd = true;
                                                        // ws_cmd_value = toFloor1;
                                                      }
                                                      request2->send(200, "OK"); }));

  // Button #11
  // server.addHandler(new AsyncCallbackJsonWebHandler("/button11pressed", [](AsyncWebServerRequest *request2, JsonVariant &json2)
  //                                                   {
  //   const JsonObject &jsonObj2 = json2.as<JsonObject>();
  //   if (jsonObj2["on"])
  //   {
  //     Serial.println("Button 11 pressed. Running DPV sweep.");
  //     // digitalWrite(LEDPIN, HIGH);

  //   }
  //   request2->send(200, "OK"); }));
  // Button #3
  server.addHandler(new AsyncCallbackJsonWebHandler("/on_Button_STOP_pressed", [](AsyncWebServerRequest *request3, JsonVariant &json3)
                                                    {
                                                      const JsonObject &jsonObj3 = json3.as<JsonObject>();
                                                      if (jsonObj3["on"])
                                                      {
                                                        // Serial.println("stop button pressed. Stopping all movement!");
                                                        // Serial.println("------------------");
                                                        // ws_cmd = true;
                                                        // ws_cmd_value = STOP;
                                                      }
                                                      request3->send(200, "OK"); }));

  // Button #4
  server.addHandler(new AsyncCallbackJsonWebHandler("/on_Button_EMERGENCY_pressed", [](AsyncWebServerRequest *request4, JsonVariant &json4)
                                                    {
                                                      const JsonObject &jsonObj4 = json4.as<JsonObject>();
                                                      if (jsonObj4["on"])
                                                      {
                                                        // Serial.println("Emergency button pressed. Stopping all movement immediately!");
                                                        // Serial.println("------------------");
                                                        // ws_cmd = true;
                                                        // ws_cmd_value = POWER_CUT;
                                                      }
                                                      request4->send(200, "OK"); }));

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // server.on("/downloadfile", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { request->send(SPIFFS, "/data.txt", "text/plain", true); });

  server.on("/rebootnanostat", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              // reboot the ESP32
              request->send(200, "text/HTML", "  <head> <meta http-equiv=\"refresh\" content=\"5; URL=index.html\" /> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> </head> <body> <h1> Rebooting! </h1>  </body>");
              delay(500);
              ESP.restart(); });

  server.onNotFound([](AsyncWebServerRequest *request)
                    {
                      if (request->method() == HTTP_OPTIONS)
                      {
                        request->send(200); // options request typically sent by client at beginning to make sure server can handle request
                      }
                      else
                      {
                        Serial.println("Not found");
                        request->send(404, "Not found");
                      } });

  // Send a POST request to <IP>/actionpage with a form field message set to <message>
  server.on("/actionpage.html", HTTP_POST, [](AsyncWebServerRequest *request)
            {
              String message;
              Serial.println("actionpage.html, HTTP_POST actionpage received , processing....");
              Serial.printf("Total Parameters Received: %d\n", request->params());
              //**********************************************
              preferences.begin("my-config", false);
              // List all parameters int params = request->params();
              int params = request->params();
              for (int i = 0; i < params; i++)
              {
                AsyncWebParameter *p = request->getParam(i);
                if (p->isPost())
                {
                  // Serial.print(i);
                  // Serial.print(F("\t"));
                  // Serial.print(p->name().c_str());
                  // Serial.print(F("\t"));
                  // Serial.println(p->value().c_str());
                  String paramName = p->name();
                  String paramValue = p->value();

                  Serial.print("Param: ");
                  Serial.print(paramName);
                  Serial.print(" = ");
                  Serial.println(paramValue);

                  if (paramName == "floor_param")
                  {
                    SET_StopPoints = paramValue.toInt();
                    preferences.putInt("stop_pt", SET_StopPoints); // บันทึก int
                  }
                  else if (paramName == "up_param")
                  {
                    SET_UpDuration = paramValue.toFloat();
                    preferences.putFloat("up_dur", SET_UpDuration); // บันทึก float
                  }
                  else if (paramName == "down_param")
                  {
                    SET_DownDuration = paramValue.toFloat();
                    preferences.putFloat("down_dur", SET_DownDuration);
                  }
                  else if (paramName == "alice_param")
                  {
                    SET_Param4 = paramValue.toInt();
                    preferences.putInt("param4", SET_Param4);
                  }
                  else if (paramName == "bob_param")
                  {
                    SET_Param5 = paramValue.toInt();
                    preferences.putInt("param5", SET_Param5);
                  }
                }
              }

              preferences.end();
              Serial.println("--- Updated Variables ---");
              Serial.printf("StopPoints: %d\n", SET_StopPoints);
              Serial.printf("UpDur: %.2f, DownDur: %.2f\n", SET_UpDuration, SET_DownDuration);
              //**********************************************

              if (request->hasParam(PARAM_MESSAGE, true))
              {
                message = request->getParam(PARAM_MESSAGE, true)->value();
                Serial.println(message);
              }
              else
              {
                message = "No message sent";
              }
              // request->send(200, "text/HTML", "Hello, POST: " + message);
              // request->send(200, "text/HTML", "Sweep data saved. Click <a href=\"/index.html\">here</a> to return to main page.");
              request->send(200, "text/HTML", "  <head> <meta http-equiv=\"refresh\" content=\"2; URL=index.html\" /> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> </head> <body> <h1> Settings saved! </h1> <p> Returning to main page. </p> </body>");
              // request->send(200, "OK");
            });

  // Wifitools stuff:
  // Save credentials:
  server.on("/saveSecret", HTTP_POST, [](AsyncWebServerRequest *request)
            { handleGetSavSecreteJsonNoReboot(request); });

  // Wifi scan:
  server.on("/wifiScan.json", HTTP_GET, [](AsyncWebServerRequest *request)
            { getWifiScanJson(request); });

  server.begin();
}

void setup()
{

  // Start serial interface:
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("Welcome to Ximplex_KIT");

  delay(50);
  delay(50);

  // ############################### SPIFFS STARTUP #######################################
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  bool m_autoconnected_attempt_succeeded = false;
  m_autoconnected_attempt_succeeded = connectAttempt("", ""); // uses SSID/PWD stored in ESP32 secret memory.....
  // Serial.print("m_autoconnected_attempt_succeeded = ");
  // Serial.println(m_autoconnected_attempt_succeeded);
  if (!m_autoconnected_attempt_succeeded)
  {
    // try SSID/PWD from file...
    Serial.println("Failed to connect.");
    String m_filenametopass = "/credentials.JSON";
    m_autoconnected_attempt_succeeded = readSSIDPWDfile(m_filenametopass);
  }
  if (!m_autoconnected_attempt_succeeded)
  {
    // start AP server
    // Serial.println("connect failed, starting AP server");
    setUpAPService();
    runWifiPortal();

    // MDNS.begin("nanostat"); // see https://randomnerdtutorials.com/esp32-access-point-ap-web-server/
  }

  MDNS.begin("ximplex_kit");

  // ############################# WEBSERVER & WIFI #####################################

  server.reset(); // try putting this in setup
  configureserver();

  m_websocketserver.begin();
  m_websocketserver.onEvent(onWebSocketEvent); // Start WebSocket server and assign callback

  Serial1.begin(38400, SERIAL_8E1, PIN_RX, PIN_TX);
  node.begin(PLC_slaveID, Serial1);

  // wifiClient.setInsecure();
  Serial.println(WiFi.localIP());
  setupMQTT();


  // for (int i = 0; i < subTopicNum; i++) {
  //   sub_buff[i].topic = "";
  //   sub_buff[i].payload = "";
  // }

  preferences.begin("my-config", false); // read only
  SET_StopPoints = preferences.getInt("stop_pt", 2);
  SET_UpDuration = preferences.getFloat("up_dur", 18.5);
  SET_DownDuration = preferences.getFloat("down_dur", 18.0);
  SET_Param4 = preferences.getInt("param4", 0);
  SET_Param5 = preferences.getInt("param5", 0);
  preferences.end();

  Serial.println("--- Loaded Settings ---");
  Serial.printf("StopPoints: %d\n", SET_StopPoints);
  Serial.printf("Up: %.2f, Down: %.2f\n", SET_UpDuration, SET_DownDuration);

  snprintf(
      x_mqtt_topic,
      sizeof(x_mqtt_topic),
      "%s%s%s%s",
      KIT_topic,
      UT_case,
      system_status,
      X_status);

  snprintf(
      y_mqtt_topic,
      sizeof(y_mqtt_topic),
      "%s%s%s%s",
      KIT_topic,
      UT_case,
      system_status,
      Y_status);

  pinMode(WIFI_READY, OUTPUT);
  mqttMutex = xSemaphoreCreateMutex();
  hasChangedMutex = xSemaphoreCreateMutex();
  xTaskCreate(vPollingTask, "PollingTask", 4096, NULL, 3, &pollingTaskHandle);
  xTaskCreate(vReconnectTask, "ReconnectTask", 4096, NULL, 3, NULL);
  xTaskCreate(vPublishTask, "PublishTask", 4096, NULL, 3, &publishTaskHandle);

  Serial.print("Heap free memory (in bytes)= ");
  Serial.println(ESP.getFreeHeap());
  Serial.println(F("Setup complete."));
}

void loop()
{

  m_websocketserver.loop();

}
