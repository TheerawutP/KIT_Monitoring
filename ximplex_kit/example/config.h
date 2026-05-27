//config.h
#pragma once
#include "arduino.h"

#define PIN_RX 16
#define PIN_TX 17
#define WIFI_READY 13
#define RFReceiver 23
#define floorSensor1 32
#define floorSensor2 33
#define R_UP 19        // Relay UP
#define R_DW 18        // Relay DOWN
#define R_POWER_CUT 15 // Relay 4
#define BRK 5          // brake
#define NoPower 25
#define safetySling 26
#define speedGovernor 14
#define EMO 21 // emergency stop

#define MASTER_ID 100

#define toFloor1 174744
#define toFloor2 174740
// #define POWER_CUT 174738
#define STOP 174737


#define INVERTER_DI_STOP 992
#define INVERTER_DI_EMO 5092

// remote set2 sena
#define toFloor1_2 12418580
#define toFloor2_2 12418584
#define STOP_2 12418578
#define EM_remote_2 12418577

// remote set3 sena
#define toFloor1_3 16003636
#define toFloor2_3 16003640
#define STOP_3 16003634
#define EM_remote_3 16003633

// remote set4 sena
#define toFloor1_4 16751236
#define toFloor2_4 16751240
#define STOP_4 16751234
#define EM_remote_4 16751233

//set 5 
#define toFloor1_5 9395170
#define toFloor2_5 9395169
#define STOP_5 16736120

#define SF_0000 0
#define SF_1001 1  // going up
#define SF_1002 2  // going dw
#define SF_1003 3  // reaching
#define SF_1004 4  // obstable under cabin
#define SF_1005 5  // beware pinch
#define SF_1006 6  // no power go to floor1
#define SF_1007 7  // overweight
#define SF_1008 8  // safety brake
#define SF_1009 9  // please close the door
#define SF_1010 10 // elevator ready to use
#define SF_1011 11 // cant connect to wifi
#define SF_1012 12 // system going to reset
#define SF_1013 13 // wait for modbus
#define SF_1014 14 // reach1
#define SF_1015 15 // reach2
#define SF_1016 16 // emerg stop

// both dir block
#define MODBUS_DIS_BIT (1 << 0) // 0x01
#define DOOR_OPEN_BIT (1 << 1)  // 0x02
#define EMERG_BIT (1 << 2)      // 0x04

// block go up
#define VTG_BIT (1 << 3) // 0x08

// block go down
#define SAFETY_BRAKE_BIT (1 << 4) // 0x10
#define VSG_BIT (1 << 5)          // 0x20

// block dir group (true = dont allowed)
#define BLOCK_UP_MASK (VTG_BIT | EMERG_BIT | DOOR_OPEN_BIT | MODBUS_DIS_BIT)
#define BLOCK_DOWN_MASK (SAFETY_BRAKE_BIT | VSG_BIT | EMERG_BIT | DOOR_OPEN_BIT | MODBUS_DIS_BIT)


#define WAIT_TO_RUNNING_MS 700

#define torque_rated_up 80
#define torque_rated_down 40 // both in percentage


// const uint32_t minSpeedPeriod = 1000;
#define upper_bound_speed_interval 800
#define lower_bound_speed_interval 1000
#define overSpeed_threshold 2

#define MIN_FLOOR 1
#define MAX_FLOOR 2


// Heartbeat and Pairing
#define HEARTBEAT_INTERVAL 200
#define HEARTBEAT_TIMEOUT 10000


