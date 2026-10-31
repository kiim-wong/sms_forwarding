#include "globals.h"

Config config;
Preferences preferences;
PDU pdu = PDU(4096);
WebServer server(80);
bool configValid = false;
bool timeSynced = false;
bool modemReady = false;
bool webLongOperationActive = false;
unsigned long lastPrintTime = 0;
ConcatSms concatBuffer[MAX_CONCAT_MESSAGES];
