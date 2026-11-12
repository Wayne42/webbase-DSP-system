/****************************************************************************
 * Filename:    Webserver.cpp
 * Description: Webserver.cpp from webbase-DSP-system
 * Author:      Luca Echterhof
 * Year:        2025
 * License:     GPL-3.0 License
 ****************************************************************************/

#include "WebserverHandler.h"
#include "IO.h" // Include the My IO library to handle gloabl data

// create Webservers object with port number
WebServer httpServer(HTTP_PORT);
WebSocketsServer webSocketServer(SOCKET_PORT);

void setupWebSocketServer(void)
{
    webSocketServer.begin();
    webSocketServer.onEvent(webSocketEvent);
    Serial.println("WebSocket is running");
}

// Define the paths for the HTML, CSS, and JS files
void setupHTTPWebServer(void)
{
    // Define Server Routes
    httpServer.on("/", HTTP_GET, handleRootHTML);                    // Route for the main page GET request
    httpServer.on("/main.css", HTTP_GET, handleRootCSS);             // Route for the CSS file GET request
    httpServer.on("/app.js", HTTP_GET, handleRootJS);                // Route for the JS file GET request
    httpServer.onNotFound(handleNotFound);                           // Route for handling 404 errors (page not found)
    httpServer.on("/mute", HTTP_POST, handleMute);                   // Route for handling POST requests to "/messages" --> Message from Client to Server
    // Routes for all filters: /filter1 ... /filterN and /bypassFilter1 ... /bypassFilterN
    for (uint8_t i = 0; i < NUM_FILTERS; i++)
    {
        const String filterNumber = String(i + 1);
        httpServer.on("/filter" + filterNumber, HTTP_POST, [i]()
                      { handleFilter(i); }); // POST from Client: freqN, gainN, qN
        httpServer.on("/bypassFilter" + filterNumber, HTTP_POST, [i]()
                      { handleFilterBypass(i); }); // POST from Client: bypassN
    }
    httpServer.on("/preGain", HTTP_POST, handlePreGain);
    httpServer.on("/postGain", HTTP_POST, handlePostGain);

    httpServer.begin(); // Start the httpServer
    // Debbug Print
    Serial.println("Webserver is running");
}

// Handle the root path ("/") and send the HTML file to the client
void handleRootHTML()
{
    File HTMLIndex = SPIFFS.open(PATH_HTML_INDEX, "r");

    if (!HTMLIndex || HTMLIndex.isDirectory())
    {
        httpServer.send(404, "text/plain", "File HTML Index not found");
        return;
    }

    size_t delivertBytes = httpServer.streamFile(HTMLIndex, "text/html", 200);
    HTMLIndex.close();

    if (delivertBytes == 0)
    {
        Serial.println("Fehler: Datei HTML Index konnte nicht gesendet werden.");
        httpServer.send(500, "text/plain", "Internal Server Error");
    }

    /*  // Get the HTML file from SPIFFS and send it to the client
     std::unique_ptr<char[]> data = fileHandlerReadSPIFFS(PATH_HTML_INDEX);

     if (data.get()) // Check if the data is not null
     {
         // hier fehlt die Server Nachricht

         httpServer.send(200, "text/html", data.get()); // Send the HTML file to the client with a 200 OK status and content type "text/html"

         // Serial.println("Succes HTML");
     }
     else

         return; */
}

// Handle the CSS file request and send it to the client
void handleRootCSS()
{

    File CSSMain = SPIFFS.open(PATH_CSS_MAIN, "r");

    if (!CSSMain || CSSMain.isDirectory())
    {
        httpServer.send(404, "text/plain", "File Main CSS not found");
        return;
    }

    size_t delivertBytes = httpServer.streamFile(CSSMain, "text/css", 200);
    CSSMain.close();

    if (delivertBytes == 0)
    {
        Serial.println("Fehler: Datei CSS Main konnte nicht gesendet werden.");
        httpServer.send(500, "text/plain", "Internal Server Error");
    }

    /*
    // Get the CSS file from SPIFFS and send it to the client
     std::unique_ptr<char[]> data = fileHandlerReadSPIFFS(PATH_CSS_MAIN);

    if (data.get()) // Check if the data is not null
    {
        // Send the CSS file to the client with a 200 OK status and content type "text/css"
        httpServer.send(200, "text/css", data.get());

        // Serial.println("Succes CSS");
    }
    else

        return; */
}

// Handle the JS file request and send it to the client
void handleRootJS()
{

    File JSRoot = SPIFFS.open(PATH_JS_APP, "r");

    if (!JSRoot || JSRoot.isDirectory())
    {
        httpServer.send(404, "text/plain", "File JS Root not found");
        return;
    }

    size_t delivertBytes = httpServer.streamFile(JSRoot, "application/javascript", 200);
    JSRoot.close();

    if (delivertBytes == 0)
    {
        Serial.println("Fehler: Datei JS Root konnte nicht gesendet werden.");
        httpServer.send(500, "text/plain", "Internal Server Error");
    }

    /*
       // Get the JS file from SPIFFS and send it to the client
       std::unique_ptr<char[]> data = fileHandlerReadSPIFFS(PATH_JS_APP);

       if (data.get()) // Check if the data is not null
       {

           httpServer.send(200, "application/javascript", data.get()); // Send the JS file to the client with a 200 OK status and content type "application/javascript"

           // Serial.println("Succes JS");
       }
       else

           return; */
}

// Handle 404 errors (page not found) and send a response to the client
void handleNotFound()
{

    Serial.print("Unbekannter Pfad: ");
    Serial.println(httpServer.uri());
    httpServer.send(404, "text/plain", "404 Not Found");
}
// Handle POST requests to "/mute" and update the mute value in the global data class
void handleMute()
{
    // Prüfe, ob der HTTP-Body Daten enthält
    if (httpServer.hasArg("mute"))
    {
        String postData = httpServer.arg("mute"); // Hole den POST-Body (Rohdaten)
        // Debug Prints
        // Serial.println("Data from Mute: " + postData); // Print the received data for debugging

        // Convert String to float
        uint8_t websiteDataMute1 = static_cast<uint8_t>(postData.toInt());

        // Flag Setzten und Daten schreiben in die Class
        auto *mute1 = globalData->getMute1Ptr(); // Pointer to the oszillator struct

        if (mute1 != nullptr) // Check if the pointer is not null
        {
            mute1->setMute(websiteDataMute1); // Set the mute values in the global data class
            mute1->setHasChanged(true);       // Set the flag for the oszillator

            if (mute1->getMute() == 0)
            {
                httpServer.send(200, "text/plain", "Mute On"); // Send a 200 OK response with a plain text message "Mute On"
            }
            else if (mute1->getMute() == 1)
            {
                httpServer.send(200, "text/plain", "Mute Off"); // Send a 200 OK response with a plain text message "Mute Off"
            }
        }
        else
        {
            Serial.println("Fail: Mute-Pointer is null form handle Mute"); // Print error message if pointer is null
            httpServer.send(500, "text/plain", "Fehler: Mute pointer ist null");
        }
    }
    else
    {
        httpServer.send(400, "text/plain", "Fehler: Argument mute fehlt");
    }
}

// Response text with the current filter values
static String filterValuesText(const GlobalData::Filter &filter)
{
    return "Frequenz: " + String(filter.getFreq()) + " Gain: " + String(filter.getGain()) + " Q: " + String(filter.getQ());
}

// Handle POST requests to "/filterN" and update the filter values in the global data class
// index is 0-based, the website uses 1-based numbers (freq1, gain1, q1 ...)
void handleFilter(uint8_t index)
{
    const String filterNumber = String(index + 1);
    const String argFreq = "freq" + filterNumber;
    const String argGain = "gain" + filterNumber;
    const String argQ = "q" + filterNumber;

    if (!(httpServer.hasArg(argFreq) && httpServer.hasArg(argGain) && httpServer.hasArg(argQ))) // check if HTTP Body has Data
    {
        httpServer.send(400, "text/plain", "Fehler: Argumente für Filter" + filterNumber + " fehlen");
        Serial.println("Fail: Website Post Data für Filter" + filterNumber + " fehlt ");
        return;
    }

    auto *filter = globalData->getFilterPtr(index); // Pointer to the filter class

    if (filter == nullptr) // Check if the pointer is not null
    {
        httpServer.send(500, "text/plain", "Fehler: Filter" + filterNumber + " pointer ist null");
        Serial.println("Fail: Filter" + filterNumber + "-Pointer is null form handleFilter");
        return;
    }

    // Convert Strings and write the values to the global data class
    filter->setFreq(httpServer.arg(argFreq).toFloat());
    filter->setGain(httpServer.arg(argGain).toFloat());
    filter->setQ(httpServer.arg(argQ).toFloat());
    filter->setHasChanged(true); // Set the Flag

    if (!filter->hasDSPAddresses())
    {
        httpServer.send(200, "text/plain", filterValuesText(*filter) + " (Filter" + filterNumber + " hat noch keine DSP-Adressen)");
        return;
    }
    httpServer.send(200, "text/plain", filterValuesText(*filter));
}

// Handle POST requests to "/bypassFilterN"
void handleFilterBypass(uint8_t index)
{
    const String filterNumber = String(index + 1);
    const String argBypass = "bypass" + filterNumber;

    if (!httpServer.hasArg(argBypass))
    {
        httpServer.send(400, "text/plain", "Fehler: Argument bypassed Filter " + filterNumber + " fehlt");
        return;
    }

    auto *filter = globalData->getFilterPtr(index); // Pointer to the Filter class

    if (filter == nullptr) // Check if the pointer is not null
    {
        httpServer.send(500, "text/plain", "Fehler: Filter" + filterNumber + " pointer ist null");
        Serial.println("Fail: Filter" + filterNumber + "-Pointer is null form handleFilterBypass");
        return;
    }

    filter->setBypass(static_cast<uint8_t>(httpServer.arg(argBypass).toInt())); // Set the Filter values in the global data class
    filter->setHasChangedBypassed(true);                                       // Set the flag for the Filter

    if (filter->getBypass() == 1)
    {
        httpServer.send(200, "text/plain", "Filter Bypassed");
    }
    else
    {
        httpServer.send(200, "text/plain", filterValuesText(*filter));
    }
}

void handlePreGain()
{

    if (httpServer.hasArg("preGain"))
    {
        String postData = httpServer.arg("preGain");
        float websiteDataGain = postData.toFloat();

        auto *gainPre = globalData->getGainPrePtr();
        if (gainPre != nullptr)
        {
            gainPre->setGaindB(websiteDataGain);
            gainPre->setHasChanged(true);

            httpServer.send(200, "text/plain", "Pre Gain: " + String(gainPre->getGaindB()) + "dB");
        }
        else
        {
            httpServer.send(500, "text/plain", "Fehler: GainPost pointer ist null");
            Serial.println("Fail: GainPre-Pointer is null form handle PreGain"); // Print error message if pointer is null
        }
    }
    else
    {
        httpServer.send(400, "text/plain", "Fehler: Argument postGain fehlt");
    }
}

void handlePostGain()
{
    if (httpServer.hasArg("postGain"))
    {
        String postData = httpServer.arg("postGain");
        float websiteDataGain = postData.toFloat();

        auto *gainPost = globalData->getGainPostPtr();
        if (gainPost != nullptr)
        {
            gainPost->setGaindB(websiteDataGain);
            gainPost->setHasChanged(true);

            httpServer.send(200, "text/plain", "Post Gain: " + String(gainPost->getGaindB()) + "dB");
        }
        else
        {
            httpServer.send(500, "text/plain", "Fehler: GainPost pointer ist null");
            Serial.println("Fail: GainPost-Pointer is null form handle PostGain");
        }
    }
    else
    {
        httpServer.send(400, "text/plain", "Fehler: Argument postGain fehlt");
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[%u] Disconnected!\n", num);
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = webSocketServer.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        // send message to client
        webSocketServer.sendTXT(num, "Connected");
    }
    break;
    case WStype_TEXT:
        Serial.printf("[%u] get Text: %s\n", num, payload);

        // send message to client
        // webSocket.sendTXT(num, "message here");

        // send data to all connected clients
        // webSocket.broadcastTXT("message here");
        break;
    case WStype_BIN:
        Serial.printf("[%u] get binary length: %u\n", num, length);
        // hexdump(payload, length);

        // send message to client
        // webSocket.sendBIN(num, payload, length);
        break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
        break;
    }
}

void webSocketsendMeter()
{

    auto *meter = globalData->getMeterPtr();

    if (meter == nullptr)
    {
        // Client bekommt davon nichts mit
        Serial.print("MeterPointer was null from webSocketsendMeter");
        return;
    }

    if (millis() - meter->getLastSend() > meter->getsendIntervall())
    {
        String json = meter->getDataForWebsocket();
        webSocketServer.broadcastTXT(json.c_str());

        meter->setLastSend(millis());
    };
}