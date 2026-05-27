#ifndef I_COMMUNICATION_SERVICE_H
#define I_COMMUNICATION_SERVICE_H

#include <Arduino.h>

/**
 * @brief Interface for communication services (WebSocket, Firebase, etc.)
 *
 * This interface provides a common API for different communication methods,
 * allowing easy switching between WebSocket, Firebase, or other protocols
 * without changing the main application logic.
 */
class ICommunicationService {
public:
    virtual ~ICommunicationService() = default;

    /**
     * @brief Initialize the communication service
     */
    virtual void begin() = 0;

    /**
     * @brief Main loop for the communication service
     * Should be called regularly in the main loop
     */
    virtual void loop() = 0;

    /**
     * @brief Send status data as JSON
     * @param jsonData JSON string containing elevator status
     */
    virtual void sendStatus(const char* jsonData) = 0;

    /**
     * @brief Send alert/notification
     * @param alertType Type of alert (ERROR, WARNING, INFO)
     * @param message Alert message content
     */
    virtual void sendAlert(const char* alertType, const char* message) = 0;

    /**
     * @brief Push data to history log
     * @param type Log type ("commands" or "status")
     * @param jsonData JSON string containing log data
     */
    virtual void pushHistory(const char* type, const char* jsonData) = 0;
};

#endif // I_COMMUNICATION_SERVICE_H