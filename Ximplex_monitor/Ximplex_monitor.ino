#include <WiFi.h>
#include <PubSubClient.h>
//modbusRTU libs
#include <ModbusMaster.h>

ModbusMaster node;

#define PIN_RX 16
#define PIN_TX 17

#define slaveNum 2
#define PLC_slaveID 1
#define SERVO_slaveID 2
#define subTopicNum 10
#define X0_ADD 0
#define Y0_ADD 4

#define X_size 8  // 8*8 input
#define Y_size 8  // 8*8 output

uint16_t hreg[8][16];

//credential
const char* ssid = "Flinkone 1-2.4G";
const char* password = "ff112335";
const char* mqtt_broker = "kit.flinkone.com";
const int mqtt_port = 1883;  //unencrypt

//topics
char* KIT_topic = "kit";
char* UT_case = "/UT_0002";
// char* system_status = "/sys_v2";
char* X_status = "/sys_v1/X_status";
char* Y_status = "/sys_v1/Y_status";
char* M_status = "/sys_v1/M_status";
char* SERVO_status = "/servo/status";
char* SERVO_ALM = "/servo/alarm";
// char* SERVO_toque =
// char* SERVO_speed =

typedef struct {
  char topic[64];
  char payload[128];
} msg;
msg sub_buff[subTopicNum];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

SemaphoreHandle_t hregMutex;
SemaphoreHandle_t mqttMutex;  // Mutex to protect MQTT client
QueueHandle_t pubQueue = NULL;

enum read_state {
  PLC,
  SERVO_STATUS,
  SERVO_ALARM
};

read_state curr_slave = PLC;
read_state last_slave = PLC;

String X_last_incoming;
String Y_last_incoming;
String X_publish_buff;
String Y_publish_buff;

uint32_t ChangeSlaveInterval = 50;

void setupMQTT() {
  mqttClient.setServer(mqtt_broker, mqtt_port);
  // mqttClient.setCallback(callback);
}

// void callback(char* topic, byte* payload, unsigned int length) {
//   String message;
//   String topicStr = String(topic);
//   for (int i = 0; i < length; i++) message += (char)payload[i];
//   for (int i = 0; i < subTopicNum; i++) {
//     if (topicStr == sub_msg[i].topic) sub_msg[i].payload = message;
//   }
// }

void wifiConnect() {
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

void publishMqtt(const char* topic, const char* msg) {
  if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {
    if (mqttClient.connected()) {
      bool result = mqttClient.publish(topic, msg, true);
      if (result) {
        Serial.println("publish success");
      } else {
        Serial.println("publish fail");
      }
      // Serial.printf("Pub: %s -> %s\n", topic, msg);
    }
    xSemaphoreGive(mqttMutex);
  }
}


void vReconnectTask(void* pvParams) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      // Protect Check/Connect with Mutex
      if (xSemaphoreTake(mqttMutex, portMAX_DELAY) == pdTRUE) {

        if (!mqttClient.connected()) {
          Serial.print("MQTT connecting...");
          if (mqttClient.connect("esp32_client_id")) {
            Serial.println("connected");
            // Re-subscribe here if needed
          } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
          }
        }

        // IMPORTANT: loop() must be called frequently to maintain connection
        if (mqttClient.connected()) {
          mqttClient.loop();
        }

        xSemaphoreGive(mqttMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms
  }
}


void vPollingTask(void* pvParams) {
  for (;;) {
    uint32_t result;
    if (xSemaphoreTake(hregMutex, portMAX_DELAY) == pdTRUE) {
      switch (curr_slave) {
        case PLC:
          node.begin(PLC_slaveID, Serial1);
          result = node.readHoldingRegisters(X0_ADD, 8);  //start hreg address, num of read
          if (result == node.ku8MBSuccess) {
            hreg[PLC_slaveID][0] = node.getResponseBuffer(0);
            hreg[PLC_slaveID][1] = node.getResponseBuffer(1);
            hreg[PLC_slaveID][2] = node.getResponseBuffer(2);
            hreg[PLC_slaveID][3] = node.getResponseBuffer(3);

            hreg[PLC_slaveID][4] = node.getResponseBuffer(4);
            hreg[PLC_slaveID][5] = node.getResponseBuffer(5);
            hreg[PLC_slaveID][6] = node.getResponseBuffer(6);
            hreg[PLC_slaveID][7] = node.getResponseBuffer(7);
            xSemaphoreGive(hregMutex);
          } else {
            Serial.println(result);  // Check this code for timeouts (226) or invalid data (227)
          }
          last_slave = PLC;
          // curr_slave = SERVO;
          break;
          // case SERVO:
          //   break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// void vPackingTask(void* pvParams) {

//   String X_status_payload;
//   String Y_status_payload;
//   msg X_pub;
//   msg Y_pub;
//   char X_topic[50];
//   char Y_topic[50];

//   for (;;) {

//     for (int i = 0; i < X_size; i++) {
//       X_status_payload = String(hreg[PLC_slaveID][i]);
//       if (i != (X_size - 1)) X_status_payload += ",";
//     }

//     for (int i = 4; i < Y_size; i++) {
//       Y_status_payload = String(hreg[PLC_slaveID][i]);
//       if (i != (Y_size - 1)) Y_status_payload += ",";
//     }

//     snprintf(X_topic, sizeof(X_topic), "kit/%s%s", UT_case, X_status);
//     X_pub.topic = String(X_topic);
//     X_pub.payload = X_status_payload;

//     snprintf(Y_topic, sizeof(Y_topic), "kit/%s%s", UT_case, Y_status);
//     Y_pub.topic = String(Y_topic);
//     Y_pub.payload = Y_status_payload;

//     if (X_last_incoming != X_status_payload && Y_last_incoming != Y_status_payload) {
//       xQueueSend(pubQueue, &X_pub, portMAX_DELAY);
//       xQueueSend(pubQueue, &Y_pub, portMAX_DELAY);
//     } else {
//       if (uxQueueMessagesWaiting(pubQueue) == 0) {
//         xQueueSend(pubQueue, &X_pub, portMAX_DELAY);
//         xQueueSend(pubQueue, &Y_pub, portMAX_DELAY);
//       }
//     }
//     X_last_incoming = X_status_payload;
//     Y_last_incoming = Y_status_payload;
//   }
// }

void vPackingTask(void* pvParams) {
  char X_status_payload[128];
  char Y_status_payload[128];
  msg X_pub;
  msg Y_pub;

  for (;;) {
    if (xSemaphoreTake(hregMutex, portMAX_DELAY) == pdTRUE) {
      // ----- Pack X -----
      X_status_payload[0] = '\0';
      for (int i = 0; i < X_size; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", hreg[PLC_slaveID][i]);
        strcat(X_status_payload, buf);
        if (i != (X_size - 1)) strcat(X_status_payload, ",");
      }

      // ----- Pack Y -----
      Y_status_payload[0] = '\0';
      for (int i = 4; i < Y_size + 4; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", hreg[PLC_slaveID][i]);
        strcat(Y_status_payload, buf);
        if (i != (Y_size - 1)) strcat(Y_status_payload, ",");
      }

      // ----- Topic -----
      snprintf(X_pub.topic, sizeof(X_pub.topic),
               "kit/%s%s", UT_case, X_status);

      snprintf(Y_pub.topic, sizeof(Y_pub.topic),
               "kit/%s%s", UT_case, Y_status);

      // ----- Payload -----
      strncpy(X_pub.payload, X_status_payload, sizeof(X_pub.payload));
      strncpy(Y_pub.payload, Y_status_payload, sizeof(Y_pub.payload));

      // ----- Send Queue -----
      xQueueSend(pubQueue, &X_pub, portMAX_DELAY);
      //xQueueSend(pubQueue, &Y_pub, portMAX_DELAY);

    xSemaphoreGive(hregMutex);
    }
  }
}


void vPublishTask(void* pvParams) {
  for (;;) {
    msg incoming;
    if (xQueueReceive(pubQueue, &incoming, portMAX_DELAY) == pdPASS) {
      // publishMqtt(incoming.topic.c_str(), incoming.payload.c_str());
      publishMqtt(incoming.topic, incoming.payload);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}


void setup() {
  Serial.begin(115200);
  delay(10);
  wifiConnect();
  Serial1.begin(38400, SERIAL_8E1, PIN_RX, PIN_TX);
  node.begin(PLC_slaveID, Serial1);

  // wifiClient.setInsecure();
  Serial.println(WiFi.localIP());
  setupMQTT();

  // for (int i = 0; i < subTopicNum; i++) {
  //   sub_buff[i].topic = "";
  //   sub_buff[i].payload = "";
  // }
  mqttMutex = xSemaphoreCreateMutex();
  hregMutex = xSemaphoreCreateMutex();
  pubQueue = xQueueCreate(20, sizeof(msg));
  xTaskCreate(vPollingTask, "PollingTask", 2048, NULL, 3, NULL);
  xTaskCreate(vReconnectTask, "ReconnectTask", 4096, NULL, 3, NULL);
  xTaskCreate(vPackingTask, "PackingTask", 4096, NULL, 3, NULL);
  xTaskCreate(vPublishTask, "PublishTask", 4096, NULL, 3, NULL);
}

void loop() {
}
