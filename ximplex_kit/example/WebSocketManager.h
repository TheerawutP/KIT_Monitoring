#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <Arduino.h>
#include "WebSocketsServer.h"
#include <ArduinoJson.h>
#include <functional>
#include "ICommunicationService.h"

// Callback types for application-specific handling
using ResetMcuCallback = std::function<void()>;

class WebSocketManager : public ICommunicationService {
public:
    WebSocketManager(uint16_t port = 81);
    ~WebSocketManager();

    // ICommunicationService interface implementation
    void begin() override;
    void loop() override;
    void sendStatus(const char* jsonData) override;
    void sendAlert(const char* alertType, const char* message) override;
    void pushHistory(const char* type, const char* jsonData) override;

    // WebSocket-specific methods
    void broadcastTXT(const char* data, size_t length);
    void broadcastTXT(const char* data);
    void onEvent();

    // Get remote IP for connected client
    IPAddress remoteIP(uint8_t num);

    // Set callbacks for application-specific handling
    void setResetMcuCallback(ResetMcuCallback callback);

private:
    WebSocketsServer m_websocketServer;
    uint16_t m_port;

    // Callbacks
    ResetMcuCallback m_resetMcuCallback;

    // Event handler
    static void onWebSocketEventStatic(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

    // Message handlers
    void handleWebSocketText(uint8_t* payload);
};

#endif // WEBSOCKET_MANAGER_H