#ifndef HISTORY_H
#define HISTORY_H

#include <Arduino.h>
#include "config_types.h"

// 最近短信历史（最多保留 10 条，内存中按从旧到新排列）
#define MAX_FORWARD_HISTORY 10
#define HISTORY_MESSAGE_MAX 1024

enum SmsHistoryStatus : uint8_t {
  SMS_HISTORY_STATUS_FORWARDED = 0,      // 全部推送成功
  SMS_HISTORY_STATUS_PARTIAL = 1,        // 部分成功
  SMS_HISTORY_STATUS_FAILED = 2,         // 有尝试但全部失败
  SMS_HISTORY_STATUS_BLACKLISTED = 3,    // 黑名单拦截，未转发
  SMS_HISTORY_STATUS_ADMIN_COMMAND = 4,  // 管理员命令，未转发
  SMS_HISTORY_STATUS_NO_CHANNEL = 5,     // 未配置/启用有效通道
  SMS_HISTORY_STATUS_WIFI_OFF = 6,       // 旧记录：WiFi 未连接，未尝试
  SMS_HISTORY_STATUS_NO_NETWORK = 7      // 当前推送网络不可用，未尝试
};

struct ForwardHistoryEntry {
  uint32_t id;
  String sender;
  String timestamp;
  String message;
  uint8_t status;
  String reason;
  uint8_t channelCount;
  PushChannelResult channels[MAX_PUSH_CHANNELS];
  bool truncated;
};

void initForwardHistory();
void addForwardHistory(const char* sender, const char* timestamp, const char* message,
                       uint8_t status, const char* reason,
                       const PushChannelResult* channels, uint8_t channelCount);
int forwardHistoryCount();
bool deleteForwardHistory(uint32_t id);
String forwardHistoryToJson();

#endif
