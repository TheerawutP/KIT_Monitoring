//elevatorTypes.h
#pragma once
#include "config.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h> 

enum direction_t
{
  DIR_NONE,
  DIR_UP,
  DIR_DOWN
};

enum state_t
{
  STATE_IDLE,
  STATE_PENDING,
  STATE_RUNNING,
  STATE_PAUSED,
  STATE_EMERGENCY
};

enum modbusStation_t
{
  INVERTER_STA,
  CABIN_STA,
  HALL_2_STA,
  HALL_1_STA,
  VSG_STA
};

typedef struct
{
  uint8_t target;
  direction_t dir;
} transitCommand_t;

enum elevatorEvent_t
{
  SAFETY_BRAKE,
  OVERSPEED,
  DOOR_IS_OPEN,
  DOOR_IS_CLOSED,
  OVERTORQUE,
  VTG_ALARM,
  VSG_ALARM,
  VTG_CLEAR,
  VSG_CLEAR,
  MODBUS_TIMEOUT,
  NO_POWER,
  COMMAND_CLEAR,
  FLOOR1_REACHED,
  FLOOR2_REACHED,
  POWER_RESTORED,
  PAUSED_CLEARED,
  EMERG_PRESSED,
  EMERG_RELEASED
};

typedef struct
{
  uint8_t pos;
  state_t state;
  direction_t dir;
  direction_t lastDir;
  uint8_t target;
  uint8_t lastTarget;
  bool isBrake;
  bool btwFloor;
  elevatorEvent_t evt;
  bool hasChanged;
} status_t;


typedef struct {
    struct {
        bool pos : 1;
        bool state : 1;
        bool dir : 1;
        bool lastDir : 1;
        bool target : 1;
        bool lastTarget : 1;
        bool isBrake : 1;
        bool btwFloor : 1;
        bool hasChanged : 1;
    } set; 

    uint8_t pos;
    state_t state;
    direction_t dir;
    direction_t lastDir;
    uint8_t target;
    uint8_t lastTarget;
    bool isBrake;
    bool btwFloor;
    bool hasChanged;
} update_status_t;



enum commandType_t
{
  moveToFloor,
  userAbort,
  userEmergStop,
  cutPower
};

enum commandSource_t
{
  FROM_RF,
  FROM_WS,
  FROM_CABIN,
  FROM_HALL2
};

typedef struct
{
  uint8_t target;
  commandType_t type;
  commandSource_t from;
} userCommand_t;

///////////////////////////////obj in system.///////////////////////////
typedef struct{
  uint16_t writtenFrame[16];
  
  uint32_t running_hz;
  uint32_t torque;
  uint16_t digitalInput;
  uint16_t raw_regs[10];
} inverter_t; 

typedef struct{
  uint16_t writtenFrame[16];

  bool isDoorClosed;
  bool isAim2;
  bool isAim1;
  bool isUserStop;
  bool isEmergStop;
  bool isBusy;
  bool vtgAlarm;
} cabin_t;

typedef struct{ 
 uint16_t writtenFrame[16];

 bool isAlarm[5];
 bool shouldPause;
} vsg_t;

typedef struct{
  uint16_t writtenFrame[16];
  
  bool isDoorClosed;
  bool vtgAlarm;
} hall_t;

typedef enum
{
  BTN_TO_FLOOR_1,
  BTN_TO_FLOOR_2,
  BTN_TO_FLOOR_3,
  BTN_TO_FLOOR_4,
  BTN_TO_FLOOR_5,
  BTN_TO_FLOOR_6,
  BTN_STOP,
  BTN_EMERGENCY,
  BTN_UNKNOWN
} RF_ButtonType;

typedef struct
{
  unsigned long rfCode;
  RF_ButtonType type;
} RF_KeyMap;

const RF_KeyMap rfKeys[] = {
    // --- set 1 ---
    {toFloor1, BTN_TO_FLOOR_1},
    {toFloor2, BTN_TO_FLOOR_2},
    {STOP, BTN_STOP},
    // {EM_remote_1, BTN_EMERGENCY},

    // --- set 2 ---
    {toFloor1_2, BTN_TO_FLOOR_1},
    {toFloor2_2, BTN_TO_FLOOR_2},
    {STOP_2, BTN_STOP},
    // {EM_remote_2, BTN_EMERGENCY},

    // --- set 3 ---
    {toFloor1_3, BTN_TO_FLOOR_1},
    {toFloor2_3, BTN_TO_FLOOR_2},
    {STOP_3, BTN_STOP},
    // {EM_remote_3, BTN_EMERGENCY},

    // --- set 4 ---
    {toFloor1_4, BTN_TO_FLOOR_1},
    {toFloor2_4, BTN_TO_FLOOR_2},
    {STOP_4, BTN_STOP},
    // {EM_remote_4, BTN_EMERGENCY},

    // --- set 5 ---
    {toFloor1_5, BTN_TO_FLOOR_1},
    {toFloor2_5, BTN_TO_FLOOR_2},
    {STOP_5, BTN_STOP}
};

const int numRfKeys = sizeof(rfKeys) / sizeof(RF_KeyMap);

typedef struct struct_message
{
  uint8_t fromID;
  uint16_t commandFrame;
  uint16_t responseFrame;
  bool shouldResponse;
} struct_message;


