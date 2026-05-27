// FirebaseCommunicationService.h
#ifndef FIREBASE_COMMUNICATION_SERVICE_H
#define FIREBASE_COMMUNICATION_SERVICE_H

#include <Arduino.h>
#include <functional>

#include <Firebase_ESP_Client.h>

#include "ICommunicationService.h"

/**
 * @brief Firebase Realtime Database communication service
 *
 * Implements the ICommunicationService interface using Firebase RTDB
 * for bidirectional communication with web applications.
 */
class FirebaseCommunicationService : public ICommunicationService
{
public:
  /**
   * @brief Constructor
   * @param apiKey Firebase API key
   * @param databaseUrl Firebase database URL
   * @param userEmail Firebase authentication email
   * @param userPassword Firebase authentication password
   * @param devicePath Base path for this device (e.g., "/ximplex_kit_01")
   * @param statusUpdateIntervalMs Minimum interval between status updates
   */
  FirebaseCommunicationService(const char *apiKey, const char *databaseUrl,
                              const char *userEmail, const char *userPassword,
                              const char *devicePath,
                              unsigned long statusUpdateIntervalMs);

  ~FirebaseCommunicationService();

  // ICommunicationService interface implementation
  void begin() override;
  void loop() override;
  void sendStatus(const char *jsonData) override;
  void sendAlert(const char *alertType, const char *message) override;
  void pushHistory(const char *type, const char *jsonData) override;
  void acknowledgeCommand(const char *commandId, const char *status) override;

  // Firebase-specific methods
  bool isConnected();
  void setCommandCallback(std::function<void(const char *id, const char *command, const char *data)> callback);
  void clearCommand();

private:
  // Firebase objects
  FirebaseData fbdo;
  FirebaseData fbdo_send;
  FirebaseAuth auth;
  FirebaseConfig config;

  // Configuration
  const char *m_apiKey;
  const char *m_databaseUrl;
  const char *m_userEmail;
  const char *m_userPassword;
  String m_devicePath;

  // Firebase paths
  String m_statusPath;
  String m_alertsPath;
  String m_commandsPath;
  String m_lastResultPath;
  String m_historyPath;

  // State tracking
  unsigned long m_lastStatusUpdate;
  unsigned long m_statusUpdateInterval;
  bool m_isConnected;

  // Command handling
  std::function<void(const char *id, const char *command, const char *data)> m_commandCallback;

  // Helper methods
  void setupCommandListener();
  void processIncomingCommands();
  void handleFirebaseError(const char *operation);
  bool sendToFirebase(const String &path, FirebaseJson *jsonPtr);
};

#endif // FIREBASE_COMMUNICATION_SERVICE_H

