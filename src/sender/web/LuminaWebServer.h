// HTTP control API and page (port 80), plus the loop/HTTP timing it reports.
#pragma once

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

extern uint32_t lastLoopUs;
extern uint32_t maxLoopUs;
extern uint32_t lastHttpUs;
extern uint32_t maxHttpUs;
extern uint32_t httpRequestCount;
extern uint32_t lastLiveBytes;

void setup_web_server();
