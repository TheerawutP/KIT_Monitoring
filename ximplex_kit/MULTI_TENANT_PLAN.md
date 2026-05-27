# Multi-Tenant Elevator Management System Implementation Plan

## 1. Project Overview
The goal is to scale the current **ximplex_kit** from a single-device prototype to a production-ready system where one user account can manage multiple specific elevators. This requires strict data isolation (User A cannot see/control User B's devices) and reliable command execution.

---

## 2. Side A: Firebase RTDB (The Cloud Infrastructure)

### A. Data Schema Design
We will move away from a flat structure to a hierarchical one.

```json
{
  "users": {
    "USER_UID": {
      "profile": { "name": "...", "role": "admin" },
      "devices": {
        "ELEVATOR_SN_001": true,
        "ELEVATOR_SN_002": true
      }
    }
  },
  "devices": {
    "ELEVATOR_SN_001": {
      "info": { "name": "East Wing Lift", "model": "X-100", "installed": "2024-01-01" },
      "status": {
        "x_mask": "0xFFFF",
        "y_mask": "0x0001",
        "hr_meter": 4500,
        "door_cycles": 120,
        "last_seen": 1716885000
      },
      "commands": {
        "current": { "id": "uuid_123", "type": "GO_TO_FLOOR", "val": 5, "ts": 1716885010 },
        "last_result": { "id": "uuid_123", "status": "SUCCESS", "ts": 1716885015 }
      },
      "logs": {
        "1716885000": { "type": "ALERT", "msg": "Door Obstruction" }
      }
    }
  }
}
```

### B. Security Rules (Isolation Logic)
Rules must enforce that a user can only access devices listed in their `users/UID/devices` node.

```javascript
{
  "rules": {
    "users": {
      "$uid": {
        ".read": "$uid === auth.uid",
        ".write": "$uid === auth.uid"
      }
    },
    "devices": {
      "$device_id": {
        ".read": "root.child('users').child(auth.uid).child('devices').child($device_id).exists()",
        ".write": "root.child('users').child(auth.uid).child('devices').child($device_id).exists()"
      }
    }
  }
}
```

---

## 3. Side B: ESP32 (The Device Side)

### A. Device Identity
*   **Unique ID:** Use the ESP32 Chip ID or MAC address as the primary `DEVICE_ID`.
*   **Initialization:** On boot, the ESP32 constructs its paths: `/devices/{DEVICE_ID}/status` and `/devices/{DEVICE_ID}/commands`.

### B. Modbus Mapping (Octal Logic)
*   **Polling:** The ESP32 polls 16-bit registers from the PLC.
*   **Bit Conversion:**
    *   Register 0 (16 bits) -> `X0-X7` (lower byte) and `X10-X17` (upper byte).
    *   This preserves the PLC's octal addressing logic in the JSON payload sent to Firebase.

### C. Reliable Command Loop (The Handshake)
To ensure the elevator works properly:
1.  **Listen:** Stream `/devices/{DEVICE_ID}/commands/current`.
2.  **Verify:** Check the timestamp (`ts`) and ID. If it's a new command, execute the Modbus write to the PLC.
3.  **Acknowledge:** Write the result to `/devices/{DEVICE_ID}/commands/last_result`.
4.  **Clear:** Delete the `current` command to prevent re-execution on reboot.

---

## 4. Side C: Webapp (The Management Side)

### A. User Authentication
*   Standard Firebase Auth (Email/Password or Google).
*   **Pairing Process:** To add an elevator, the user enters the `DEVICE_ID` (printed on the hardware). The Webapp adds this ID to the user's `managed_devices` list in Firebase.

### B. Dashboard Features
*   **Live View:** Real-time indicator lights for `X` and `Y` bits using SVG icons.
*   **History Charts:** Graphs showing elevator usage (door cycles) over time.
*   **Remote Control:** Buttons that write to the `commands/current` path.

### C. UI Feedback Loop
*   **State Management:** When a user clicks "Reset Counter", the button shows a "Sending..." state.
*   **Success Indicator:** The button turns green only when the Webapp detects the update in `commands/last_result` from the ESP32.

---

## 5. Verification Plan

### Phase 1: Isolation Test
*   Create two accounts.
*   Verify Account A cannot read data from Elevator B by attempting a direct URL access.

### Phase 2: Stress Test (Reliability)
*   Trigger 10 commands in rapid succession.
*   Verify the ESP32 processes them sequentially without skipping or double-executing.

### Phase 3: Connectivity Test
*   Disconnect the ESP32 for 5 minutes.
*   Verify the Webapp shows the device as "Offline" (via a heartbeat/last_seen timestamp).
