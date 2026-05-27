// FirebaseCommunicationService.cpp
#include "FirebaseCommunicationService.h"

#include <addons/RTDBHelper.h>
#include <addons/TokenHelper.h>

// Constants for limits
static constexpr int MAX_RETRIES = 3;

FirebaseCommunicationService::FirebaseCommunicationService(
    const char *apiKey, const char *databaseUrl,
    const char *userEmail, const char *userPassword,
    const char *devicePath,
    unsigned long statusUpdateIntervalMs)
    : m_apiKey(apiKey), m_databaseUrl(databaseUrl),
      m_userEmail(userEmail), m_userPassword(userPassword),
      m_devicePath(devicePath),
      m_lastStatusUpdate(0),
      m_statusUpdateInterval(statusUpdateIntervalMs),
      m_isConnected(false)
{
  // Build Firebase paths according to Multi-Tenant Plan
  m_statusPath = m_devicePath + "/status";
  m_alertsPath = m_devicePath + "/alerts";
  m_commandsPath = m_devicePath + "/commands/current";
  m_lastResultPath = m_devicePath + "/commands/last_result";
  m_historyPath = m_devicePath + "/history_log";

  Serial.println("FirebaseCommunicationService created");
}

FirebaseCommunicationService::~FirebaseCommunicationService()
{
  Serial.println("FirebaseCommunicationService destroyed");
}

void FirebaseCommunicationService::begin()
{
  Serial.println("Initializing Firebase Communication Service...");

  if (!m_apiKey || strlen(m_apiKey) == 0 ||
      !m_databaseUrl || strlen(m_databaseUrl) == 0 ||
      !m_userEmail || strlen(m_userEmail) == 0 ||
      !m_userPassword || strlen(m_userPassword) == 0)
  {
    Serial.println("Firebase config missing. Please set FIREBASE_* macros in FirebaseConfig.h");
    m_isConnected = false;
    return;
  }

  // Configure Firebase
  config.api_key = m_apiKey;
  config.database_url = m_databaseUrl;

  // Set up authentication
  auth.user.email = m_userEmail;
  auth.user.password = m_userPassword;

  // Token status callback (from Firebase add-on)
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase Communication Service initialized!");
  Serial.printf("Device Path: %s\n", m_devicePath.c_str());
  Serial.printf("Status Path: %s\n", m_statusPath.c_str());
  Serial.printf("Commands Path: %s\n", m_commandsPath.c_str());

  setupCommandListener();
}

void FirebaseCommunicationService::loop()
{
  processIncomingCommands();
  delay(10);
}

void FirebaseCommunicationService::sendStatus(const char *jsonData)
{
  if (!jsonData || strlen(jsonData) == 0)
  {
    return;
  }

  // Rate limiting
  unsigned long now = millis();
  if (now - m_lastStatusUpdate < m_statusUpdateInterval)
  {
    return;
  }
  m_lastStatusUpdate = now;

  FirebaseJson json;
  json.setJsonData(jsonData);

  if (sendToFirebase(m_statusPath, &json))
  {
    m_isConnected = true;
  }
  else
  {
    m_isConnected = false;
    handleFirebaseError("sendStatus");
  }
}

void FirebaseCommunicationService::sendAlert(const char *alertType, const char *message)
{
  if (!alertType || !message)
  {
    return;
  }

  unsigned long timestamp = millis();
  String alertPath = m_alertsPath;
  alertPath += "/";
  alertPath += String(timestamp);

  String alertJson = "{\"type\":\"";
  alertJson += String(alertType);
  alertJson += "\",\"message\":\"";
  alertJson += String(message);
  alertJson += "\",\"timestamp\":";
  alertJson += String(timestamp);
  alertJson += "}";

  FirebaseJson alertData;
  alertData.setJsonData(alertJson);

  if (!sendToFirebase(alertPath, &alertData))
  {
    Serial.printf("Failed to send alert: %s\n", fbdo_send.errorReason().c_str());
  }
}

void FirebaseCommunicationService::pushHistory(const char *type, const char *jsonData)
{
  if (!type || !jsonData)
    return;

  String path = m_historyPath;
  path += "/";
  path += String(type);
  FirebaseJson json;
  json.setJsonData(jsonData);

  if (Firebase.ready())
  {
    if (!Firebase.RTDB.pushJSON(&fbdo_send, path.c_str(), &json))
    {
      Serial.printf("Failed to push history (%s): %s\n", type, fbdo_send.errorReason().c_str());
    }
  }
}

void FirebaseCommunicationService::acknowledgeCommand(const char *commandId, const char *status)
{
  if (!commandId || !status)
    return;

  FirebaseJson json;
  json.add("id", commandId);
  json.add("status", status);
  json.add("ts", (uint64_t)millis()); // Simple timestamp for now

  if (!sendToFirebase(m_lastResultPath, &json))
  {
    Serial.printf("Failed to acknowledge command: %s\n", fbdo_send.errorReason().c_str());
  }
  else
  {
    Serial.printf("Command %s acknowledged with status %s\n", commandId, status);
  }
}

bool FirebaseCommunicationService::isConnected()
{
  return m_isConnected && Firebase.ready();
}

void FirebaseCommunicationService::setCommandCallback(std::function<void(const char *id, const char *command, const char *data)> callback)
{
  m_commandCallback = callback;
  Serial.println("Command callback registered");
}

void FirebaseCommunicationService::clearCommand()
{
  if (Firebase.RTDB.deleteNode(&fbdo_send, m_commandsPath.c_str()))
  {
    Serial.println("Current command cleared from Firebase");
  }
  else
  {
    Serial.printf("Failed to clear command: %s\n", fbdo_send.errorReason().c_str());
  }
}

void FirebaseCommunicationService::setupCommandListener()
{
  Serial.println("Setting up Firebase command listener...");

  if (Firebase.RTDB.beginStream(&fbdo, m_commandsPath.c_str()))
  {
    Serial.printf("Command listener started on: %s\n", m_commandsPath.c_str());
  }
  else
  {
    Serial.printf("Failed to start command listener: %s\n", fbdo.errorReason().c_str());
  }
}

void FirebaseCommunicationService::processIncomingCommands()
{
  if (Firebase.RTDB.readStream(&fbdo))
  {
    if (fbdo.streamAvailable())
    {
      if (fbdo.dataType() == "json")
      {
        FirebaseJson json;
        json.setJsonData(fbdo.stringData());

        String commandId;
        String commandType;
        String commandData;
        FirebaseJsonData jsonData;

        if (json.get(jsonData, "id"))
        {
          commandId = jsonData.stringValue;
        }

        if (json.get(jsonData, "type"))
        {
          commandType = jsonData.stringValue;
        }

        if (json.get(jsonData, "data"))
        {
          commandData = jsonData.stringValue;
        }

        if (!commandId.isEmpty() && !commandType.isEmpty() && m_commandCallback)
        {
          m_commandCallback(commandId.c_str(), commandType.c_str(), commandData.c_str());
          clearCommand();
        }
      }

      fbdo.clear();
    }
  }

  if (fbdo.streamTimeout())
  {
    Serial.println("Stream timeout, auto-resuming...");
  }
}

void FirebaseCommunicationService::handleFirebaseError(const char *operation)
{
  Serial.printf("Firebase error during %s: %s\n", operation, fbdo.errorReason().c_str());

  if (fbdo.errorReason().indexOf("network") >= 0 ||
      fbdo.errorReason().indexOf("connection") >= 0)
  {
    m_isConnected = false;
  }
}

bool FirebaseCommunicationService::sendToFirebase(const String &path, FirebaseJson *jsonPtr)
{
  if (!Firebase.ready())
  {
    return false;
  }
  if (!jsonPtr)
  {
    return false;
  }

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++)
  {
    if (Firebase.RTDB.setJSON(&fbdo_send, path.c_str(), jsonPtr))
    {
      return true;
    }
    if (attempt < MAX_RETRIES - 1)
    {
      delay(100);
    }
  }

  return false;
}

