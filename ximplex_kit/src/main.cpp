
// Libraries
#include <WiFi.h>
#include "Arduino.h"
#include <memory>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <AsyncJson.h>
#include "wifi_credentials.h"
#include "DNSServer.h"
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include <ArduinoJson.h>

#include <WiFiClientSecure.h>
// #include <HTTPUpdate.h>
#include <Preferences.h>

#include "communication/ICommunicationService.h"
#include "communication/FirebaseCommunicationService.h"
#include "FirebaseConfig.h"

#include "app/AppTypes.h"
#include "app/DefaultTopics.h"
#include "app/HardwareConfig.h"

// const char *firmwareURL = "https://raw.githubusercontent.com/TheerawutP/test_OTA/main/MCU.ino.bin";
// bool shouldUpdateFirmware = false;
TaskHandle_t pollingTaskHandle = NULL;
TaskHandle_t publishTaskHandle = NULL;

ModbusMaster node;

uint16_t hreg[8][16];
uint16_t servo_data[8][16];

// credential
const char *ELEVATOR_ID = "E10"; // Hardcoded for now
// const char *ssid = "Flinkone 1-2.4G";
// const char *password = "ff112335";
const char *ssid = "";
const char *password = "";
// const char *mqtt_broker = "kit.flinkone.com";
// const int mqtt_port = 1883; // unencrypt
const char *mqtt_broker = "158.101.156.71";
const int mqtt_port = 1883; // unencrypt

// topics
// publish topics
char X_pTopic[128] = "";
char Y_pTopic[128] = "";
char all_status_pTopic[128] = "";
char cmd_sTopic[128] = "";
char ack_pTopic[128] = "";

// subs topics
const char *listenToAll_sTopic = DEFAULT_LISTEN_ALL_STOPIC;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

SemaphoreHandle_t mqttMutex;     // Mutex to protect MQTT client
SemaphoreHandle_t firebaseMutex; // Mutex to protect Firebase access
SemaphoreHandle_t hasChangedMutex;
SemaphoreHandle_t modbusMutex; // Mutex to protect Modbus communication

TaskHandle_t firebasePublishTaskHandle = NULL;

read_state curr_slave = PLC;
read_state last_slave = PLC;
door_state currentState = DOOR_NULL;

char X_status_payload[64];
char Y_status_payload[64];

char X_prev[64] = "";
char Y_prev[64] = "";

char alarm_pTopic[128] = "";
uint16_t prev_servo_alarm = 0xFFFF;
char alarm_status_payload[16] = "AL000";
bool alarmPublishPending = false;

bool XY_hasChanged = true;
bool mqttPublishPending = true;
bool firebasePublishPending = true;
uint32_t ChangeSlaveInterval = 500;

bool buildStatePayload(char *buffer, size_t bufferSize);  
bool buildAlarmPayload(char *buffer, size_t bufferSize);
void setPublishPending();

AsyncWebServer server(80); //

std::unique_ptr<ICommunicationService> g_comm;

// WifiTool object
int WAIT_FOR_WIFI_TIME_OUT = 6000;
const char *PARAM_MESSAGE = "message"; // message server receives from client
std::unique_ptr<DNSServer> dnsServer;
std::unique_ptr<AsyncWebServer> m_wifitools_server;
const byte DNS_PORT = 53;
bool restartSystem = false;
String temp_json_string = "";

Preferences preferences;

void handleChangeTopic(byte *payload, unsigned int length)
{

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error)
  {
    Serial.print("JSON Parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char *target = doc["topic"];
  const char *newPath = doc["change_to"];

  if (!target || !newPath)
  {
    Serial.println("JSON format invalid: missing 'topic' or 'change_to'");
    return;
  }

  preferences.begin("my-app", false);

  if (strcmp(target, "X_pTopic") == 0)
  {
    strncpy(X_pTopic, newPath, sizeof(X_pTopic) - 1);
    X_pTopic[sizeof(X_pTopic) - 1] = '\0';
    preferences.putString("x_stat_top", newPath);
    Serial.println("X_pTopic updated!");
  }
  else if (strcmp(target, "Y_pTopic") == 0)
  {
    strncpy(Y_pTopic, newPath, sizeof(Y_pTopic) - 1);
    Y_pTopic[sizeof(Y_pTopic) - 1] = '\0';
    preferences.putString("y_stat_top", newPath);
    Serial.println("Y_pTopic updated!");
  }
  else
  {
    Serial.printf("Target topic '%s' not found.\n", target);
  }

  preferences.end();
}

void publishMqtt(const char *topic, const char *msg);

void sendAck(const char *msgId, bool ok, const char *error = NULL)
{
  StaticJsonDocument<256> doc;
  doc["msgId"] = msgId;
  doc["ts"] = millis();
  doc["ok"] = ok;
  if (error)
    doc["error"] = error;

  char buffer[256];
  serializeJson(doc, buffer);
  publishMqtt(ack_pTopic, buffer);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.printf("Message arrived [%s] Content: ", topic);
  for (int i = 0; i < length; i++)
    Serial.print((char)payload[i]);
  Serial.println();

  // Handle Secure Bridge Commands
  if (strcmp(topic, cmd_sTopic) == 0)
  {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
      Serial.println("JSON Parse failed");
      return;
    }

    const char *msgId = doc["msgId"] | "";
    const char *type = doc["type"] | "";
    const char *userEmail = doc["issuedBy"]["email"] | "unknown";

    Serial.printf("🚀 Command Received: %s (msgId: %s) by %s\n", type, msgId, userEmail);

    bool success = false;
    int targetBit = -1;
    const uint16_t controlAddr = 20;

    if (strcmp(type, "goToFloor") == 0)
    {
      int floor = doc["payload"]["floor"] | -1;
      Serial.printf("Action: Moving to floor %d\n", floor);
      if (floor == 1)
        targetBit = 0;
      else if (floor == 2)
        targetBit = 1;
      else if (floor == 3)
        targetBit = 2;
      else if (floor == 4)
        targetBit = 3;
    }
    else if (strcmp(type, "openDoor") == 0)
    {
      Serial.println("Action: Opening Door");
      targetBit = 11;
    }
    else if (strcmp(type, "closeDoor") == 0)
    {
      Serial.println("Action: Closing Door");
      targetBit = 12;
    }
    else if (strcmp(type, "holdDoor") == 0)
    {
      Serial.println("Action: Holding Door");
      targetBit = 10;
    }

    if (targetBit != -1)
    {
      // Pulse logic: Set bit HIGH, wait 200ms, set bit LOW on address 20
      if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(500)) == pdTRUE)
      {
        node.writeSingleRegister(controlAddr, (1 << targetBit));
        Serial.printf("Set bit %d HIGH at control address %d\n", targetBit, controlAddr);
        xSemaphoreGive(modbusMutex);

        vTaskDelay(pdMS_TO_TICKS(1000));

        if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(500)) == pdTRUE)
        {
          node.writeSingleRegister(controlAddr, 0);
          Serial.printf("Set control address %d LOW\n", controlAddr);
          xSemaphoreGive(modbusMutex);
          success = true;
        }
      }
    }

    sendAck(msgId, success);
    return;
  }

  // Legacy command handling (Fallback)
  if (strcmp(topic, "kit/UT_25061/changeTopic") == 0)
  {
    handleChangeTopic(payload, length);
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
  char X_temp[64];
  char Y_temp[64];

  if (strcmp(from, "PLC") == 0)
  {
    // PLC Octal Mapping:
    // data[0]: X0-X7 (low byte), X10-X17 (high byte)
    // data[1]: X20-X27, X30-X37
    // data[2]: X40-X47, X50-X57
    // data[3]: X60-X67, X70-X77

    // We format these as a single hex string for efficient transmission
    snprintf(X_temp, sizeof(X_temp), "%04X%04X%04X%04X",
             data[0], data[1], data[2], data[3]);

    // data[4-7]: Y0-Y77 mapping same as above
    snprintf(Y_temp, sizeof(Y_temp), "%04X%04X%04X%04X",
             data[4], data[5], data[6], data[7]);

    if (strcmp(X_temp, X_prev) != 0 || strcmp(Y_temp, Y_prev) != 0)
    {
      *flag = true;
      setPublishPending();

      strncpy(X_prev, X_temp, sizeof(X_prev));
      strncpy(Y_prev, Y_temp, sizeof(Y_prev));

      strncpy(X_status_payload, X_temp, sizeof(X_status_payload));
      strncpy(Y_status_payload, Y_temp, sizeof(Y_status_payload));

      Serial.println("Change Detected!");
      Serial.print("X Mask: ");
      Serial.println(X_status_payload);
      Serial.print("Y Mask: ");
      Serial.println(Y_status_payload);
    }
  }
}

void updateTopicFromPrefs(const char *key, char *buffer, size_t bufferSize)
{
  String temp = preferences.getString(key, buffer);

  strncpy(buffer, temp.c_str(), bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
}

void publishMqtt(const char *topic, const char *msg)
{
  if (xSemaphoreTakeRecursive(mqttMutex, portMAX_DELAY) == pdTRUE)
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
    xSemaphoreGiveRecursive(mqttMutex);
  }
}

bool buildStatePayload(char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0) return false;

  StaticJsonDocument<256> doc;
  doc["x"] = X_status_payload;
  doc["y"] = Y_status_payload;
  // doc["sensors"]["weight"] = current_weight;   //etra_spec_reading add here

  size_t len = serializeJson(doc, buffer, bufferSize);
  return len > 0;
}

bool buildAlarmPayload(char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0) return false;

  StaticJsonDocument<128> doc;
  doc["alarm"] = alarm_status_payload; 
  
  size_t len = serializeJson(doc, buffer, bufferSize);
  return len > 0;
}

void setPublishPending()
{
  mqttPublishPending = true;
  firebasePublishPending = true;
}

void vReconnectTask(void *pvParams)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      // Protect Check/Connect with Mutex
      if (xSemaphoreTakeRecursive(mqttMutex, portMAX_DELAY) == pdTRUE)
      {
        if (!mqttClient.connected())
        {
          Serial.print("MQTT connecting...");

          String clientId = "ESP32-";
          clientId += String(random(0xffff), HEX); // Unique ID

          if (mqttClient.connect(clientId.c_str()))
          {
            Serial.println("connected");
            mqttClient.subscribe(listenToAll_sTopic);
            mqttClient.subscribe(cmd_sTopic); // Subscribe to secure bridge commands
          }
          else
          {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
          }
        }

        if (mqttClient.connected())
        {
          mqttClient.loop();
        }

        xSemaphoreGiveRecursive(mqttMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool readDataFrom(uint8_t slaveID, uint16_t startAddress, uint8_t numRead, uint16_t *hreg_row)
{
  node.begin(slaveID, Serial1);
  uint8_t result = node.readHoldingRegisters(startAddress, numRead);

  if (result == node.ku8MBSuccess)
  {
    for (int i = 0; i < numRead; i++)
    {
      hreg_row[i] = node.getResponseBuffer(i);
    }
    return true;
  }
  else
  {
    Serial.print("Error at ID ");
    Serial.print(slaveID);
    Serial.print(": ");
    Serial.println(result, HEX);
    return false;
  }
}

void vPollingTask(void *pvParams)
{
  const uint16_t SERVO_ALARM_ADDR = 0x0002;

  for (;;)
  {
    if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(200)) == pdTRUE)
    {
      switch (curr_slave)
      {
      case PLC:
        if (readDataFrom(PLC_slaveID, X0_ADD, 8, hreg[PLC_slaveID]))
        {
          if (xSemaphoreTake(hasChangedMutex, pdMS_TO_TICKS(50)) == pdTRUE)
          {
            isChange("PLC", hreg[PLC_slaveID], &XY_hasChanged);
            xSemaphoreGive(hasChangedMutex);
          }
        }
        else
        {
          Serial.println("Failed to read from PLC");
        }

        last_slave = PLC;
        curr_slave = SERVO;
        break;

      case SERVO:
        if (readDataFrom(SERVO_slaveID, SERVO_ALARM_ADDR, 1, servo_data[SERVO_slaveID]))
        {
          uint16_t rawAlarm = servo_data[SERVO_slaveID][0];

          if (rawAlarm != prev_servo_alarm)
          {

            prev_servo_alarm = rawAlarm;
            snprintf(alarm_status_payload, sizeof(alarm_status_payload), "AL%03X", rawAlarm);
            Serial.printf("Servo Alarm Changed: %s\n", alarm_status_payload);
            if (xSemaphoreTake(hasChangedMutex, pdMS_TO_TICKS(50)) == pdTRUE)
            {
              alarmPublishPending = true;
              xSemaphoreGive(hasChangedMutex);
            }
          }
        }
        else
        {
          Serial.println("Failed to read from SERVO");
        }

        last_slave = SERVO;
        curr_slave = PLC;
        break;
      }
      xSemaphoreGive(modbusMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(ChangeSlaveInterval));
  }
}

void vPublishTask(void *pvParams)
{
  for (;;)
  {
    bool shouldPublishState = false;
    bool shouldPublishAlarm = false;

    if (xSemaphoreTake(hasChangedMutex, portMAX_DELAY) == pdTRUE)
    {
      if (mqttPublishPending)
      {
        shouldPublishState = true;
        mqttPublishPending = false;
      }
      if (alarmPublishPending)
      {
        shouldPublishAlarm = true;
        alarmPublishPending = false;
      }
      xSemaphoreGive(hasChangedMutex);
    }

    if (shouldPublishState)
    {
      char statePayload[256];
      if (buildStatePayload(statePayload, sizeof(statePayload)))
      {
        publishMqtt(all_status_pTopic, statePayload);
        Serial.print("Published State to MQTT: ");
        Serial.println(statePayload);
      }
    }

    if (shouldPublishAlarm)
    {
      char alarmPayload[128];
      if (buildAlarmPayload(alarmPayload, sizeof(alarmPayload)))
      {
        publishMqtt(alarm_pTopic, alarmPayload);
        Serial.print("Published Alarm to MQTT: ");
        Serial.println(alarmPayload);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void vFirebasePublishTask(void *pvParams)
{
  for (;;)
  {
    bool shouldPublish = false;

    if (xSemaphoreTake(hasChangedMutex, portMAX_DELAY) == pdTRUE)
    {
      if (firebasePublishPending)
      {
        shouldPublish = true;
        firebasePublishPending = false;
      }
      xSemaphoreGive(hasChangedMutex);
    }

    if (shouldPublish && g_comm && WiFi.status() == WL_CONNECTED)
    {
      char statePayload[256];
      if (buildStatePayload(statePayload, sizeof(statePayload)))
      {
        if (xSemaphoreTake(firebaseMutex, portMAX_DELAY) == pdTRUE)
        {
          // g_comm->sendStatus(statePayload);
          g_comm->pushHistory("status", statePayload);
          xSemaphoreGive(firebaseMutex);
        }

        Serial.print("Published State to Firebase: ");
        Serial.println(statePayload);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
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
  String m_PWD2_name = m_JSONdoc_from_pwd_file["PWD2"];
  String m_PWD3_name = m_JSONdoc_from_pwd_file["PWD3"];

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

  dnsServer.reset(new DNSServer());
  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Ximplex_KIT");
  delay(500);

  (*dnsServer).setErrorReplyCode(DNSReplyCode::NoError);
  (*dnsServer).start(DNS_PORT, "*", apIP);

  Serial.println("dns server config done");
}

void process()
{
  /// DNS
  if (dnsServer)
  {
    dnsServer->processNextRequest();
  }
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
      json += "\"RSSI\":";
      json += String(WiFi.RSSI(i));
      json += ",\"SSID\":\"";
      json += WiFi.SSID(i);
      json += "\"";
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

  m_wifitools_server->serveStatic("/", SPIFFS, "/").setDefaultFile("wifi_index.html");
  // m_wifitools_server->serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // m_wifitools_server->on("/saveSecret/", HTTP_ANY, [&, this](AsyncWebServerRequest *request) {
  //   handleGetSavSecreteJson(request);
  // });

  m_wifitools_server->on("/saveSecret", HTTP_POST, [](AsyncWebServerRequest *request)
                         { handleGetSavSecreteJson(request); });

  // m_wifitools_server->on("/list", HTTP_ANY, [](AsyncWebServerRequest *request)
  //                        { handleFileList(request); });

  m_wifitools_server->on("/wifiScan.json", HTTP_GET, [](AsyncWebServerRequest *request)
                         { getWifiScanJson(request); });

  m_wifitools_server->onNotFound([](AsyncWebServerRequest *request)
                                 { request->redirect("/"); });

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
                  String paramName = p->name();
                  String paramValue = p->value();

                  Serial.print("Param: ");
                  Serial.print(paramName);
                  Serial.print(" = ");
                  Serial.println(paramValue);

                  // logic for updating variables based on parameter names here
                }
              }

              preferences.end();
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

  Serial.println("Welcome to Ximqtt");

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
    setUpAPService();
    runWifiPortal();
  }

  MDNS.begin("ximplex_kit");

  // ############################# WEBSERVER & WIFI #####################################

  server.reset(); // try putting this in setup
  configureserver();

  // Generate unique ID based on MAC address
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String dynamicId = "ESP32_";
  dynamicId += mac;

  String dynamicDevicePath = "/devices/";
  dynamicDevicePath += dynamicId;
  Serial.print("Dynamic Firebase Device Path: ");
  Serial.println(dynamicDevicePath);

  g_comm.reset(new FirebaseCommunicationService(
      FIREBASE_API_KEY,
      FIREBASE_DATABASE_URL,
      FIREBASE_USER_EMAIL,
      FIREBASE_USER_PASSWORD,
      dynamicDevicePath.c_str(),
      FIREBASE_STATUS_UPDATE_INTERVAL));

  // Set up command callback for the multi-tenant handshake
  g_comm->setCommandCallback([](const char *id, const char *cmd, const char *data)
                             {
    Serial.printf("Received Command: %s (ID: %s, Data: %s)\n", cmd, id, data);
    
    bool success = false;
    int targetBit = -1;
    const uint16_t controlAddr = 20;

    if (strcmp(cmd, "goToFloor") == 0) {
      // Data might be a simple number string or JSON "{\"floor\": n}"
      int floor = atoi(data);
      if (floor == 0 && data[0] == '{') {
         StaticJsonDocument<128> doc;
         if (deserializeJson(doc, data) == DeserializationError::Ok) {
           floor = doc["payload"]["floor"] | -1;
         }
      }
      Serial.printf("Action: Moving to floor %d\n", floor);
      if (floor == 1) targetBit = 0;
      else if (floor == 2) targetBit = 1;
      else if (floor == 3) targetBit = 2;
      else if (floor == 4) targetBit = 4;
    }
    else if (strcmp(cmd, "openDoor") == 0) {
      targetBit = 11;
    }
    else if (strcmp(cmd, "closeDoor") == 0) {
      targetBit = 12;
    }
    else if (strcmp(cmd, "holdDoor") == 0) {
      targetBit = 10;
    }


    if (targetBit != -1) {
      // Pulse logic: Set bit HIGH, wait 200ms, set bit LOW on address 20
      if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        node.writeSingleRegister(controlAddr, (1 << targetBit));
        xSemaphoreGive(modbusMutex);
        
        delay(200);
        
        if (xSemaphoreTake(modbusMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
          node.writeSingleRegister(controlAddr, 0);
          xSemaphoreGive(modbusMutex);
          success = true;
        }
      }
    }

    if (success) {
      g_comm->acknowledgeCommand(id, "SUCCESS");
    } else {
      g_comm->acknowledgeCommand(id, "UNKNOWN_COMMAND_OR_FAILED");
    } });

  g_comm->begin();

  Serial1.begin(38400, SERIAL_8E1, PIN_RX, PIN_TX);
  node.begin(PLC_slaveID, Serial1);

  // wifiClient.setInsecure();
  Serial.println(WiFi.localIP());
  setupMQTT();

  // Initialize secure topics based on the dynamic ID (MAC-based)
  // This MUST match the ID used in the Bridge and UI
  // snprintf(cmd_sTopic, sizeof(cmd_sTopic), "elevator/%s/cmd", dynamicId.c_str());
  // snprintf(ack_pTopic, sizeof(ack_pTopic), "elevator/%s/ack", dynamicId.c_str());
  // snprintf(all_status_pTopic, sizeof(all_status_pTopic), "elevator/%s/state", dynamicId.c_str());
  snprintf(cmd_sTopic, sizeof(cmd_sTopic), "elevator/%s/cmd", ELEVATOR_ID);
  snprintf(ack_pTopic, sizeof(ack_pTopic), "elevator/%s/ack", ELEVATOR_ID);
  snprintf(all_status_pTopic, sizeof(all_status_pTopic), "elevator/%s/state", ELEVATOR_ID);
  snprintf(alarm_pTopic, sizeof(alarm_pTopic), "elevator/%s/alarm", ELEVATOR_ID);

  // Legacy topics (Optional fallback, but prioritizing new ones)
  strncpy(X_pTopic, DEFAULT_X_PTOPIC, sizeof(X_pTopic) - 1);
  X_pTopic[sizeof(X_pTopic) - 1] = '\0';
  strncpy(Y_pTopic, DEFAULT_Y_PTOPIC, sizeof(Y_pTopic) - 1);
  Y_pTopic[sizeof(Y_pTopic) - 1] = '\0';

  pinMode(WIFI_READY, OUTPUT);
  mqttMutex = xSemaphoreCreateRecursiveMutex();
  firebaseMutex = xSemaphoreCreateMutex();
  hasChangedMutex = xSemaphoreCreateMutex();
  modbusMutex = xSemaphoreCreateMutex();
  xTaskCreate(vPollingTask, "PollingTask", 4096, NULL, 4, &pollingTaskHandle);
  xTaskCreate(vReconnectTask, "ReconnectTask", 4096, NULL, 3, NULL);
  xTaskCreate(vPublishTask, "PublishTask", 4096, NULL, 3, &publishTaskHandle);
  // xTaskCreate(vFirebasePublishTask, "FirebasePublishTask", 9192, NULL, 3, &firebasePublishTaskHandle);

  Serial.print("Heap free memory (in bytes)= ");
  Serial.println(ESP.getFreeHeap());
  Serial.println(F("Setup complete."));
}

void loop()
{

  if (g_comm)
  {
    if (xSemaphoreTake(firebaseMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
      g_comm->loop();
      xSemaphoreGive(firebaseMutex);
    }
  }
  vTaskDelay(pdMS_TO_TICKS(1));
}
