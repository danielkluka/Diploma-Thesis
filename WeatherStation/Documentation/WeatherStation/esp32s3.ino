/** \file       esp32s3.ino
 *  \brief      Main source of ESP32-S3 DevKitC-1 WROOM-1 N16R8
 *  \details    Main source for Diploma Thesis: "Design of a secure data transmission system in NB-IoT environment".
 *  \attention  Diploma Thesis available at: https://github.com/danielkluka/Diploma-Thesis
 *  \attention  Diploma Thesis source code has finished development
 *  \author     Bc. Daniel Kluka
 *  \version    2024
 *  $Id: esp32s3.ino XXXX 2025-05-27 03:41:30Z danielkluka $
 */

#include "Adafruit_VEML7700.h"        // https://github.com/adafruit/Adafruit_VEML7700
#include "Wire.h"                     // part of Arduino Core, no need to download
#include "OneWire.h"                  // https://github.com/PaulStoffregen/OneWire
#include "DallasTemperature.h"        // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include "SdsDustSensor.h"            // https://github.com/lewapek/sds-dust-sensors-arduino-library
#include "Sensors.h"                  // file contains Sensors function definitions used for Diploma Thesis: "Design of a secure data transmission system in NB-IoT environment".
#include <driver/adc.h>               // part of ESP-IDF framework by Espressif, no need to download
#include "esp_sleep.h"                // part of ESP-IDF framework by Espressif, no need to download
#include <stdio.h>                    // pre sscanf
#include <Quectel_BC660.h>            // https://github.com/R4sp1/Quectel-BC660



//==================================================================================================================
//=========================================== LOGIC SETUP SPACE    1/4    ==========================================
//==================================================================================================================



// GPIO pin definitions on module ESP32-S3 DevKitC-1 WROOM-1 N16R8
#define DS_OUT 4                      // DATA pin for DS18B20 in the radiation shield   do not change pin
#define SDS_RX 18                     // RX pin for SDS011                              do not change pin
#define SDS_TX 17                     // TX pin for SDS011                              do not change pin
#define WXT536_RX 16                  // RX pin for WXT536                              do not change pin
#define WXT536_TX 15                  // TX pin for WXT536                              do not change pin
#define BC660K_RX 44                  // RX pin pre Quectel BC660K                      do not change pin
#define BC660K_TX 43                  // TX pin pre Quectel BC660K                      do not change pin
#define PWR_PIN 5                     // sensor power pin
#define ADC_CHANNEL ADC1_CHANNEL_0    // voltage divider channel


// modes definitions used for debugging
//#define DEBUG                       // mode used for serial printing
//#define BC660K_DEBUG                // mode used for debugging NB-IoT module BC660K 
#define TIMESTAMP                     // mode used for create timestamp and to measure device UP time
//#define LOOP_DEEP_CONTROL           // mode used for attempt to re-initialize the sensor in case of problematic or no communication with the sensor
//#define DEBUG_BATTERY_MEASUREMENTS  // late development battery measurement values
//#define DEBUG_AVERAGE_MEASUREMENTS  // late development average measurement values


// modules and functions definitions used to virtually disconnect sensor when debugging
#define DS18B20                       // air temperature sensor
#define VEML7700                      // light intensity sensor
#define SDS011                        // air quality sensor
#define WXT536                        // multiparametric sensor
#define BC660K                        // NB-IoT module Quectel BC660K
#define BC660K_MQTT                   // communication protocol used for data transmission to server


// ESP32-S3 uses sleep logic to measure samples every 5 or 30 minutes, recharge battery for 2 hours and send data every hour
#define ESP_SLEEP 1                  // basic measurements - air temperature (DS18B20), light intensity (VEML7700), weather parameters (WXT536)
#define SDS_SLEEP 6                  // air quality measurement (SDS011)
#define RECHARGE 120                 // if battery drops below 3300 mV, weatherstation stops measuring for 2 hours to recharge
#define DATA_SEND 1                  // measured and stored data in data buffer send every hour via BC660K and MQTT to Node-Red server
#define SAMPLES 5                    // number of sensors data samples to be measured
// ESP_SLEEP, DATA_SEND and BUFFER_SIZE are connected ->  60 / 10 = 6, alternate version if not evenly divisible used in declaration space


//==================================================================================================================
//=========================================== DECLARATION SPACE    2/4    ==========================================
//==================================================================================================================



// sensors variables declarations 
#ifdef DS18B20
  OneWire oneWire(DS_OUT);
  DallasTemperature sensors(&oneWire);
  bool dsInitialized = false;
  float temperatureCReadings[SAMPLES];
  float temperatureC = 0;
  float averageTemperatureC = 0;
#endif

#ifdef VEML7700
  Adafruit_VEML7700 veml;
  bool vemlInitialized = false;
  float luxReadings[SAMPLES];
  float whiteReadings[SAMPLES];
  float lux = 0;
  float white = 0;
  float averageLux = 0;
  float averageWhite = 0;
#endif

#ifdef SDS011
  HardwareSerial SDS011Serial(1);
  SdsDustSensor sds(SDS011Serial);
  int rxPin = SDS_RX;
  int txPin = SDS_TX;
  bool sdsInitialized = false;
  float pm25Readings[SAMPLES];
  float pm10Readings[SAMPLES];
  float pm25 = 0;
  float pm10 = 0;
  float averagePm25 = 0;
  float averagePm10 = 0;
#endif

#ifdef WXT536
  int wxtRX = WXT536_RX;
  int wxtTX = WXT536_TX;
  HardwareSerial WXT536Serial(2);
  const long WXT536_BAUDRATE = 19200;
  String receivedData = "";
  String completeMessage = "";
  char incomingChar;
  bool wxtInitialized = false;
  float windDirectionReadings[SAMPLES];
  float windSpeedReadings[SAMPLES];
  float temperatureReadings[SAMPLES];
  float humidityReadings[SAMPLES];
  float pressureReadings[SAMPLES];
  float rainfallReadings[SAMPLES];
  float heaterTempReadings[SAMPLES];
  float heaterVoltageReadings[SAMPLES];
  float windDirection = 0;
  float windSpeed = 0;
  float temperature = 0;
  float humidity = 0;
  float pressure = 0;
  float rainfall = 0;
  float heaterTemp = 0;
  float heaterVoltage = 0;
  float averageWindDirection = 0;
  float averageWindSpeed = 0;
  float averageTemperature = 0;
  float averageHumidity = 0;
  float averagePressure = 0;
  float averageRainfall = 0;
  float averageHeaterTemp = 0;
  float averageHeaterVoltage = 0;  
#endif


// buffer storing data in RTC memory while ESP23-S3 is sleeping - buffer send every hour

// automatically calculated buffer size, data measured every 5 minutes - 12 times / hour
//#define BUFFER_SIZE (DATA_SEND / ESP_SLEEP)

// alternate version, round up if not evenly divisible
#define BUFFER_SIZE ((DATA_SEND + ESP_SLEEP - 1) / ESP_SLEEP)

struct Measurement {
  float batteryVoltage;
  float stateOfCharge;
  int recharged;
  unsigned long initDuration;
  unsigned long measureDuration;
  unsigned long espDuration;
  int64_t timestamp;
  #ifdef DS18B20
    float temperature;
  #endif
  #ifdef VEML7700
    float lux;
    float white;
    float chargingCurrent;
    float photonFlux;
  #endif
  #ifdef SDS011
    float pm25;
    float pm10;
  #endif
  #ifdef WXT536
    float windDirection;
    float windSpeed;
    float temperatureWXT536;
    float humidity;
    float pressure;
    float rainfall;
    float heaterTemp;
    float heaterVoltage;
  #endif
};

//int BUFFER_SIZE = 0;
RTC_DATA_ATTR Measurement dataBuffer[BUFFER_SIZE];
RTC_DATA_ATTR int bufferIndex = 0;


// main algorithm variables declarations
int sensorsCount = 0;                 // counts how many sensor are successfully initialized
//int sleepCyclesLimit = ESP_SLEEP;     // measuring data every 5 minutes
RTC_DATA_ATTR int sleepCyclesLimit = ESP_SLEEP;

RTC_DATA_ATTR int sleepCycles = 0;
//int sdsSleepCyclesLimit = SDS_SLEEP;  // measuring data every 30 minutes
RTC_DATA_ATTR int sdsSleepCyclesLimit = SDS_SLEEP;

RTC_DATA_ATTR int sdsSleepCycles = 0;
//int dataSendCyclesLimit = DATA_SEND;  // sending data every 60 minutes
RTC_DATA_ATTR int dataSendCyclesLimit = DATA_SEND;

RTC_DATA_ATTR int dataSendCycles = 0;
int rechargeCyclesLimit = RECHARGE;   // battery charging for 120 minutes
RTC_DATA_ATTR int rechargeCycles = 0;
RTC_DATA_ATTR int recharge = 0;       // battery charging flag
RTC_DATA_ATTR int recharged = 0;      // number of finished recharges

float batteryVoltage = 0.0;            // mV measured by ESP32-S3
float stateOfCharge = 0.0;            // estimated battery state of charge
float chargingCurrent = 0.0;          // estimated charging current
float photonFlux = 0.0;               // estimated charging current
long batteryVoltageReadings[SAMPLES]; // number of battery voltage samples
const float minVoltage = 3300.0;      // mV required for measurement
const float REF_VBAT = 3470.0;        // reference mV measured by multimeter
const float REF_VCCRAW_ESP32 = 3584.0;// reference mV measured by ESP32-S3
const float maxPanelPower = 4.5;      // maximum panel power [W]
const float panelVoltage = 6.0;       // maximum panel voltage [V]
#define PLANCK 6.626e-34              // J·s
#define SPEED_OF_LIGHT 3.0e8          // m/s
#define WAVELENGTH 550e-9             // 550 nm
#define SLEEP_DURATION 6000000       // 1 minute sleep - 60000000
QuectelBC660 quectel = QuectelBC660(NOT, true);
RTC_DATA_ATTR String dataSendTime = "";
bool success = false;
bool measured = false;

// TLS/SSL certificates
static const char caPem[] PROGMEM = R"PEM(-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----)PEM";

static const char clientPem[] PROGMEM = R"PEM(-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----)PEM";

static const char clientKey[] PROGMEM = R"PEM(-----BEGIN PRIVATE KEY-----

-----END PRIVATE KEY-----)PEM";


// timestamp variables declarations
#ifdef TIMESTAMP
  unsigned long powerOnTime;
  unsigned long initStartTime;
  unsigned long initEndTime;
  unsigned long initDuration;
  unsigned long measureStartTime;
  unsigned long measureEndTime;
  unsigned long measureDuration;
  unsigned long registrationStartTime;
  unsigned long registrationEndTime;
  unsigned long registrationDuration;
  RTC_DATA_ATTR unsigned long espDuration;
#endif



//==================================================================================================================
//=========================================== MAIN ALGORITHM    3/4    =============================================
//==================================================================================================================



// setup and initialization functions ======================================================================== setup
void setup() {
  // use only if in Debug mode
  #ifdef DEBUG
    Serial.begin(115200);
    Serial.print("Sleep cycles limit: ");
    Serial.println(sleepCyclesLimit);
    Serial.print("SDS011 sleep cycles limit: ");
    Serial.println(sdsSleepCyclesLimit);
    Serial.print("Data send cycles limit: ");
    Serial.println(dataSendCyclesLimit);
  #endif

  // time when device turned on
  #ifdef TIMESTAMP
    powerOnTime = millis();
  #endif

  #ifdef TIMESTAMP
    #ifdef DEBUG
      Serial.println("");
      Serial.print("Device turned on in time (ms): ");
      Serial.println(powerOnTime);
    #endif
  #endif

  #ifdef DEBUG
    Serial.println("");
    Serial.print("Wake up, sleepCycles: ");
    Serial.println(sleepCycles);
  #endif

  // measure average battery voltage of defined samples count
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
  batteryVoltage = batteryMeasure();

  if((sleepCycles == sleepCyclesLimit) && (recharge == 0) ) {

    int sensorsCount = 0;
    pinMode(PWR_PIN, OUTPUT);
    delay(1000);
  
    // automatic Buck-Boost TPS63020 requires 1.8-5.5V 2A
    if (batteryVoltage > minVoltage) {
      #ifdef DEBUG
        Serial.print("Start of sensors initialization");
      #endif

      // initialization start
      #ifdef TIMESTAMP
        initStartTime = millis();
        #ifdef DEBUG
          Serial.print(" in time (ms): ");
          Serial.println(initStartTime);
        #endif  
      #endif
  
      digitalWrite(PWR_PIN, HIGH);
      sensorsInit();

	    #ifdef DEBUG
        Serial.print("End of sensors initialization");
      #endif
  
      #ifdef TIMESTAMP
        initEndTime = millis();
        #ifdef DEBUG
          Serial.print(" in time (ms): ");
          Serial.println(initEndTime);
        #endif
        initDuration = initEndTime - initStartTime;
      #endif
  
      // measurement start
      #ifdef DEBUG
        Serial.println("");
        Serial.print("Start of measurement");
      #endif

      #ifdef TIMESTAMP
        measureStartTime = millis();
        #ifdef DEBUG
          Serial.print(" in time (ms): ");
          Serial.println(measureStartTime);
        #endif
      #endif
      
      averageMeasurements();
      
      // measurement end
      #ifdef DEBUG
        Serial.print("End of measurement");
      #endif


      #ifdef TIMESTAMP
        measureEndTime = millis();
        #ifdef DEBUG
          Serial.print(" in time (ms): ");
          Serial.println(measureEndTime);
        #endif
        measureDuration = measureEndTime - initEndTime;
      #endif

      addToBuffer();

      #ifdef DEBUG
        //debugMQTTMessage();
      #endif

      #ifdef DEBUG
        Serial.println("");
      #endif

      Serial.print("dataSendCycles:");
      Serial.println(dataSendCycles);
      Serial.print("dataSendCyclesLimit:");
      Serial.println(dataSendCyclesLimit);
      Serial.print("dataSendCycles:");
      Serial.println(dataSendCycles);
      Serial.print("bufferIndex:");
      Serial.println(bufferIndex);
      Serial.print("BUFFER_SIZE:");
      Serial.println(BUFFER_SIZE);
      if (dataSendCycles == dataSendCyclesLimit) {
        if (bufferIndex >= BUFFER_SIZE) {
          #ifdef BC660K_MQTT
            if (!bc660kMQTT()) {
              #ifdef DEBUG
                Serial.println("Failed to send data, keeping buffer.");
              #endif
            } else {
              bufferIndex = 0;
            }
          #endif

          // do not use with BC660K defined, only for debugging
          //bufferIndex = 0;
        }
      }

      digitalWrite(PWR_PIN, LOW);
    } else {
      #ifdef DEBUG
        Serial.print("Charging battery, rechargeCycles: ");
        Serial.println(rechargeCycles);
      #endif

      ++rechargeCycles;
      recharge = 1;
    }
  }                                                  

  if (sdsSleepCycles >= sdsSleepCyclesLimit) sdsSleepCycles = 0;

  if (sleepCycles >= sleepCyclesLimit) sleepCycles = 0;
  
  if (dataSendCycles >= dataSendCyclesLimit) {
    dataSendCycles = 0;
    #ifdef DEBUG
      Serial.println("Optimizing measurement frequency.");
      Serial.println("State of charge: ");
      Serial.println(stateOfCharge);
      Serial.println("Starting:");
    #endif
    if (stateOfCharge >= 80.0) {
      #ifdef DEBUG
        Serial.println("Battery is above 80%.");
      #endif
      sleepCyclesLimit = ESP_SLEEP;           // 1 minute
      dataSendCyclesLimit = DATA_SEND;        // 6 minutes
      sdsSleepCyclesLimit = SDS_SLEEP;        // 6 minutes
    } else {
      #ifdef DEBUG
        Serial.println("Battery is under 80%.");
      #endif
      sleepCyclesLimit = (ESP_SLEEP * 10);    // 10 minutes
      dataSendCyclesLimit = (DATA_SEND * 10); // 60 minutes
      sdsSleepCyclesLimit = 30;
    }
    #ifdef DEBUG
      Serial.println("Optimization finished.");
      Serial.print("Sleep cycles limit: ");
      Serial.println(sleepCyclesLimit);
      Serial.print("SDS011 sleep cycles limit: ");
      Serial.println(sdsSleepCyclesLimit);
      Serial.print("Data send cycles limit: ");
      Serial.println(dataSendCyclesLimit);
    #endif
  }

  if (recharge == 1) {
    if (rechargeCycles >= rechargeCyclesLimit) {
      rechargeCycles = 0;
      if (stateOfCharge <= 80.0) {
        recharge = 1;  
      } else {
        recharge = 0;
        ++recharged;
      }
    }
  }

  ++sleepCycles;
  ++sdsSleepCycles;
  ++dataSendCycles;

  #ifdef DEBUG
    Serial.println("Going to sleep.");
  #endif

  if (measured == true) {
    espDuration = millis();
  }

  //digitalWrite(PWR_PIN, LOW);
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
  esp_deep_sleep_start();
}



// main measurement and communication loop ==================================================================== loop 
void loop() {
  // not used
}



//==================================================================================================================
//=========================================== FUNCTION SPACE    4/4    =============================================
//==================================================================================================================



// battery function ============================================================================================ 1/13
long batteryMeasure() {
  long sumBatteryVoltage = 0;
  float R2 = 1470000.0;
  float R3 = 470000.0; 
  const float dividerRatio = R3 / (R2 + R3);
  const float offset = REF_VCCRAW_ESP32 - REF_VBAT;

  for (int i = 0; i < SAMPLES; i++) {
        int raw = adc1_get_raw(ADC_CHANNEL);
        float dividerOutput = (raw / 4095.0) * 3.6 * 1000;
        float localBatteryVoltage = (dividerOutput / dividerRatio) - offset;

        batteryVoltageReadings[i] = localBatteryVoltage;
        sumBatteryVoltage += batteryVoltageReadings[i];

        #ifdef DEBUG_BATTERY_MEASUREMENTS
            Serial.print("Reading ");
            Serial.print(i + 1);
            Serial.print(": Raw ADC: ");
            Serial.print(raw);
            Serial.print(", Voltage on divider [mV]: ");
            Serial.print(dividerOutput);
            Serial.print(", Battery voltage [mV]: ");
            Serial.println(localBatteryVoltage);
        #endif

        delay(10);
    }

    long averageBatteryVoltage = sumBatteryVoltage / SAMPLES;

    #ifdef DEBUG_BATTERY_MEASUREMENTS
        Serial.println("Vzorky:");
        for (int i = 0; i < SAMPLES; i++) {
            Serial.print("  ");
            Serial.print(batteryVoltageReadings[i]);
            Serial.println(" mV");
        }
        Serial.print("Average battery voltage: ");
        Serial.print(averageBatteryVoltage);
        Serial.println(" mV");
    #endif

    stateOfCharge = mapBatteryVoltageToSOC(averageBatteryVoltage);

    #ifdef DEBUG_BATTERY_MEASUREMENTS
      Serial.print("Estimated SOC: ");
      Serial.print(stateOfCharge);
      Serial.println(" %");
    #endif

    return averageBatteryVoltage;
}



// battery capacity ============================================================================================ 2/13
float mapBatteryVoltageToSOC(float voltage) {
  /*if (voltage >= 4200) return 100.0;
  if (voltage >= 4020) return 80.0;
  if (voltage >= 3840) return 60.0;
  if (voltage >= 3660) return 40.0;
  if (voltage >= 3480) return 20.0;
  if (voltage >= 3300) return 0.0;
  return 0.0;*/

  if (voltage <= 3300) {
    return 0.0f;
  }
  if (voltage >= 4200) {
    return 100.0f;
  }

  static const int mV_values[] = {
    3300, 3400, 3500, 3600, 3700,
    3800, 3850, 3900, 3950, 4000,
    4050, 4100, 4150, 4200
  };

  static const float soc_values[] = {
    0.0f, 10.0f, 20.0f, 30.0f, 40.0f,
    50.0f, 60.0f, 70.0f, 75.0f, 80.0f,
    85.0f, 90.0f, 95.0f, 100.0f
  };

  const int count = sizeof(mV_values) / sizeof(mV_values[0]);

  for (int i = 0; i < count - 1; i++) {
    int lowVal = mV_values[i];
    int highVal = mV_values[i + 1];

    if (voltage >= lowVal && voltage <= highVal) {
      float socLow  = soc_values[i];
      float socHigh = soc_values[i + 1];

      float ratio = (float)(voltage - lowVal) / (float)(highVal - lowVal);
      float interpolatedSOC = socLow + ratio * (socHigh - socLow);

      return roundf(interpolatedSOC);
    }
  }

  return 0.0f;
}



// charging current estimation ================================================================================= 3/13
float calculateChargingCurrent(float lux) {
  float panelPower;

  panelPower = (lux / 100000.0) * maxPanelPower;
  if (panelPower > maxPanelPower) {
    panelPower = maxPanelPower;
  }

  float chargingCurrent = (panelPower / panelVoltage) * 1000.0;
  return chargingCurrent;
}



// photon function ============================================================================================= 4/13
float calculatePhotonFlux(float lux) {
  float panelPower = (lux / 100000.0) * maxPanelPower;
  if (panelPower > maxPanelPower) {
    panelPower = maxPanelPower;
  }

  float photonEnergy = (PLANCK * SPEED_OF_LIGHT) / WAVELENGTH;

  float photonFlux = panelPower / photonEnergy; 

  return photonFlux;
}



// init function =============================================================================================== 5/13
void sensorsInit() {
  // initialization of DS18B20
  #ifdef DS18B20
    #ifdef DEBUG
      Serial.println("");
      Serial.println("  Initializing DS18B20 in radiation shield.");
    #endif

    if (dsInitialize(dsInitialized, sensors)) {
      ++sensorsCount;

      #ifdef DEBUG
        Serial.println("  DS18B20 initialized.");
      #endif  
    }
  #endif


  // initialization of VEML7700
  #ifdef VEML7700
    #ifdef DEBUG
      Serial.println("");
      Serial.println("  Initializing VEML7700.");
    #endif

    if (vemlInitialize(vemlInitialized, veml)) {
      ++sensorsCount;

      #ifdef DEBUG
        Serial.println("  VEML7700 initialized.");
      #endif
    }
  #endif


  // initialization of SDS011
  #ifdef SDS011
    #ifdef DEBUG
      Serial.println("");
      Serial.println("  Initializing SDS011.");
    #endif
    
    if (sdsInitialize(sdsInitialized, rxPin, txPin, SDS011Serial, sds)) {
      ++sensorsCount;

      #ifdef DEBUG
        Serial.println("  SDS011 initialized.");
      #endif
    }  
  #endif

  if (sdsSleepCycles < sdsSleepCyclesLimit) {
        #ifdef SDS011
          sds.sleep();
        #endif
      }


  // initialization of WXT536
  #ifdef WXT536
    #ifdef DEBUG
      Serial.println("");
      Serial.println("  Initializing WXT536.");
    #endif

    if (wxtInitialize(wxtInitialized, wxtRX, wxtTX, WXT536Serial)) {
      ++sensorsCount;

      #ifdef DEBUG
        Serial.println("  WXT536 initialized.");
      #endif
    }    
  #endif
}



// measurement function ======================================================================================== 6/13
void sensorsMeasure() {
  // attempts to re-establish communications
  #ifdef LOOP_DEEP_CONTROL
    #ifdef DS18B20
      if(!dsInitialized) {
        if(dsInitialize(dsInitialized, sensors)) {
          ++sensorsCount;
        }
      }
    #endif

    #ifdef VEML7700
      if(!vemlInitialized) {
        if(vemlInitialize(vemlInitialized, veml)) {
          ++sensorsCount;
        }
      }
    #endif

    #ifdef SDS011
      if(sdsSleepCycles == sdsSleepCyclesLimit) {
        if(!sdsInitialized) {
          if(sdsInitialize(sdsInitialized, rxPin, txPin, SDS011Serial, sds)) {
            ++sensorsCount;
          }
        }
      }
    #endif

    #ifdef WXT536
      if(!wxtInitialized) {
        if(wxtInitialize(wxtInitialized, wxtRX, wxtTX, WXT536Serial)) {
          ++sensorsCount;
        }
      }
    #endif
  #endif

  #ifdef DEBUG
    Serial.println("");
    Serial.print("  ----------------- MEASURING SAMPLE ----------------- ");
  #endif

  // air temperature measurement
  #ifdef DS18B20
    if (dsInitialized) {
      #ifdef DEBUG
        Serial.println("");
        Serial.println("  Sensor DS18B20 (air temperature)");
      #endif

      if(dsMeasure(temperatureC, sensors)) {
        #ifdef DEBUG
          Serial.print("  Temperature: ");
          Serial.print(temperatureC);
          Serial.println(" °C");
        #endif
      } else {
        #ifdef DEBUG
          Serial.println("  Unable to measure air temperature!");
        #endif

        --sensorsCount;
        dsInitialized = false;
      }
    }
  #endif


  // light intensity and white light amount measurement
  #ifdef VEML7700
    if (checkI2CConnection(0x10)) {
      #ifdef DEBUG
        Serial.println("");
        Serial.println("  Sensor VEML7700 (light intensity and amount of white light)");
      #endif

      if(vemlMeasure(lux, white, veml)) {
        #ifdef DEBUG
          Serial.print("  Luminosity [lx]: ");
          Serial.println(lux);
          Serial.print("  White luminosity: ");
          Serial.println(white);
        #endif
      } else {
        #ifdef DEBUG
          Serial.println("  Unable to measure light values!");
        #endif
        
        --sensorsCount;
        vemlInitialized = false;
      }
    }
  #endif

  // dust particles PM 2.5 and PM 10 measurement
  #ifdef SDS011
    if(sdsSleepCycles == sdsSleepCyclesLimit) {
      if (sdsInitialized) {
        #ifdef DEBUG
          Serial.println("");
          Serial.println("  Sensor SDS011 (air quality)");
        #endif

        if(sdsMeasure(pm25, pm10, sds)) {
          #ifdef DEBUG
            Serial.print("  PM 2.5 [µg/m3]: ");
            Serial.println(pm25);
            Serial.print("  PM 10 [µg/m3]: ");
            Serial.println(pm10);
          #endif
        } else {
          #ifdef DEBUG
            Serial.println("  Unable to measure dust particles values!");
          #endif
        
          --sensorsCount;
          sdsInitialized = false;
        }
      }
    }
  #endif


  // weather parameters measurement
  #ifdef WXT536
    if (wxtInitialized) {
      #ifdef DEBUG
        Serial.println("");
        Serial.println("  Multiparametric weather transmitter WXT536 (weather parameters)");
      #endif

      if(wxtMeasure(WXT536Serial, receivedData, completeMessage, windDirection, windSpeed, temperature, humidity, pressure, rainfall, heaterTemp, heaterVoltage)) {
        #ifdef DEBUG    
          Serial.print("  Wind Direction [°]: ");
          Serial.println(windDirection);
          Serial.print("  Wind Speed [m/s]: ");
          Serial.println(windSpeed);
          Serial.print("  Temperature [°C]: ");
          Serial.println(temperature);
          Serial.print("  Humidity [%]: ");
          Serial.println(humidity);
          Serial.print("  Pressure [hPa]: ");
          Serial.println(pressure); 
          Serial.print("  Rainfall Accumulation [mm]: ");
          Serial.println(rainfall);
          Serial.print("  Heater Temperature (°C): ");
          Serial.println(heaterTemp);
          Serial.print("  Heater Voltage (V): ");
          Serial.println(heaterVoltage);
        #endif
      } else {
        #ifdef DEBUG
          Serial.println("  Unable to measure weather parameters!");
        #endif

        --sensorsCount;
        wxtInitialized = false;
      }
    }
  #endif

  #ifdef DEBUG
    Serial.println("  /////////////////////// END //////////////////////// ");
  #endif
}



// average measurement function ================================================================================ 7/13
void averageMeasurements() {
  #ifdef SDS011
    if(sdsSleepCycles == sdsSleepCyclesLimit) {
      sds.wakeup();
      delay(30000);
    }
  #endif

  sensorsMeasure();
  delay(1000);


  // variables declarations
  #ifdef DS18B20
    float sumTemperatureC = 0;
  #endif
  #ifdef VEML7700
    float sumLux = 0;
    float sumWhite = 0; 
  #endif
  #ifdef SDS011
    float sumPm25 = 0;
    float sumPm10 = 0;
  #endif
  #ifdef WXT536
    float sumWindDirection = 0;
    float sumWindSpeed = 0;
    float sumTemperature = 0;
    float sumHumidity = 0;
    float sumPressure = 0;
    float sumRainfall = 0;
    float sumHeaterTemp = 0;
    float sumHeaterVoltage = 0;
  #endif


  // variables declarations
  for (int i = 0; i < SAMPLES; i++) {
    sensorsMeasure();

    #ifdef DS18B20
      temperatureCReadings[i] = temperatureC;
    #endif
    #ifdef VEML7700
      luxReadings[i] = lux;
      whiteReadings[i] = white;
    #endif
    #ifdef SDS011
      if(sdsSleepCycles == sdsSleepCyclesLimit) {
        pm25Readings[i] = pm25;
        pm10Readings[i] = pm10;
      }
    #endif
    #ifdef WXT536
      windDirectionReadings[i] = windDirection;
      windSpeedReadings[i] = windSpeed;
      temperatureReadings[i] = temperature;
      humidityReadings[i] = humidity;
      pressureReadings[i] = pressure;
      rainfallReadings[i] = rainfall;
      heaterTempReadings[i] = heaterTemp;
      heaterVoltageReadings[i] = heaterVoltage;
    #endif

    delay(1000);
  }

  #ifdef SDS011
    if(sdsSleepCycles == sdsSleepCyclesLimit) {
      sds.sleep();
      }
  #endif


  // average measures evaluation
  for (int i = 0; i < SAMPLES; i++) {
    #ifdef DS18B20
      sumTemperatureC += temperatureCReadings[i];
    #endif
    #ifdef VEML7700
      sumLux += luxReadings[i];
      sumWhite += whiteReadings[i];
    #endif
    #ifdef SDS011
      if(sdsSleepCycles == sdsSleepCyclesLimit) {
        sumPm25 += pm25Readings[i];
        sumPm10 += pm10Readings[i];
      }
    #endif
    #ifdef WXT536
      sumWindDirection += windDirectionReadings[i];
      sumWindSpeed += windSpeedReadings[i];
      sumTemperature += temperatureReadings[i];
      sumHumidity += humidityReadings[i];
      sumPressure += pressureReadings[i];
      sumRainfall += rainfallReadings[i];
      sumHeaterTemp += heaterTempReadings[i];
      sumHeaterVoltage += heaterVoltageReadings[i];
    #endif
  }

  #ifdef DS18B20
    averageTemperatureC = sumTemperatureC / SAMPLES;
  #endif
  #ifdef VEML7700
    averageLux = sumLux / SAMPLES;
    chargingCurrent = calculateChargingCurrent(averageLux);
    photonFlux = calculatePhotonFlux(averageLux);
    averageWhite = sumWhite / SAMPLES;
  #endif
  #ifdef SDS011
    if(sdsSleepCycles == sdsSleepCyclesLimit) {
      averagePm25 = sumPm25 / SAMPLES;
      averagePm10 = sumPm10 / SAMPLES;
    }
  #endif
  #ifdef WXT536
    averageWindDirection = sumWindDirection / SAMPLES;
    averageWindSpeed = sumWindSpeed / SAMPLES;
    averageTemperature = sumTemperature / SAMPLES;
    averageHumidity = sumHumidity / SAMPLES;
    averagePressure = sumPressure / SAMPLES;
    averageRainfall = sumRainfall / SAMPLES;
    averageHeaterTemp = sumHeaterTemp / SAMPLES;
    averageHeaterVoltage = sumHeaterVoltage / SAMPLES; 
  #endif

  #ifdef DEBUG_AVERAGE_MEASUREMENTS
    #ifdef DS18B20
      Serial.println("");
      Serial.print("  Average air temperature DS18B20 [°C]: ");
      Serial.println(averageTemperatureC);
    #endif
    #ifdef VEML7700
      Serial.print("  Average lux [lx]: ");
      Serial.println(averageLux);
      Serial.print("  Average white [-]: ");
      Serial.println(averageWhite);
    #endif
    #ifdef SDS011
      if(sdsSleepCycles == sdsSleepCyclesLimit) {
        Serial.print("  Average PM 2.5 [µg/m3]: ");
        Serial.println(averagePm25);
        Serial.print("  Average PM 10 [µg/m3]: ");
        Serial.println(averagePm10);
      }
    #endif
    #ifdef WXT536
      Serial.print("  Average wind direction [°]: ");
      Serial.println(averageWindDirection);
      Serial.print("  Average wind speed [m/s]: ");
      Serial.println(averageWindSpeed);
      Serial.print("  Average air temperature WXT536 [°C]: ");
      Serial.println(averageTemperature);
      Serial.print("  Average humidity [%]: ");
      Serial.println(averageHumidity);
      Serial.print("  Average pressure [hPa]: ");
      Serial.println(averagePressure);
      Serial.print("  Average rainfall accumulation [mm]: ");
      Serial.println(averageRainfall);
      Serial.print("  Average heater temperature [°C]: ");
      Serial.println(averageHeaterTemp);
      Serial.print("  Average heater voltage [V]: ");
      Serial.println(averageHeaterVoltage);
    #endif
  #endif

  measured = true;
}


// UNIX conversion function ==================================================================================== 8/13
int64_t convertCclkToUnix(const char *cclkStr)
{
  int yy, MM, dd, hh, mm, ss, tz_quarters;
  char sign;

  int items = sscanf(cclkStr, "%2d/%2d/%2d,%2d:%2d:%2d%c%d",
                     &yy, &MM, &dd, &hh, &mm, &ss, &sign, &tz_quarters);

  if (items < 8) {
    return -1;
  }

  int fullYear = (yy < 70) ? (2000 + yy) : (1900 + yy);

  #define IS_LEAP_YEAR(y) ( ((y) % 400 == 0) || (((y) % 4 == 0) && ((y) % 100 != 0)) )

  static const int daysBeforeMonth[12] = {
    0,
    31,
    59,
    90,
    120,
    151,
    181,
    212,
    243,
    273,
    304,
    334
  };

  if (MM < 1 || MM > 12 || dd < 1 || dd > 31) {
    return -1;
  }
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59 || ss < 0 || ss > 59) {
    return -1;
  }

  int days = 0;
  if (fullYear >= 1970) {
    for (int y = 1970; y < fullYear; y++) {
      days += IS_LEAP_YEAR(y) ? 366 : 365;
    }
  } else {
    for (int y = fullYear; y < 1970; y++) {
      days -= IS_LEAP_YEAR(y) ? 366 : 365;
    }
  }

  days += daysBeforeMonth[MM - 1];

  if (IS_LEAP_YEAR(fullYear) && MM > 2) {
      days += 1;
  }

  days += (dd - 1);

  int64_t totalSeconds = (int64_t)days * 86400
                       + (int64_t)hh   * 3600
                       + (int64_t)mm   * 60
                       + (int64_t)ss;

  int offsetMinutes = tz_quarters * 15;
  if (sign == '-') {
    offsetMinutes = -offsetMinutes;
  }
  int64_t offsetSeconds = (int64_t)offsetMinutes * 60;

  int64_t unixTime = totalSeconds - offsetSeconds;

  return unixTime;
}


// timestamps calcutaion functions ============================================================================= 9/13
void computeAllTimestamps(int64_t currentUnixTime,
                          int   sleepCyclesLimit,
                          Measurement dataBuffer[BUFFER_SIZE]){

  int lastIndex = BUFFER_SIZE - 1;
  int64_t leftoverLast = (int64_t)dataBuffer[lastIndex].espDuration
                       - (int64_t)dataBuffer[lastIndex].measureDuration
                       - (int64_t)dataBuffer[lastIndex].initDuration;
  leftoverLast /= 1000;

  currentUnixTime -= leftoverLast;

  dataBuffer[lastIndex].timestamp = currentUnixTime;

  int64_t sleepSec = (int64_t)sleepCyclesLimit * 60;

  for (int i = (BUFFER_SIZE - 2); i >= 0; i--) {
    int64_t leftover_i = (int64_t)dataBuffer[i].espDuration
                       - (int64_t)dataBuffer[i].measureDuration
                       - (int64_t)dataBuffer[i].initDuration;
    leftover_i /= 1000;

    int64_t initSec    = (int64_t)dataBuffer[i+1].initDuration    / 1000;
    int64_t measureSec = (int64_t)dataBuffer[i+1].measureDuration / 1000;

    int64_t offsetSec = sleepSec + initSec + measureSec + leftover_i;

    dataBuffer[i].timestamp = dataBuffer[i+1].timestamp - offsetSec;
  }
}



// MQTT function =============================================================================================== 10/13
bool bc660kMQTT() {
  #ifdef BC660K
    HardwareSerial SERIAL_PORT(1);  
    SERIAL_PORT.begin(115200, SERIAL_8N1, BC660K_RX, BC660K_TX);
    quectel.begin(&SERIAL_PORT);
  #endif

  #ifdef BC660K
    #ifdef TIMESTAMP
      registrationStartTime = millis();
    #endif

    quectel.setDeepSleep();

    while(!quectel.getRegistrationStatus(5)) {                
      #ifdef DEBUG
        Serial.println("Waiting for network registration...");
      #endif
      delay(1000);
    }

    #ifdef TIMESTAMP
      registrationEndTime = millis();
      registrationDuration = registrationEndTime - registrationStartTime;
    #endif

    #ifdef DEBUG
      Serial.println("Module is successfully registered to network");
    #endif
  #endif

  #ifdef BC660K_DEBUG
    Serial.println("\nOpen MQTT connection.");
  #endif

  quectel.configTLS(caPem, clientPem, clientKey, 2);   // SSL ctx 1
  if(quectel.openMQTTSecure("XXX.XXX.XXX.XXX", 8883, 3, 0)) {
  //if(quectel.openMQTT("XXX.XXX.XXX.XXX", 1883)) {
    #ifdef BC660K_DEBUG
      Serial.println("\tMQTT connection opened.");
    #endif
  } else {
    #ifdef BC660K_DEBUG
      Serial.println("\tFailed to open MQTT connection.");
    #endif  
  }
  delay(1000);

  #ifdef BC660K_DEBUG
    Serial.println("\nConnect to MQTT broker.");
  #endif

  if(quectel.connectMQTTSecure("DKWS","danielkluka","Easter Egg: už je veľa (málo?) hodín, 3:45 a odovzdávam to o 10:00")) {
    #ifdef BC660K_DEBUG
      Serial.println("\tConnected to MQTT broker.");
    #endif
  } else {
    #ifdef BC660K_DEBUG
      Serial.println("\tFailed to connect to MQTT broker.");
    #endif
  }
  delay(1000);

  #ifdef BC660K_DEBUG
    Serial.println("\nPublish average readings to MQTT broker.");
  #endif
  
  // buffer message to send stored average measured data from past hour
  String message = generateMQTTMessage();
  //String message = "iba testujem :)";
  int messageLength = message.length();
  if(quectel.publishMQTT(message.c_str(), messageLength, "DKWS/TLS")) {
    #ifdef BC660K_DEBUG
      Serial.println("Average readings published to MQTT broker.");
    #endif
    success = true;
  } else {
    #ifdef BC660K_DEBUG
      Serial.println("\tFailed to publish average readings to MQTT broker.");
    #endif
  }
  delay(1000);

  #ifdef BC660K_DEBUG
    Serial.println("\nClose MQTT connection.");
  #endif

  if(quectel.closeMQTT()) {
    #ifdef BC660K_DEBUG
      Serial.println("\tMQTT connection closed.");
    #endif  
  } else {
    #ifdef BC660K_DEBUG
      Serial.println("\tFailed to close MQTT connection.");
    #endif  
  }

  delay(1000);

  #ifdef BC660K_DEBUG
    Serial.println("======MQTT SEND DONE======");
  #endif

  #ifdef BC660K_DEBUG
    Serial.println("\nTurn on deep sleep mode.");
  #endif
  if(quectel.setDeepSleep(1)) {
    #ifdef BC660K_DEBUG
      Serial.println("\tDeep sleep mode turned on.");
    #endif
  } else {
    #ifdef BC660K_DEBUG
      Serial.println("\tFailed to turn on deep sleep mode.");
    #endif
  }

  delay(1000);

  #ifdef BC660K
    return success;
  #endif
}



// buffer updating ============================================================================================= 11/13
void addToBuffer() {
  if (bufferIndex < BUFFER_SIZE) {
    dataBuffer[bufferIndex].batteryVoltage = batteryVoltage;
    dataBuffer[bufferIndex].stateOfCharge = stateOfCharge;
    dataBuffer[bufferIndex].recharged = recharged;
    dataBuffer[bufferIndex].initDuration = initEndTime;
    dataBuffer[bufferIndex].measureDuration = measureDuration;
    if (bufferIndex > 0) dataBuffer[bufferIndex - 1].espDuration = espDuration;
    #ifdef DS18B20
      dataBuffer[bufferIndex].temperature = averageTemperatureC;
    #endif
    #ifdef VEML7700
      dataBuffer[bufferIndex].lux = averageLux;
      dataBuffer[bufferIndex].white = averageWhite;
      dataBuffer[bufferIndex].chargingCurrent = chargingCurrent;
      dataBuffer[bufferIndex].photonFlux = photonFlux;
    #endif
    #ifdef SDS011
      if (sdsSleepCycles == sdsSleepCyclesLimit) {
        dataBuffer[bufferIndex].pm25 = averagePm25;
        dataBuffer[bufferIndex].pm10 = averagePm10;
      } else {
        dataBuffer[bufferIndex].pm25 = NAN;
        dataBuffer[bufferIndex].pm10 = NAN;
      }
    #endif
    #ifdef WXT536
      dataBuffer[bufferIndex].windDirection = averageWindDirection;
      dataBuffer[bufferIndex].windSpeed = averageWindSpeed;
      dataBuffer[bufferIndex].temperatureWXT536 = averageTemperature;
      dataBuffer[bufferIndex].humidity = averageHumidity;
      dataBuffer[bufferIndex].pressure = averagePressure;
      dataBuffer[bufferIndex].rainfall = averageRainfall;
      dataBuffer[bufferIndex].heaterTemp = averageHeaterTemp;
      dataBuffer[bufferIndex].heaterVoltage = averageHeaterVoltage;
    #endif
    bufferIndex++;
  }
}



// message generating ========================================================================================== 12/13
String generateMQTTMessage() {
  #ifdef TIMESTAMP
    dataBuffer[bufferIndex-1].espDuration = millis();
  #endif

  #ifdef BC660K
    dataSendTime = quectel.getDateAndTime();
    int64_t ts = convertCclkToUnix(dataSendTime.c_str());
    computeAllTimestamps(ts, sleepCyclesLimit, dataBuffer);
  #endif

  String message = "[";
  for (int i = 0; i < bufferIndex; i++) {
    message += "{";
    #ifdef BC660K
      message += "\"Ts\":" + String(dataBuffer[i].timestamp);
    #endif
    message += ",\"Vb\":" + String(dataBuffer[i].batteryVoltage);
    message += ",\"Sc\":" + String(dataBuffer[i].stateOfCharge);
    message += ",\"Re\":" + String(dataBuffer[i].recharged);
    message += ",\"Id\":" + String(dataBuffer[i].initDuration);
    message += ",\"Md\":" + String(dataBuffer[i].measureDuration);
    message += ",\"Ed\":" + String(dataBuffer[i].espDuration);
    #ifdef BC660K
      if (i == (bufferIndex - 1)) message += ",\"Rd\":" + String(registrationDuration);
    #endif

    #ifdef DS18B20
      message += ",\"Td\":" + String(dataBuffer[i].temperature);
    #endif

    #ifdef VEML7700
      message += ",\"Lu\":" + String(dataBuffer[i].lux);
      message += ",\"Wh\":" + String(dataBuffer[i].white);
      message += ",\"Cc\":" + String(dataBuffer[i].chargingCurrent);
      //message += ",\"Pf\":" + String(dataBuffer[i].photonFlux);
    #endif

    #ifdef SDS011
      message += ",\"PM25\":";
      if (isnan(dataBuffer[i].pm25)) {
        message += "null";
      } else {
        message += String(dataBuffer[i].pm25);
      }
      message += ",\"PM10\":";
      if (isnan(dataBuffer[i].pm10)) {
        message += "null";
      } else {
        message += String(dataBuffer[i].pm10);
      }
    #endif

    #ifdef WXT536
      message += ",\"Tw\":" + String(dataBuffer[i].temperatureWXT536);
      message += ",\"Hu\":" + String(dataBuffer[i].humidity);
      message += ",\"Pr\":" + String(dataBuffer[i].pressure);
      message += ",\"Ra\":" + String(dataBuffer[i].rainfall);
    #endif

    message += "}";
    if (i < bufferIndex - 1) {
      message += ",";
    }
  }
  message += "]";
  return message;
}



// message debugging =========================================================================================== 13/13
void debugMQTTMessage() {
  String message = generateMQTTMessage();
  Serial.println("");
  Serial.println("Generated MQTT Message:");
  Serial.println(message);
}
