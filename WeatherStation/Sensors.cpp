/** \file Sensors.cpp
 *  \brief Sensors functions source
 *  \details File contains implementation of Sensors support functions used for Semestral Project: "Design of a secure data transmission system in NB-IoT environment".
 *  \author Bc. Daniel Kluka
 *  \version 2024
 *  \attention Project available at: https://github.com/danielkluka/Semestral-Project
 *	\attention File Sensors.cpp is in late stage of development.
 *  $Id$: Sensors.cpp 3 2024-12-10 00:22:00Z xkluka00 $
 */

#include "Adafruit_VEML7700.h"  // https://github.com/adafruit/Adafruit_VEML7700
#include "Wire.h"               // part of Arduino Core, no need to download
#include "OneWire.h"            // https://github.com/PaulStoffregen/OneWire
#include "DallasTemperature.h"  // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include "SdsDustSensor.h"      // https://github.com/lewapek/sds-dust-sensors-arduino-library
#include "Sensors.h"            // File contains Sensors function definitions used for Semestral Project: "Design of a secure data transmission system in NB-IoT environment".
#include <cstdint>


// initialization variables declarations
int retries = 0;
int max_retries = 5;


// sensor initialization functions
/*	Air temperature sensor DS18B20 initialization function
 *	author Kluka
 */
bool dsInitialize(bool& dsInit, DallasTemperature& sensor) {
    retries = 0;

    sensor.begin();

    while (retries < max_retries) {
        if (sensor.getDeviceCount()) {
            dsInit = true;
            break;
        }
        else {
            retries++;
#ifdef DEBUG
            Serial.println("  Failed to initialize DS18B20, attempt number: " + String(retries));
#endif
            delay(1000);
        }
    }

    if (retries == max_retries) {
#ifdef DEBUG
        Serial.println("  DS18B20 failed to initialize after " + String(max_retries) + " attempts. The system continues without the air temperature sensor as part of the measurement.");
#endif
    }

    return dsInit;
}

/*	Light intensity sensor VEML7700 initialization function
 *	author Kluka
 */
bool vemlInitialize(bool& vemlInit, Adafruit_VEML7700& veml) {
    retries = 0;

    while (retries < max_retries) {
        if (veml.begin()) {
            veml.setGain(VEML7700_GAIN_1_8);
            veml.setIntegrationTime(VEML7700_IT_25MS);

            vemlInit = true;
            break;
        }
        else {
            retries++;
#ifdef DEBUG
            Serial.println("  Failed to initialize VEML7700, attempt number: " + String(retries));
#endif
            delay(1000);
        }
    }

    if (retries == max_retries) {
#ifdef DEBUG
        Serial.println("  VEML7700 failed to initialize after " + String(max_retries) + " attempts. The system continues without the light intensity sensor as part of the measurement.");
#endif
    }

    return vemlInit;
}

/*	Air quality sensor SDS011 initialization function
 *	author Kluka
 */
bool sdsInitialize(bool& sdsInit, int& SDS_RX, int& SDS_TX, HardwareSerial& SDS011Serial, SdsDustSensor& sds) {
    retries = 0;

    SDS011Serial.begin(9600, SERIAL_8N1, SDS_RX, SDS_TX); // RX = GPIO43, TX = GPIO44
    sds.begin();
    sds.setActiveReportingMode();
    sds.setContinuousWorkingPeriod();
    //sds.setCustomWorkingPeriod(1); // Nastaví pracovný cyklus na 1 minútu

    while (retries < max_retries) {
        if (SDS011Serial) {
            sdsInit = true;
            break;
        }
        else {
            retries++;
#ifdef DEBUG
            Serial.println("  Failed to initialize SDS011, attempt number: " + String(retries));
#endif
        }
        delay(1000);
    }

    if (retries == max_retries) {
#ifdef DEBUG
        Serial.println("  SDS011 failed to initialize after " + String(max_retries) + " attempts. The system continues without the air quality sensor as part of the measurement.");
#endif
    }

    return sdsInit;
}

/*	Multiparametric weather transmitter WXT536 initialization function
 *	author Kluka
 */
bool wxtInitialize(bool& wxtInit,  int& WXT_RX, int& WXT_TX, HardwareSerial& WXT536Serial) {
    retries = 0;
    
    WXT536Serial.begin(19200, SERIAL_8N1, WXT_RX, WXT_TX); // RX = GPIO43, TX = GPIO44
    delay(15000);

    while (retries < max_retries) {
        if (WXT536Serial) {
            wxtInit = true;
            break;
        }
        else {
            retries++;
#ifdef DEBUG
            Serial.println("  Failed to initialize WXT536, attempt number: " + String(retries));
#endif
        }
        delay(1000);
    }

    if (retries == max_retries) {
#ifdef DEBUG
        Serial.println("  WXT536 failed to initialize after " + String(max_retries) + " attempts. The system continues without the air quality sensor as part of the measurement.");
#endif
    }

    return wxtInit;
}


// sensor connection check functions
/*	I2C connection check function
 *	author Kluka
 */
bool checkI2CConnection(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0); // 0 means success
}


// sensor measurement functions
/*	Air temperature sensor DS18B20 measurement function
 *	author Kluka
 */
bool dsMeasure(float& tempC, DallasTemperature& sensor) {
    sensor.requestTemperatures();
    tempC = sensor.getTempCByIndex(0);

    if (tempC != DEVICE_DISCONNECTED_C) {
        // debug
    }
    else {
        return false;
    }

    return true;
}

/*	Light intensity sensor VEML7700 measurement function
 *	author Kluka
 */
bool vemlMeasure(float& lux, float& white, Adafruit_VEML7700& veml) {
    lux = veml.readLux();
    white = veml.readWhite();

    if (lux >= 0 && white >= 0) {
        // debug
    }
    else {
        return false;
    }

    return true;
}

/*	Air quality sensor SDS011 measurement function
 *	author Kluka
 */
bool sdsMeasure(float& pm25, float& pm10, SdsDustSensor& sds) {
    PmResult pm = sds.readPm();
    //PmResult pm = sds.queryPm();

    if (pm.isOk()) {
        pm25 = pm.pm25;
        pm10 = pm.pm10;
    }
    else {
        return false;
    }

    return true;
}

/*	Multiparametric weather transmitter WXT536 measurement function
 *	author Kluka
 */
bool wxtMeasure(HardwareSerial& WXT536Serial, String& receivedData, String& completeMessage, float& windDir, float& windSp, float& temp, float& humi, float& pres, float& rain, float& heaterT, float& heaterV) {
    bool success = false;

    while (WXT536Serial.available()) {
        char incomingChar = WXT536Serial.read();
        receivedData += incomingChar;

        
        if (incomingChar == '#') {
            completeMessage += receivedData;
            receivedData = "";

            if (completeMessage.indexOf("Vh=") != -1) {
#ifdef DEBUG
                Serial.println("  Full data received: " + completeMessage);
#endif
                parseWXT536Data(completeMessage, windDir, windSp, temp, humi, pres, rain, heaterT, heaterV);
                completeMessage = "";
            }
        }

        success = true;
    }

    return success;
}

/*	Multiparametric weather transmitter WXT536 data parsing function
 *	author Kluka
 */
void parseWXT536Data(String data, float& windDirData, float& windSpData, float& tempData, float& humiData, float& presData, float& rainData, float& heaterTData, float& heaterVData) {
    int startIndex = data.indexOf("R0,") + 3;
    data = data.substring(startIndex);

    if (data.indexOf("Dm=") != -1) {
        windDirData = extractValue(data, "Dm=", "#").toFloat();
    }
    if (data.indexOf("Sm=") != -1) {
        windSpData = extractValue(data, "Sm=", "#").toFloat();
    }
    if (data.indexOf("Ta=") != -1) {
        tempData = extractValue(data, "Ta=", "C").toFloat();
    }
    if (data.indexOf("Ua=") != -1) {
        humiData = extractValue(data, "Ua=", "P").toFloat();
    }
    if (data.indexOf("Pa=") != -1) {
        presData = extractValue(data, "Pa=", "H").toFloat();
    }
    if (data.indexOf("Ri=") != -1) {
        rainData = extractValue(data, "Ri=", "M").toFloat();
    }
    if (data.indexOf("Th=") != -1) {
        heaterTData = extractValue(data, "Th=", "C").toFloat();
    }
    if (data.indexOf("Vh=") != -1) {
        heaterVData = extractValue(data, "Vh=", "#").toFloat();
    }
}

/*	Multiparametric weather transmitter WXT536 data extraction function
 *	author Kluka
 */
String extractValue(String data, String key, String delimiter) {
    int startIndex = data.indexOf(key) + key.length();
    int endIndex = data.indexOf(delimiter, startIndex);
    return data.substring(startIndex, endIndex);
}
