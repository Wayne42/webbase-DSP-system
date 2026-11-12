/****************************************************************************
 * Filename:    WebserverHandler.h
 * Description: WebserverHandler.h from webbase-DSP-system
 * Author:      Luca Echterhof
 * Year:        2025
 * License:     GPL-3.0 License
 ****************************************************************************/

#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#include <WebServer.h>
#include <Arduino.h>
#include <WebSocketsServer.h>
#include "IO.h"
#include "DSPSignals.h"

#define HTTP_PORT 80
#define SOCKET_PORT 81

extern WebSocketsServer webSocketServer;
extern WebServer httpServer;

void setupWebSocketServer(void);
void setupHTTPWebServer(void);
void serveFile(const char *path, const char *contentType);
void handleRootHTML();
void handleRootCSS();
void handleRootJS();
void handleNotFound();
void handleMute();
void handleFilter(uint8_t index);       // index 0 ... NUM_FILTERS-1
void handleFilterBypass(uint8_t index); // index 0 ... NUM_FILTERS-1
void handlePreGain();
void handlePostGain();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void webSocketsendMeter();
#endif
