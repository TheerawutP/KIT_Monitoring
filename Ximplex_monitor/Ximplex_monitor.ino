#include <WiFi.h>
#include <PubSubClient.h>
// #include <WiFiClient.h>

//modbusRTU libs
#include <ModbusRTU.h>

ModbusRTU RTU_SLAVE;
#define PIN_RX2 16
#define PIN_TX2 17
#define PIN_EN 4
#define SLAVE_ID 125

//credential
const char* ssid = "Flinkone 1-2.4G";
const char* password = "ff112335";
const char* mqtt_broker = "kit.flinkone.com";
const int mqtt_port = 1883;  //unencrypt
// const char* mqtt_username = "Flink1";
// const char* mqtt_password = "Fff11111";

char* X_status = "kit/ut_0001/X_status";

uint16_t m_data[5];
uint8_t publish_payload[2];
String lastTopic = "";
String lastPayload = "";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setupMQTT() {
  mqttClient.setServer(mqtt_broker, mqtt_port);
  mqttClient.setCallback(callback);
}

void endian(uint16_t data, uint8_t* payload) {
  payload[0] = (data >> 8) & 0xFF;  // High byte
  payload[1] = data & 0xFF;         // Low byte
}


void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];
  lastTopic = topic;
  lastPayload = message;
}

void publishMqtt(String msg, char* topic) {
  Serial.print("......publishing.....");
  Serial.println(msg);
  int debug = mqttClient.publish(topic, msg.c_str(), true);  //(topic, payload, [length], [retained])
  if (debug) {
    Serial.println("Publish Success");
  } else {
    // Clear All Bypass and Command
    Serial.println("Publish Fail");
  }
}

void wifiConnect() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int counter = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (counter == 10) {
      ESP.restart();
    }
    counter += 1;
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //end Wifi connect
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    //String clientId = "ESP32Client-0001";
    // if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
    if (mqttClient.connect("esp32")) {
      Serial.println("connected");
      // Serial.print("Client ID: ");
      // Serial.println(clientId);
      mqttClient.subscribe(X_status);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

uint16_t cbWrite(TRegister* reg, uint16_t val) {
  // if (lastSVal != val) {
  //   lastSVal = val;
  //   Serial.println(String("HregSet val (from STM32):") + String(val));
  // }
  reg->value = val;
  return val;
}

uint16_t cbRead(TRegister* reg, uint16_t val) {
  reg->value = val;
  return val;
}

void setup() {
  Serial.begin(115200);
  delay(10);
  wifiConnect();

  Serial1.begin(38400, SERIAL_8E1, 16, 17);
  RTU_SLAVE.begin(&Serial1);
  RTU_SLAVE.slave(2);  // Slave ID 1

  //for written by main controller's data
  RTU_SLAVE.addHreg(0x0000, 0);
  RTU_SLAVE.onSetHreg(0x0000, cbWrite);
  RTU_SLAVE.onGetHreg(0x0000, cbRead);
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(1000);
  //   Serial.println("Connecting to WiFi..");
  // }

  // esp_reset_reason_t reason = esp_reset_reason();
  // Serial.printf("[BOOT] Reset reason: %d\n", reason);
  // Hreg_setting_call(RTU_SLAVE, SLAVE_ID, PIN_EN, PIN_RX2, PIN_TX2, H_LIM);
  // AutoReset_init(1440);

  // wifiClient.setInsecure();
  Serial.println(WiFi.localIP());
  setupMQTT();
}

void loop() {
  String publish_payload = "0x05,0x06,0x07,0x08";
  RTU_SLAVE.task();
  if (!mqttClient.connected()) {
    reconnect();
  }


  // RTU_SLAVE.Hreg(1, 20);
  // m_data[0] = RTU_SLAVE.Hreg(1);
  // m_data[1] = RTU_SLAVE.Hreg(2);
  // m_data[2] = RTU_SLAVE.Hreg(3);
  // m_data[3] = RTU_SLAVE.Hreg(4);
  // m_data[4] = RTU_SLAVE.Hreg(5);

  //Hreg[0] used for store alive bit

  // if (lastTopic == "esp32/return_data_1") {
  //   Serial.print("Responding to ");
  //   Serial.println(lastTopic);

  //   endian(m_data[0], publish_payload);
  //   Serial.print("HIGH BITS: ");
  //   Serial.println(publish_payload[0]);
  //   Serial.print("LOW BITS: ");
  //   Serial.println(publish_payload[1]);

  // if (mqttClient.publish("esp32/return_data_1", publish_payload, 2, false)) {
  //   Serial.println("Publish SUCCESS");
  // } else {
  //   Serial.println("Publish FAILED");
  // }
  publishMqtt(publish_payload, X_status);


  // if (lastTopic == "esp32/return_data_2") {
  //   Serial.print("Responding to ");
  //   Serial.println(lastTopic);

  //   endian(m_data[0], publish_payload);
  //   Serial.print("HIGH BITS: ");
  //   Serial.println(publish_payload[0]);
  //   Serial.print("LOW BITS: ");
  //   Serial.println(publish_payload[1]);

  //   if (mqttClient.publish("esp32/return_data_2", publish_payload, 2, false)) {
  //     Serial.println("Publish SUCCESS");
  //   } else {
  //     Serial.println("Publish FAILED");
  //   }
  // }

  // if (lastTopic == "esp32/alive") {
  //   Serial.print("Responding to ");
  //   Serial.println(lastTopic);
  //   uint16_t value = lastPayload.toInt();  // string to uint16_t
  //   RTU_SLAVE.Hreg(0, value);
  // }
  // lastTopic = "";
  // lastPayload = "";
  mqttClient.loop();
  delay(2000);
}
