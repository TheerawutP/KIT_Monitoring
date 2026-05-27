#ifndef FIREBASE_CONFIG_H
#define FIREBASE_CONFIG_H

// Firebase Configuration
// Replace these with your actual Firebase credentials

#define FIREBASE_API_KEY "AIzaSyB6XlIfQMBTpznkeFiUehZ0rsL-YMkgVe0"
#define FIREBASE_DATABASE_URL "https://xim-sharklice-default-rtdb.firebaseio.com/"
#define FIREBASE_USER_EMAIL "flukflipflap1@gmail.com"
#define FIREBASE_USER_PASSWORD "killthegame121"

// Device Configuration
#define FIREBASE_DEVICE_PATH "/remora"

// Communication Settings
#define USE_FIREBASE_COMMUNICATION true  // Set to true to use Firebase, false for WebSocket

// Performance Settings
#define FIREBASE_STATUS_UPDATE_INTERVAL 100  // ms between status updates
#define FIREBASE_MAX_RETRIES 3               // Number of retry attempts
#define FIREBASE_TIMEOUT 5000                // ms timeout for operations

#endif // FIREBASE_CONFIG_H