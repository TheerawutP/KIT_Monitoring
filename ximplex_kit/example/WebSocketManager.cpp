#include "WebSocketManager.h"

// Static pointer to instance for static callback
static WebSocketManager* s_instance = nullptr;

WebSocketManager::WebSocketManager(uint16_t port)
    : m_websocketServer(port), m_port(port), m_resetMcuCallback(nullptr) {
    s_instance = this;
}

WebSocketManager::~WebSocketManager() {
    s_instance = nullptr;
}

void WebSocketManager::begin() {
    m_websocketServer.begin();
    Serial.printf("WebSocket server started on port %d\n", m_port);
}

void WebSocketManager::onEvent() {
    m_websocketServer.onEvent(onWebSocketEventStatic);
}

void WebSocketManager::loop() {
    m_websocketServer.loop();
}

void WebSocketManager::sendAlert(const char* alertType, const char* message) {
    char alertBuf[128];
    snprintf(alertBuf, sizeof(alertBuf), "{\"alert\":\"%s\", \"msg\":\"%s\"}", alertType, message);

    m_websocketServer.broadcastTXT(alertBuf, strlen(alertBuf));
    Serial.printf(">> Sent Alert to UI: %s\n", message);
}

void WebSocketManager::sendStatus(const char* jsonData) {
    broadcastTXT(jsonData, strlen(jsonData));
}

void WebSocketManager::pushHistory(const char* type, const char* jsonData) {
    // Stub implementation for WebSocket
    // Could potentially broadcast to a specific log channel if needed
}

void WebSocketManager::broadcastTXT(const char* data, size_t length) {
    m_websocketServer.broadcastTXT(data, length);
}

void WebSocketManager::broadcastTXT(const char* data) {
    m_websocketServer.broadcastTXT(data);
}

IPAddress WebSocketManager::remoteIP(uint8_t num) {
    return m_websocketServer.remoteIP(num);
}

void WebSocketManager::setResetMcuCallback(ResetMcuCallback callback) {
    m_resetMcuCallback = callback;
}

void WebSocketManager::onWebSocketEventStatic(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (s_instance) {
        s_instance->onWebSocketEvent(num, type, payload, length);
    }
}

void WebSocketManager::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;

        case WStype_CONNECTED: {
            IPAddress ip = m_websocketServer.remoteIP(num);
            Serial.printf("[%u] Connection from ", num);
            Serial.println(ip.toString());
        }
        break;

        case WStype_TEXT:
            Serial.printf("[%u] Received text: %s\n", num, payload);
            handleWebSocketText(payload);
            break;

        case WStype_BIN:
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        case WStype_PING:
        case WStype_PONG:
            // Do nothing for these event types
            break;
    }
}

void WebSocketManager::handleWebSocketText(uint8_t* payload) {
    Serial.printf("handle_websocket_text called for: %s\n", payload);

    // Parse JSON payload
    StaticJsonDocument<1000> jsonDoc;
    DeserializationError error = deserializeJson(jsonDoc, payload);

    if (error) {
        Serial.println("deserializeJson() failed with code ");
        Serial.println(error.c_str());
        return;
    }

    // Handle reset_mcu command
    if (jsonDoc["reset_mcu"] == true) {
        Serial.println(">> WebCommand: Reset MCU Request Received.");
        sendAlert("WARNING", "MCU is restarting...");

        delay(500);

        if (m_resetMcuCallback) {
            m_resetMcuCallback();
        } else {
            ESP.restart();
        }
    }


}