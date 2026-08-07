#ifndef APP_DEFAULT_TOPICS_H
#define APP_DEFAULT_TOPICS_H

// Default MQTT topics (can be overridden by Preferences at runtime)
static constexpr const char *DEFAULT_X_PTOPIC = "kit/UT_26018/Homy/2F2S_MSW/X_status";
static constexpr const char *DEFAULT_Y_PTOPIC = "kit/UT_26018/Homy/2F2S_MSW/Y_status";
static constexpr const char *DEFAULT_HOUR_METER_RUNTIME_PTOPIC = "kit/UT_26018/Homy/2F2S_MSW/hour_meter_runtime";
static constexpr const char *DEFAULT_OPEN_TIME_PTOPIC = "kit/UT_26018/Homy/2F2S_MSW/door_open_time";
static constexpr const char *DEFAULT_CLOSE_TIME_PTOPIC = "kit/UT_26018/Homy/2F2S_MSW/door_close_time";
static constexpr const char *DEFAULT_ALL_STATUS_PTOPIC = "kit/UT_26018/Homy/2F2S_MSW/all_status";

static constexpr const char *DEFAULT_LISTEN_ALL_STOPIC = "kit/UT_26018/#";

#endif // APP_DEFAULT_TOPICS_H

