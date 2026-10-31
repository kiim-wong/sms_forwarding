#ifndef MODEM_HTTP_H
#define MODEM_HTTP_H

#include "globals.h"

struct ModemHttpResponse {
  bool gotResponse;
  bool requestSent;
  int statusCode;
  String headers;
  String body;
  String error;
};

bool modemDataBegin(bool& activatedByUs);
void modemDataEnd(bool activatedByUs);
bool modemHttpRequest(const String& url, bool isGet, const String& contentType,
                      const String& body, ModemHttpResponse& response);

#endif
