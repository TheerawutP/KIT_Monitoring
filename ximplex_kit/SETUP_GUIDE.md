# Firebase & Webapp Setup Guide (Ximplex Multi-Tenant)

To make the multi-tenant system work, follow these steps to configure your cloud environment and connect your web application.

---

## 1. Firebase RTDB Configuration

### Step A: Database Creation
1. Go to the [Firebase Console](https://console.firebase.google.com/).
2. Select your project and navigate to **Realtime Database**.
3. Create a database in your preferred region.
4. Set the initial security rules to `public` for testing, but **change them immediately** after verification (see Step B).

### Step B: Deploy Security Rules
Copy the contents of `FIREBASE_SECURITY_RULES.json` (created in your project root) and paste them into the **Rules** tab of your Firebase Realtime Database. This ensures that:
* Users can only see their authorized elevators.
* The ESP32 paths are protected.

### Step C: Initial Data Seeding (For Testing)
To authorize a user to see a device, you must manually (or via a signup flow) add the device ID to the user's profile.
1. Find your ESP32's unique ID from the Serial Monitor (e.g., `ESP32_AABBCCDDEEFF`).
2. In the Database **Data** tab, create this structure:
   ```json
   {
     "users": {
       "YOUR_AUTH_UID": {
         "devices": {
           "ESP32_AABBCCDDEEFF": true
         }
       }
     }
   }
   ```

---

## 2. Webapp Setup

### Step A: Environment Configuration
Open `CENTRAL_DASHBOARD_PROTOTYPE.html` and locate the `firebaseConfig` object. Replace the placeholders with your actual project credentials from the Firebase Console (**Project Settings > General > Your apps**).

```javascript
const firebaseConfig = {
    apiKey: "...",
    authDomain: "...",
    databaseURL: "...",
    projectId: "...",
    // ... rest of your keys
};
```

### Step B: User Authentication
The dashboard uses Firebase Auth. You must enable the **Email/Password** provider in the Firebase Console:
1. Go to **Authentication > Sign-in method**.
2. Enable **Email/Password**.
3. Create a test user account.

### Step C: How to Control the Elevator
1. Login to the dashboard with your test account.
2. Select your elevator ID from the dropdown.
3. **Status:** The `X` and `Y` grids will light up automatically as the PLC state changes.
4. **Commands:** When you click "Reset Hour Meter":
   * The Webapp writes a unique command to `/devices/{ID}/commands/current`.
   * The ESP32 sees it, resets the counter, and writes to `/devices/{ID}/commands/last_result`.
   * The Webapp sees the result and updates the "Command Status" text.

---

## 3. Deployment Logic

### Adding New Elevators
When you deploy a new kit:
1. Flash the ESP32 code.
2. Note the `ESP32_{MAC}` ID from the Serial Monitor.
3. In your database (or admin panel), add that ID to the specific user's `devices` list.
4. The elevator will immediately appear in that user's dashboard.

### Reliability Tip
If the elevator shows "Offline" in the dashboard, check the `last_seen` timestamp in the database. You can add a heartbeat to the `vFirebasePublishTask` to update this timestamp every minute.
