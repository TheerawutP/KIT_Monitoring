#ifndef APP_TYPES_H
#define APP_TYPES_H

enum read_state
{
  PLC,
  SERVO
};

enum door_state
{
  DOOR_NULL,
  DOOR_CLOSED,
  DOOR_OPENING,
  DOOR_OPEN,
  DOOR_CLOSING
};

#endif // APP_TYPES_H

