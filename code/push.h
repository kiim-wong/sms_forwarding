#ifndef PUSH_H
#define PUSH_H

#include "globals.h"

struct PushSummary {
  uint8_t attempted;
  uint8_t succeeded;
  bool networkAvailable;
  bool hasEnabledChannel;
  uint8_t channelCount;
  PushChannelResult channels[MAX_PUSH_CHANNELS];
};

PushSummary sendSMSToServer(const char* sender, const char* message, const char* timestamp);
bool sendToChannel(const PushChannel& channel, const char* sender, const char* message,
                   const char* timestamp, bool testMode = false);
String urlEncode(const String& str);
String jsonEscape(const String& str);
String formatSmsTimestamp(const String& rawTimestamp);
String dingtalkSign(const String& secret, int64_t timestamp);
int64_t getUtcMillis();

#endif
