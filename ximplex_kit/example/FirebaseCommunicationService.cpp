//FirebaseCommunicationService.cpp
#include "FirebaseCommunicationService.h"
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

// Constants for timing and limits
const unsigned long DEFAULT_STATUS_UPDATE_INTERVAL = 100; // ms
const unsigned long FIREBASE_TIMEOUT = 5000; // ms
const int MAX_RETRIES = 3;

FirebaseCommunicationService::FirebaseCommunicationService(
    const char* apiKey, const char* databaseUrl,
    const char* userEmail, const char* userPassword,
    const char* devicePath)
    : m_apiKey(apiKey), m_databaseUrl(databaseUrl),
      m_userEmail(userEmail), m_userPassword(userPassword),
      m_devicePath(devicePath),
      m_lastStatusUpdate(0),
      m_statusUpdateInterval(DEFAULT_STATUS_UPDATE_INTERVAL),
      m_isConnected(false) {

    // Build Firebase paths
    m_statusPath = String(m_devicePath);
    m_statusPath += "/status";

    m_alertsPath = String(m_devicePath);
    m_alertsPath += "/alerts";

    m_commandsPath = String(m_devicePath);
    m_commandsPath += "/commands";

    m_historyPath = String(m_devicePath);
    m_historyPath += "/history_log";

    Serial.println("FirebaseCommunicationService created");
}

FirebaseCommunicationService::~FirebaseCommunicationService() {
    Serial.println("FirebaseCommunicationService destroyed");
}

void FirebaseCommunicationService::begin() {
    Serial.println("Initializing Firebase Communication Service...");

    // Configure Firebase
    config.api_key = m_apiKey;
    config.database_url = m_databaseUrl;

    // Set up authentication
    auth.user.email = m_userEmail;
    auth.user.password = m_userPassword;

    // Set up token status callback for debugging
    config.token_status_callback = tokenStatusCallback;

    // Initialize Firebase
    Firebase.begin(&config, &auth);

    // Enable auto-reconnection
    Firebase.reconnectWiFi(true);

    Serial.println("Firebase Communication Service initialized!");
    Serial.printf("Device Path: %s\n", m_devicePath);
    Serial.printf("Status Path: %s\n", m_statusPath.c_str());
    Serial.printf("Alerts Path: %s\n", m_alertsPath.c_str());
    Serial.printf("Commands Path: %s\n", m_commandsPath.c_str());
    // Set up command listener
    setupCommandListener();
}

void FirebaseCommunicationService::loop() {
    // Firebase automatically handles reconnection and token refresh
    // Just need to process any incoming commands
    processIncomingCommands();

    // Small delay to prevent overwhelming Firebase
    delay(10);
}

void FirebaseCommunicationService::sendStatus(const char* jsonData) {
    if (!jsonData || strlen(jsonData) == 0) {
        Serial.println("Cannot send empty status data");
        return;
    }

    // Rate limiting to prevent Firebase quota issues
    unsigned long now = millis();
    if (now - m_lastStatusUpdate < m_statusUpdateInterval) {
        return; // Skip this update
    }

    m_lastStatusUpdate = now;
    FirebaseJson json;
    json.setJsonData(jsonData);

    // Send status to Firebase
    if (sendToFirebase(m_statusPath, &json)) {
        // Successfully sent
        m_isConnected = true;
    } else {
        m_isConnected = false;
        handleFirebaseError("sendStatus");
    }
}

void FirebaseCommunicationService::sendAlert(const char* alertType, const char* message) {
    if (!alertType || !message) {
        Serial.println("Cannot send alert with null parameters");
        return;
    }

    // Create alert JSON with timestamp
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

    if (sendToFirebase(alertPath, &alertData)) {
        Serial.printf("Alert sent to Firebase: %s - %s\n", alertType, message);
    } else {
        Serial.printf("Failed to send alert: %s\n", fbdo.errorReason().c_str());
    }
}

void FirebaseCommunicationService::pushHistory(const char* type, const char* jsonData) {
    if (!type || !jsonData) return;

    String path = m_historyPath;
    path += "/";
    path += type;

    FirebaseJson json;
    json.setJsonData(jsonData);

    // Use pushJSON to create a timestamp-based unique ID
    if (Firebase.ready()) {
        if (!Firebase.RTDB.pushJSON(&fbdo_send, path.c_str(), &json)) {
            Serial.printf("Failed to push history (%s): %s\n", type, fbdo_send.errorReason().c_str());
        }
    }
}

bool FirebaseCommunicationService::isConnected() {
    return m_isConnected && Firebase.ready();
}

void FirebaseCommunicationService::setCommandCallback(
    std::function<void(const char* command, const char* data)> callback) {
    m_commandCallback = callback;
    Serial.println("Command callback registered");
}

void FirebaseCommunicationService::clearCommand() {
    // Delete the command from Firebase after processing
    if (Firebase.RTDB.deleteNode(&fbdo_send, m_commandsPath.c_str())) {
        Serial.println("Command cleared from Firebase");
    } else {
        Serial.printf("Failed to clear command: %s\n", fbdo_send.errorReason().c_str());
    }
}

void FirebaseCommunicationService::setupFirebaseConnection() {
    Serial.println("Setting up Firebase connection...");

    // Wait for Firebase to be ready
    int attempts = 0;
    while (!Firebase.ready() && attempts < MAX_RETRIES) {
        Serial.printf("Waiting for Firebase... (attempt %d/%d)\n", attempts + 1, MAX_RETRIES);
        delay(1000);
        attempts++;
    }

    if (Firebase.ready()) {
        Serial.println("Firebase is ready!");
        m_isConnected = true;
    } else {
        Serial.println("Firebase connection timeout");
        m_isConnected = false;
    }
}

void FirebaseCommunicationService::setupCommandListener() {
    Serial.println("Setting up Firebase command listener...");

    // 🟢 ใช้ beginStream() เพื่อเปิดการเชื่อมต่อท่อข้อมูลแบบ Real-time
    if (Firebase.RTDB.beginStream(&fbdo, m_commandsPath.c_str())) {
        Serial.printf("Command listener started on: %s\n", m_commandsPath.c_str());
    } else {
        Serial.printf("Failed to start command listener: %s\n", fbdo.errorReason().c_str());
    }
}

void FirebaseCommunicationService::processIncomingCommands() {
    if (Firebase.RTDB.readStream(&fbdo)) {
        
        if (fbdo.streamAvailable()) {
            Serial.println("Processing incoming command from Firebase");

            // Parse the command JSON
            if (fbdo.dataType() == "json") {
                FirebaseJson json;
                json.setJsonData(fbdo.stringData());

                // Extract command type and data
                String commandType;
                String commandData;
                FirebaseJsonData jsonData;

                // Get command type
                if (json.get(jsonData, "command")) {
                    commandType = jsonData.stringValue;
                    Serial.printf("Command type: %s\n", commandType.c_str());
                }

                // Get command data if present
                if (json.get(jsonData, "data")) {
                    commandData = jsonData.stringValue;
                    Serial.printf("Command data: %s\n", commandData.c_str());
                }

                // Call the registered callback if we have a command
                if (!commandType.isEmpty() && m_commandCallback) {
                    m_commandCallback(commandType.c_str(), commandData.c_str());
                    clearCommand();  // Clear the command from Firebase
                }
            }

            // Clear the stream data after processing
            fbdo.clear();
        }
    }
    
    if (fbdo.streamTimeout()) {
        Serial.println("Stream timeout, auto-resuming...");
    }
}

void FirebaseCommunicationService::handleFirebaseError(const char* operation) {
    Serial.printf("Firebase error during %s: %s\n", operation, fbdo.errorReason().c_str());

    // Check if it's an authentication error
    if (fbdo.errorReason().indexOf("auth") >= 0) {
        Serial.println("Authentication error - will retry on next operation");
    }

    // Check if it's a network error
    if (fbdo.errorReason().indexOf("network") >= 0 ||
        fbdo.errorReason().indexOf("connection") >= 0) {
        Serial.println("Network error - Firebase will auto-reconnect");
        m_isConnected = false;
    }
}

bool FirebaseCommunicationService::sendToFirebase(const String& path, FirebaseJson* jsonPtr) {
    if (!Firebase.ready()) {
        Serial.println("Firebase not ready - cannot send data");
        return false;
    }

    if (!jsonPtr) {
        Serial.println("Invalid JSON pointer");
        return false;
    }

    // Try to send data with retries
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        if (Firebase.RTDB.setJSON(&fbdo_send, path.c_str(), jsonPtr)) {
            return true;
        }

        Serial.printf("Send attempt %d failed: %s\n", attempt + 1, fbdo_send.errorReason().c_str());

        // Wait before retry
        if (attempt < MAX_RETRIES - 1) {
            delay(100);
        }
    }

    return false;
}