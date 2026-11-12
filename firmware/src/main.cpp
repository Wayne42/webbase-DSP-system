/****************************************************************************
 * Filename:    main.cpp
 * Description: main.cpp from webbase-DSP-system
 * Author:      Luca Echterhof
 * Year:        2025
 * License:     GPL-3.0 License
 ****************************************************************************/

#include <Arduino.h>          // Include the Arduino core library
#include <WiFi.h>             // Include the WiFi library
#include "WebserverHandler.h" // Include the My WebServer library to handle HTTP requests
#include "IO.h"               // Include the My IO library to handle input/output operations like SPIFFS
#include "DSPSignals.h"       // Include the My DSP signals library to handle DSP signals for Adau

#include "credentials.h" // WLAN credentials

#define BAUDRATE 9600 // Define the baud rate for serial communication
#define WIFI_TIMEOUT_MS 15000 // 15 Sekunden Verbindungstimeout für das Heim-WLAN

GlobalData *globalData; // Declare a pointer to the GlobalData class to access global data


// Funktion versucht sich ins Heim-WLAN einzuklinken. Gibt true bei Erfolg zurück.
bool connectToHomeWiFi() 
{
  Serial.print("Versuche Verbindung mit Heim-WLAN: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA); // Station Mode: ESP32 verbindet sich mit einem Router
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  unsigned long startAttemptTime = millis();
  
  // Warte bis verbunden oder Timeout erreicht
  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime < WIFI_TIMEOUT_MS)) 
  {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) 
  {
    Serial.println("\n[WLAN] Erfolgreich mit Heimnetzwerk verbunden!");
    Serial.print("[WLAN] IP-Adresse des ESP32: ");
    Serial.println(WiFi.localIP());
    return true;
  } 
  else 
  {
    Serial.println("\n[WLAN] Verbindung fehlgeschlagen (Timeout).");
    return false;
  }
}


void setup()
{
  Serial.begin(BAUDRATE);                 
  globalData = GlobalData::getInstance(); 
  if (globalData == nullptr)              
  {
    Serial.println("Fehler beim Erstellen der GlobalData-Instanz NullPointer"); 
    return;                                                                     
  }
  mountSPIFFS(); // Mount the SPIFFS filesystem to access files

  // --- SMART WIFI SELECTION ---
  bool isConnected = connectToHomeWiFi();

  if (isConnected) 
  {
    // ERFOLG: Wir sind im Heim-WLAN -> mDNS aktivieren
    if (!MDNS.begin("esp32")) 
    {
      Serial.println("Fehler beim Einrichten von mDNS!");
    } 
    else 
    {
      Serial.println("mDNS gestartet. Erreichbar unter: http://esp32.local");
      MDNS.addService("http", "tcp", HTTP_PORT);
      MDNS.addService("ws", "tcp", SOCKET_PORT);
    }
  } 
  else 
  {
    // FALLBACK: Keine Verbindung möglich -> Alten Access Point Modus starten
    Serial.println("[FALLBACK] Starte den integrierten Access Point...");
    setupAccesPoint(); // Ruft deine originale Funktion aus der IO.cpp auf
  }
  // -----------------------------

  setupWebSocketServer();
  setupHTTPWebServer(); // Set up the web httpServer to handle HTTP requests
  dspInitForSPI();      // Initialize the DSP for SPI communication
}

void loop()
{
  handleNewDatafromWebsite(); // Check for new data from the website and update the DSP register if needed
  webSocketsendMeter();       // Send Meter Data with sendIntervall_ms
  httpServer.handleClient();  // Handle incoming HTTP requests from clients connected to the web httpServer
  webSocketServer.loop();
  // Serial.println(floatTodB(sqrt(dspReadRegister(117))));
}
