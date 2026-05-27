#ifndef APP_HARDWARE_CONFIG_H
#define APP_HARDWARE_CONFIG_H

// Hardware pins
static constexpr int PIN_RX = 16;
static constexpr int PIN_TX = 17;
static constexpr int WIFI_READY = 22;

// Modbus / device IDs
static constexpr int PLC_slaveID = 1;
static constexpr int SERVO_slaveID = 2;

// Register addresses
static constexpr int X0_ADD = 0;
static constexpr int Y0_ADD = 4;

// IO sizes
static constexpr int X_size = 8; // 8*8 input
static constexpr int Y_size = 8; // 8*8 output

// Other defaults
static constexpr int slaveNum = 2;
static constexpr int subTopicNum = 10;

#endif // APP_HARDWARE_CONFIG_H

