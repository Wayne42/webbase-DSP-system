/****************************************************************************
 * Filename:    IO.cpp
 * Description: IO.cpp from webbase-DSP-system
 * Author:      Luca Echterhof
 * Year:        2025
 * License:     GPL-3.0 License
 ****************************************************************************/

#include "IO.h"
#include "DSPSignals.h"

//  Filter-Methode Implementierungen
bool GlobalData::Filter::getHasChanged() const { return hasChanged; }
void GlobalData::Filter::setHasChanged(bool value) { hasChanged = value; }

bool GlobalData::Filter::getHasChangedBypassed() const { return hasChangedBypass; }
void GlobalData::Filter::setHasChangedBypassed(bool value) { hasChangedBypass = value; }

uint8_t GlobalData::Filter::getBypass() const { return bypass; };
void GlobalData::Filter::setBypass(uint8_t value) { bypass = value; }

double GlobalData::Filter::getA1() const { return a1; }
void GlobalData::Filter::setA1(double value) { a1 = value; }

double GlobalData::Filter::getA2() const { return a2; }
void GlobalData::Filter::setA2(double value) { a2 = value; }

double GlobalData::Filter::getB0() const { return b0; }
void GlobalData::Filter::setB0(double value) { b0 = value; }

double GlobalData::Filter::getB1() const { return b1; }
void GlobalData::Filter::setB1(double value) { b1 = value; }

double GlobalData::Filter::getB2() const { return b2; }
void GlobalData::Filter::setB2(double value) { b2 = value; }

float GlobalData::Filter::getFreq() const { return freq; }
void GlobalData::Filter::setFreq(float value) { freq = value; }

float GlobalData::Filter::getGain() const { return gain; }
void GlobalData::Filter::setGain(float value) { gain = value; }

float GlobalData::Filter::getQ() const { return q; }
void GlobalData::Filter::setQ(float value) { q = value; }

float GlobalData::Filter::getSamplingFreq() const { return samplingFreq; }
void GlobalData::Filter::setSamplingFreq(float value) { samplingFreq = value; }

uint16_t GlobalData::Filter::getSlewMode() const { return slewMode; }
void GlobalData::Filter::setSlewMode(uint16_t value) { slewMode = value; }

void GlobalData::Filter::setDSPAddresses(const FilterDSPAddresses &addresses) { dspAddresses = addresses; }
const FilterDSPAddresses &GlobalData::Filter::getDSPAddresses() const { return dspAddresses; }
bool GlobalData::Filter::hasDSPAddresses() const { return dspAddresses.isAssigned(); }

uint8_t GlobalData::Filter::getParameterCount() const { return FILTER_PARAMETER_COUNT; }

// Schreibt die übergebenen Koeffizienten per Safeload nacheinander auf Ch1 und Ch2
void GlobalData::Filter::writeCoefficientsSafeload(double b2Value, double b1Value, double b0Value, double a2Value, double a1Value)
{
    if (!hasDSPAddresses())
    {
        Serial.println("Filter hat keine DSP-Adressen (DSP_ADDR_UNASSIGNED) -> nichts geschrieben");
        return;
    }

    auto *safeload = globalData->getSafeloadPtr();

    if (safeload == nullptr)
    {
        Serial.println("Safeloadptr is Null GlobalData::Filter::writeCoefficientsSafeload()");
        return;
    }

    const uint16_t startAddresses[] = {dspAddresses.b2Ch1, dspAddresses.b2Ch2}; // Ch1, Ch2

    for (uint16_t startAddress : startAddresses)
    {
        while (!safeload->checkSafeloadAudioFrameOver())
        {
            // bad praticese // Wait until the current audio frame is complete before writing to safeload
            delayMicroseconds(1);
        }

        //  Daten schreiben in die Safeload Register spätere Adressen für Data aufsteigend
        dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData0(), static_cast<float>(b2Value));
        dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData1(), static_cast<float>(b1Value));
        dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData2(), static_cast<float>(b0Value));
        dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData3(), static_cast<float>(a2Value));
        dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData4(), static_cast<float>(a1Value));

        // Erste Adresse übergeben wird dann intern hochgezählt bis a1 liegen hintereinander die Adressen
        dspWriteSafeloadAdressRegister(startAddress);
        // Write Number Register With Anzahl der geschrieben Daten
        dspWriteRegisterFixPoint(safeload->getDSPAdressSafeloadNumber(), FILTER_PARAMETER_COUNT);
    }
}

// Schreibt die berechneten Koeffizienten
void GlobalData::Filter::writeSafeloadRegister()
{
    writeCoefficientsSafeload(b2, b1, b0, a2, a1);
}

// Bypass: alle Koeffizienten 0 außer B0 = 1 (Durchgang)
void GlobalData::Filter::writeBypassedSafeloadRegister()
{
    writeCoefficientsSafeload(0.0, 0.0, 1.0, 0.0, 0.0);
}

// Meter Implementierungen

long GlobalData::Meter::getLastSend() { return lastSend; }
void GlobalData::Meter::setLastSend(long value) { lastSend = value; }
uint16_t GlobalData::Meter::getsendIntervall() { return sendIntervall_ms; }

void GlobalData::Meter::updateMeterValues()
{
    MeterLdB = floatTodB(sqrt(dspReadRegister(dspAdressMeterLCh1)));
    // Serial.println("Meter L:" + String(MeterLdB));
    MeterRdB = floatTodB(sqrt(dspReadRegister(dspAdressMeterRCh2)));
    // Serial.println("Meter R:" + String(MeterRdB));
}

float GlobalData::Meter::getLdB()
{
    return MeterLdB;
};
float GlobalData::Meter::getRdB()
{

    return MeterRdB;
};

char const *GlobalData::Meter::checkInfinityOrNaN(float value)
{
    if (isnan(value))
    {
        return "NaN";
    }
    else if (isinf(value))
    {
        return "inf";
    }
    else
        return nullptr;
}

String GlobalData::Meter::getDataForWebsocket()
{
    updateMeterValues();
    JsonDocument meterDoc;
    // Check infinity Or NaN with NullPointer for R
    if (const char *result = checkInfinityOrNaN(MeterLdB))
    {
        meterDoc["Left"] = result;
    }
    else
    {
        meterDoc["Left"] = MeterLdB;
    }
    // Check infinity Or NaN with NullPointer for R
    if (const char *result = checkInfinityOrNaN(MeterRdB))
    {
        meterDoc["Right"] = result;
    }
    else
    {
        meterDoc["Right"] = MeterRdB;
    }

    String stringJsonMeter;
    serializeJson(meterDoc, stringJsonMeter);
    return stringJsonMeter;
}
// Mute Methoden Implementierungen
bool GlobalData::Mute::getHasChanged() const { return hasChanged; }
void GlobalData::Mute::setHasChanged(bool value) { hasChanged = value; }

uint8_t GlobalData::Mute::getMute() const { return mute; }
void GlobalData::Mute::setMute(uint8_t value) { mute = value; }

uint16_t GlobalData::Mute::getSlewMode() const { return slewMode; }
void GlobalData::Mute::setSlewMode(uint16_t value) { slewMode = value; }

uint16_t GlobalData::Mute::getDSPAdressMuteCh1() const { return dspAdressMuteCh1; }
uint16_t GlobalData::Mute::getDSPAdressSlewMCh1() const { return dspAdressSlewMCh1; }
uint16_t GlobalData::Mute::getDSPAdressMuteCh2() const { return dspAdressMuteCh2; }
uint16_t GlobalData::Mute::getDSPAdressSlewMCh2() const { return dspAdressSlewMCh2; }
uint8_t GlobalData::Mute::getParameterCount() const { return parameterCount; }

void GlobalData::Mute::writeRegister()
{

    // Write Ch1 Mute  and Slew
    dspWriteRegister_8_24(dspAdressMuteCh1, static_cast<uint32_t>(mute));
    // dspWriteRegisterFixPoint(dspAdressSlewMCh1, static_cast<uint32_t>(slewMode));

    // Write Ch2 Mute and Slew
    dspWriteRegister_8_24(dspAdressMuteCh2, static_cast<uint32_t>(mute));
    // dspWriteRegisterFixPoint(dspAdressSlewMCh2, static_cast<uint32_t>(slewMode));
}

// Safeload Methoden Implemtierung
uint16_t GlobalData::Safeload::getDSPAdressSafeloadData0() const { return dspAdressSafeloadData0; }
uint16_t GlobalData::Safeload::getDSPAdressSafeloadData1() const { return dspAdressSafeloadData1; }
uint16_t GlobalData::Safeload::getDSPAdressSafeloadData2() const { return dspAdressSafeloadData2; }
uint16_t GlobalData::Safeload::getDSPAdressSafeloadData3() const { return dspAdressSafeloadData3; }
uint16_t GlobalData::Safeload::getDSPAdressSafeloadData4() const { return dspAdressSafeloadData4; }
uint16_t GlobalData::Safeload::getDSPAdressSafeloadAdress() const { return dspAdressSafeloadAdress; }
uint16_t GlobalData::Safeload::getDSPAdressSafeloadNumber() const { return dspAdressSafeloadNumber; }

bool GlobalData::Safeload::checkSafeloadAudioFrameOver()
{

    if ((micros() - lastSafeloadMicros) >= minAudioFrameDurationMicros)
    {
        lastSafeloadMicros = micros();
        return true;
    }
    else
        return false;
}

// Gain Methoden Implementierung
bool GlobalData::Gain::getHasChanged() const { return hasChanged; }
void GlobalData::Gain::setHasChanged(bool value) { hasChanged = value; }

float GlobalData::Gain::getGainLinear() const { return gainLinear; }
void GlobalData::Gain::setGainLinear(float value)
{
    // ist das schlau das an der stelle zu machen?
    gainLinear = value;
    gaindB = floatTodB(value);
}

float GlobalData::Gain::getGaindB() const { return gaindB; }
void GlobalData::Gain::setGaindB(float value)
{
    // ist das schlau das an der stelle zu machen?
    gaindB = value;
    gainLinear = dBToFloat(value);
}

uint16_t GlobalData::Gain::getSlewMode() const { return slewMode; }
void GlobalData::Gain::setSlewMode(uint16_t value) { slewMode = value; }

uint16_t GlobalData::Gain::getDSPAdressGainCh1() const { return dspAdressGainCh1; }

uint16_t GlobalData::Gain::getDSPAdressGainCh2() const { return dspAdressGainCh2; }

uint16_t GlobalData::Gain::getDSPAdressSlewMCh1() const { return dspAdressSlewMCh1; }

uint16_t GlobalData::Gain::getDSPAdressSlewMCh2() const { return dspAdressSlewMCh2; }

uint8_t GlobalData::Gain::getParameterCount() const { return parameterCount; }

void GlobalData::Gain::writeSafeloadRegister()
{
    auto *safeload = globalData->getSafeloadPtr();

    if (safeload == nullptr)
    {
        Serial.println("Safeloadptr is Null GlobalData::Gain::writeSafeLoadRegister()");
        return;
    }

    while (!safeload->checkSafeloadAudioFrameOver())
    {
        // bad praticese // Wait until the current audio frame is complete before writing to safeload
        delayMicroseconds(1);
    }
    // Write Filter Ch1

    //  Daten schreiben in die Safeload Register spätere Adressen für Data aufsteigend
    dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData0(), gainLinear);

    // Erste Adresse übergeben wird dann intern hochgezählt bis a2 liegen hintereinander die Adressen
    dspWriteSafeloadAdressRegister(dspAdressGainCh1);
    // Write Number Register With Anzahl der geschrieben Daten
    dspWriteRegisterFixPoint(safeload->getDSPAdressSafeloadNumber(), parameterCount);

    while (!safeload->checkSafeloadAudioFrameOver())
    {
        // bad praticese // Wait until the current audio frame is complete before writing to safeload
        delayMicroseconds(1);
    }

    // Write Filter Ch2

    //  Daten schreiben in die Safeload Register spätere Adressen für Data aufsteigend
    dspWriteRegister_8_24(safeload->getDSPAdressSafeloadData0(), gainLinear);

    // Erste Adresse übergeben wird dann intern hochgezählt bis a2 liegen hintereinander die Adressen
    dspWriteSafeloadAdressRegister(dspAdressGainCh2);
    // Write Number Register With Anzahl der geschrieben Daten
    dspWriteRegisterFixPoint(safeload->getDSPAdressSafeloadNumber(), parameterCount);
}

// Konstruktor Implementierung
GlobalData::GlobalData()
    : mute1(
          DSPAddresses::Ch1_Mute,
          DSPAddresses::Ch1_Mute_SlewMode,
          DSPAddresses::Ch2_Mute,
          DSPAddresses::Ch2_Mute_SlewMode),
      safeload(
          DSPAddresses::Safeload_Data0,
          DSPAddresses::Safeload_Data1,
          DSPAddresses::Safeload_Data2,
          DSPAddresses::Safeload_Data3,
          DSPAddresses::Safeload_Data4,
          DSPAddresses::Safeload_Adress,
          DSPAddresses::Safeload_Number),
      gainPre(
          DSPAddresses::Ch1_PreGain,
          DSPAddresses::Ch1_PreGain_SlewMode,
          DSPAddresses::Ch2_PreGain,
          DSPAddresses::Ch2_PreGain_SlewMode),
      gainPost(
          DSPAddresses::Ch1_PostGain,
          DSPAddresses::Ch1_PostGain_SlewMode,
          DSPAddresses::Ch2_PostGain,
          DSPAddresses::Ch2_PostGain_SlewMode),
      meter(
          DSPAddresses::Ch1_MeterL,
          DSPAddresses::Ch2_MeterR)
{
    // DSP-Adressen aus der Tabelle in DSPSignals.h den Filtern zuweisen
    for (uint8_t i = 0; i < NUM_FILTERS; i++)
    {
        filters[i].setDSPAddresses(DSPAddresses::Filters[i]);
    }
}

// Singleton und Objektzugriff Implementierungen
GlobalData *GlobalData::getInstance()
{
    static GlobalData instance; // Es wird nur eine Instanz des Singletons erzeugt --> instance wird nur einmal erstellt und bleibt während der gesamten Programmausführung bestehen
    return &instance;
}

GlobalData::Filter *GlobalData::getFilterPtr(uint8_t index)
{
    if (index >= NUM_FILTERS)
    {
        return nullptr;
    }
    return &filters[index];
}

GlobalData::Meter *GlobalData::getMeterPtr()
{
    return &meter;
}

GlobalData::Mute *GlobalData::getMute1Ptr()
{
    return &mute1;
}

GlobalData::Safeload *GlobalData::getSafeloadPtr()
{
    return &safeload;
}

GlobalData::Gain *GlobalData::getGainPrePtr()
{
    return &gainPre;
}

GlobalData::Gain *GlobalData::getGainPostPtr()
{
    return &gainPost;
}

// Mount SPIFFS
void mountSPIFFS(void)
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("Fehler beim Mounten des SPIFFS-Dateisystems");
        return;
    }
}

// Set ESP 32 as Access Point
void setupAccesPoint(void)
{
    // get the SSID and password from the JSON config file
    String ssid = readFromJSON(PATH_CONFIG, JSON_KEY_SSID);
    String password = readFromJSON(PATH_CONFIG, JSON_KEY_PW);
    // get static IP from JSON File
    String stringIP = readFromJSON(PATH_CONFIG, JSON_KEY_IP);
    String stringIPGateway = readFromJSON(PATH_CONFIG, JSON_KEY_IPGATEWAY);
    String stringSubnetmask = readFromJSON(PATH_CONFIG, JSON_KEY_SUBNETMASK);
    String wifiChannels = readFromJSON(PATH_CONFIG, JSON_KEY_WIFICHANNEL);
    String wifiClients = readFromJSON(PATH_CONFIG, JSON_KEY_WIFICLIENTS);
    String wifiHidden = readFromJSON(PATH_CONFIG, JSON_KEY_WIFIHIDDEN);

    // need this IP object for WiFi.softAPConfig
    IPAddress ip,
        ipGateway, subnetmask;

    // Use the IPAddress function to get an IPAdress object from a string
    if (!ip.fromString(stringIP))
    {
        Serial.println("Fehler beim Parsen des IP Strings von der JSON Datai");
        return;
    }

    if (!ipGateway.fromString(stringIPGateway))
    {
        Serial.println("Fehler beim Parsen des IP Gateway Strings von der JSON Datai");
        return;
    }

    if (!subnetmask.fromString(stringSubnetmask))
    {
        Serial.println("Fehler beim Parsen des Subnetmask Strings von der JSON Datai");
        return;
    }

    // configurate the static IP
    WiFi.softAPConfig(ip, ipGateway, subnetmask);

    // check Acces Point
    if (!WiFi.softAP(ssid, password, wifiChannels.toInt(), wifiHidden.toInt(), wifiClients.toInt()))
    {
        Serial.println("Fehler beim Starten des Access Points");
        return; // terminate function
    }

    // Print
    Serial.print("IP-Adresse: ");
    Serial.println(WiFi.softAPIP());

    // get the mDNS name from the JSON config file
    String mdnsName = readFromJSON(PATH_CONFIG, JSON_KEY_MDNS);
    if (!MDNS.begin(mdnsName)) // Set mDNS name to esp32.local and start mDNS responder and check
    {
        Serial.println("Fehler beim Starten von mDNS");
        return;
    }
    Serial.println("mDNS gestartet: " + mdnsName + ".local"); // Print mDNS name
}

// Read file from SPIFFS and return a unique pointer to the data
std::unique_ptr<char[]> fileHandlerReadSPIFFS(const String &path)
{
    // oppen file for reading
    File file = SPIFFS.open(path, "r");
    if (!file)
    {
        return nullptr; // null pointer if file not found
    }

    size_t fileSize = file.size(); // get file size

    // allocate memory for the buffer as a unique pointer named buffer with keyword new as char array
    std::unique_ptr<char[]> buffer(new char[fileSize + 1]); //+1 for null termination
    if (!buffer)                                            // check allocation
    {
        Serial.println("Fehler: Memory allokation fehlgeschlagen. (FileHandlerReadSPIFFS buffer)");
        file.close();
        return nullptr; // null pointer if memory allocation fails
    }

    file.readBytes(buffer.get(), fileSize); // read file content into buffer
    buffer[fileSize] = '\0';                // add null termination

    file.close();  // close file
    return buffer; // return unique pointer to buffer with file content
}

// Read JSON file from SPIFFS
// Problem: you can only read string not efficient when only integers needed
String readFromJSON(const String &path, const String &jsonKey)
{
    std::unique_ptr<char[]> data = fileHandlerReadSPIFFS(path); // get pointer to the file data

    if (!data) // check if file was read successfully
    {
        Serial.println("Fehler: Json-File konnte nicht gelesen werden.(nullptr)");
        return ""; // return empty string if file not found
    }

    // jsonDoc is a JsonDocument object that will be used to parse the JSON data
    JsonDocument jsonDoc;

    // Parse the JSON data from the file into the jsonDoc object
    DeserializationError error = deserializeJson(jsonDoc, data.get());
    if (error)
    {
        Serial.print("Fehler beim Parsen von JSON: ");
        Serial.println(error.c_str()); // print error message from jsonDoc library
        return "";
    }

    // Data from jsonDOC have null termination
    // get the value associated with the specified key from the JSON document and convert it to a String
    String jsonString = jsonDoc[jsonKey.c_str()].as<String>();

    // Serial.println(jsonString);

    return jsonString;
}
