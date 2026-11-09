/****************************************************************************
 * Filename:    DSPSignals.h
 * Description: DSPSignals.h from webbase-DSP-system
 * Author:      Luca Echterhof
 * Year:        2025
 * License:     GPL-3.0 License
 ****************************************************************************/

#ifndef DSPSIGNALS_H
#define DSPSIGNALS_H

#include <stdint.h> // Bib für Datentypen
#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "IO.h" // Include the My IO library to handle gloabl data

#define TWO_POW_24 16777216 // 2^24 = 16777216 für 8.24 Umwandlung

typedef int32_t fixed_8_24; // Datentyp für fixpointwerte

extern SPISettings spiSettings;

//=================
// DSP SetUp

#define SPI_CS_PIN 5    // Chip Select Pin
#define SPI_CLK_PIN 18  // Clock Pin (HSPI)
#define SPI_MISO_PIN 19 // MISO Pin
#define SPI_MOSI_PIN 23 // MOSI Pin
#define DSP_BYTES_ADDR 2
#define DSP_BYTES_DATA 4
#define DSP_ADDR_READ 1
#define DSP_ADDR_WRITE 0
#define DSP_NYQUIST_LIMIT 48000

// DSP Adressen

namespace DSPAddresses
{
    // Safeload
    const uint16_t Safeload_Data0 = 24;  // Addresse für Data im Safeload
    const uint16_t Safeload_Data1 = 25;  // Addresse für Data im Safeload
    const uint16_t Safeload_Data2 = 26;  // Addresse für Data im Safeload
    const uint16_t Safeload_Data3 = 27;  // Addresse für Data im safeload
    const uint16_t Safeload_Data4 = 28;  // Addresse für Data im safeload
    const uint16_t Safeload_Adress = 29; // Adresse für ZielAdresse im Safeload
    const uint16_t Safeload_Number = 30; // Adresse für Number und Trigger register

    // Filter (Parametric EQ, EQS300MultiDPHWSlewP1)
    // Pro Filter und Kanal wird nur die Adresse von B2 benötigt: B1, B0, A2, A1 liegen direkt dahinter (B2+1 ... B2+4).
    // Quelle: Sigma Studio Export ../dsp/ParamAddress.dat (Targ_B2_1 und _slewmode).
    // Für einen Filter ohne DSP-Block DSP_ADDR_UNASSIGNED eintragen, dann wird für ihn nichts zum DSP geschrieben.
    constexpr FilterDSPAddresses Filters[NUM_FILTERS] = {
        // B2 Ch1, Slew Ch1, B2 Ch2, Slew Ch2
        {24732, 42, 24737, 48},   // Filter 1 (Ch1 Alg3  / Ch2 Alg5)
        {24742, 54, 24747, 60},   // Filter 2 (Ch1 Alg1  / Ch2 Alg6)
        {24752, 66, 24757, 72},   // Filter 3 (Ch1 Alg2  / Ch2 Alg7)
        {24762, 78, 24767, 84},   // Filter 4 (Ch1 Alg4  / Ch2 Alg8)
        {24777, 96, 24772, 90},   // Filter 5 (Ch1 Alg13 / Ch2 Alg9)
        {24787, 108, 24782, 102}, // Filter 6 (Ch1 Alg16 / Ch2 Alg10)
        {24797, 120, 24792, 114}, // Filter 7 (Ch1 Alg15 / Ch2 Alg11)
        {24807, 132, 24802, 126}, // Filter 8 (Ch1 Alg14 / Ch2 Alg12)
    };

    // Mute Ch1 (MuteHWSlewAlgADAU145X1)
    constexpr uint16_t Ch1_Mute = 141;
    constexpr uint16_t Ch1_Mute_SlewMode = 140;
    // Mute Ch2 (MuteHWSlewAlgADAU145X2)
    constexpr uint16_t Ch2_Mute = 144;
    constexpr uint16_t Ch2_Mute_SlewMode = 143;
    // Pre Gain Ch1 (HWGainADAU145XAlg2)
    constexpr uint16_t Ch1_PreGain = 35;
    constexpr uint16_t Ch1_PreGain_SlewMode = 36;
    // Pre Gain Ch2 (HWGainADAU145XAlg3)
    constexpr uint16_t Ch2_PreGain = 32;
    constexpr uint16_t Ch2_PreGain_SlewMode = 33;
    // Post Gain Ch1 (HWGainADAU145XAlg1)
    constexpr uint16_t Ch1_PostGain = 137;
    constexpr uint16_t Ch1_PostGain_SlewMode = 138;
    // Post Gain Ch2 (HWGainADAU145XAlg4)
    constexpr uint16_t Ch2_PostGain = 134;
    constexpr uint16_t Ch2_PostGain_SlewMode = 135;
    // Meter (SingleBandLevelDetS3001 / S3002, Wert X)
    constexpr uint16_t Ch1_MeterL = 169;
    constexpr uint16_t Ch2_MeterR = 155;

}

void handleNewDatafromWebsite();
void handleFilterUpdate(GlobalData::Filter &filter, uint8_t filterNumber);
void calculateFilterCoefficients(GlobalData::Filter &filter);
void dspWriteRegister_8_24(uint16_t const address, float const data);
void dspWriteRegisterFixPoint(uint16_t const address, uint32_t const data);
void dspWriteSafeloadAdressRegister(uint16_t addressRegister);
void dspInitForSPI();
float dspReadRegister(uint16_t const address);
fixed_8_24 floatToFixed8_24(float value);
float fixed8_24ToFloat(fixed_8_24 num);
float mapFrequenz(float freq);
float unmapFrequenz(float num);
float dBToFloat(float dB);
float floatTodB(float num);

#endif
