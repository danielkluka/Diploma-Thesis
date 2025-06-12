#ifndef SENSORS_H
#define SENSORS_H
#define SENSORS_TEST_API

/** \file Sensors.h
 *  \brief Sensors header
 *  \details File contains Sensors function definitions used for Diploma Thesis: "Design of a secure data transmission system in NB-IoT environment".
 *  \author Bc. Daniel Kluka
 *  \version 2024
 *  \attention Project available at: https://github.com/danielkluka/Diploma-Thesis
 *	\attention File Sensors.h is in late stage of development.
 *  $Id$: Sensors.h 3 2024-12-10 00:22:00Z xkluka00 $
 */

#include <cstdint>

// sensor initialization functions
/** \brief	Air temperature sensor DS18B20 initialization function
 *  \details	Initializes temperature sensor.
 *  \param[in, out]	dsInit	Reference to the control flag indicating initialization status.
 *  \param[in, out]	sensor	Reference to the DallasTemperature object representing the sensor.
 *  \return	Bool value representing the success of initialization.
 *  \author	Kluka
 */
bool dsInitialize(bool& dsInit, DallasTemperature& sensor);

/** \brief	Light intensity sensor VEML7700 initialization function
 *  \details	Initializes light intensity sensor.
 *  \param[in, out]	vemlInit	Reference to the control flag indicating initialization status.
 *  \param[in, out] veml		Reference to the Adafruit_VEML7700 object representing the sensor.
 *  \return	Bool value representing the success of initialization.
 *  \author	Kluka
 */
bool vemlInitialize(bool& vemlInit, Adafruit_VEML7700& veml);

/** \brief Air quality sensor SDS011 initialization function
 *  \details Initializes the SDS011 air quality sensor.
 *  \param[in, out]	sdsInit			Reference to the control flag indicating initialization status.
 *  \param[in, out] SDS_RX			Pin number for SDS011 RX.
 *  \param[in]		SDS_TX			Pin number for SDS011 TX.
 *  \param[in]		SDS011Serial	Reference to the HardwareSerial object for communication.
 *  \param[in, out]	sds				Reference to the SdsDustSensor object representing the sensor.
 *  \return Bool value representing the success of initialization.
 *  \author Kluka
 */
bool sdsInitialize(bool& sdsInit, int& SDS_RX, int& SDS_TX, HardwareSerial& SDS011Serial, SdsDustSensor& sds);

/**	\brief Multiparametric weather transmitter WXT536 initialization function
 *	\details Initializes the WXT536 weather transmitter for communication.
 *	\param[in, out]	wxtInit			Reference to the control flag indicating the initialization status of the WXT536.
 *	\param[in, out]	WXT_RX			Pin number for the WXT536 RX (receive) line.
 *	\param[in]		WXT_TX			Pin number for the WXT536 TX (transmit) line.
 *	\param[in]		WXT536Serial	Reference to the HardwareSerial object used for communication with the WXT536 weather transmitter.
 *	\return	Bool value indicating the success of the initialization.
 *	\author Kluka
 */
bool wxtInitialize(bool& wxtInit, int& WXT_RX, int& WXT_TX, HardwareSerial& WXT536Serial);


// sensor connection check functions
/** \brief	I2C connection check function
 *  \details	Checks if I2C data transmission is functional.
 *  \param[in]	address	Memory address of connected I2C device.
 *  \return	Bool value representing the success of I2C data transmission.
 *  \author	Kluka
 */
bool checkI2CConnection(uint8_t address);


// sensor measurement functions
/** \brief Air temperature sensor DS18B20 measurement function
 *  \details Measures air temperature.
 *  \param[in, out]	tempC	Reference to the temperature variable in °C.
 *  \param[in, out]	sensor	Reference to the DallasTemperature object representing the sensor.
 *  \return Bool value representing the success of measurement.
 *  \author Kluka
 */
bool dsMeasure(float& tempC, DallasTemperature& sensor);

/** \brief Light intensity sensor VEML7700 measurement function
 *  \details Measures light intensity and amount of white light.
 *  \param[in, out] lux		Reference to the variable storing light intensity in lux (lx) - lumen per square meter [lm/m²].
 *  \param[in, out]	white	Reference to the variable storing the amount of white light.
 *  \param[in]		veml	Reference to the Adafruit_VEML7700 object representing the sensor.
 *  \return Bool value representing the success of measurement.
 *  \author Kluka
 */
bool vemlMeasure(float& lux, float& white, Adafruit_VEML7700& veml);

/** \brief Air quality sensor SDS011 measurement function
 *  \details Measures dust particles PM 2.5 and PM 10.
 *  \param[in, out]	pm25	Reference to the variable storing dust particles up to 2.5 µm in diameter. Measured in micrograms per cubic meter [µg/m³].
 *  \param[in, out]	pm10	Reference to the variable storing dust particles up to 10 µm in diameter. Measured in micrograms per cubic meter [µg/m³].
 *  \param[in, out]	sds		Reference to the SdsDustSensor object representing the sensor.
 *  \return Bool value representing the success of measurement.
 *  \author Kluka
 */
bool sdsMeasure(float& pm25, float& pm10, SdsDustSensor& sds);

/**	\brief Multiparametric weather transmitter WXT536 measurement function
 *	\details Reads data from the WXT536 weather transmitter and parses the measured values.
 *	\param[in, out]	WXT536Serial	Reference to the HardwareSerial object used for communication with the WXT536.
 *	\param[in, out]	receivedData	Reference to a String variable used to store incoming data temporarily.
 *	\param[in, out]	completeMessage	Reference to a String variable storing the complete data message.
 *	\param[in, out]	windDir			Reference to a variable storing the wind direction in degrees.
 *	\param[in, out]	windSp			Reference to a variable storing the wind speed in meters per second [m/s].
 *	\param[in, out]	temp			Reference to a variable storing the temperature in degrees Celsius [°C].
 *	\param[in, out]	humi			Reference to a variable storing the relative humidity in percentage [%].
 *	\param[in, out]	pres			Reference to a variable storing the atmospheric pressure in hectopascals [hPa].
 *	\param[in, out]	rain			Reference to a variable storing the amount of rainfall in millimeters [mm].
 *	\param[in, out]	heaterT			Reference to a variable storing the heater temperature in degrees Celsius [°C].
 *	\param[in, out]	heaterV			Reference to a variable storing the heater voltage in volts [V].
 *	\return Bool value indicating whether data was successfully received.
 *	\author Kluka
 */
bool wxtMeasure(HardwareSerial& WXT536Serial, String& receivedData, String& completeMessage, float& windDir, float& windSp, float& temp, float& humi, float& pres, float& rain, float& heaterT, float& heaterV);

/**	\brief Multiparametric weather transmitter WXT536 data parsing function
 *	\details Parses a complete data string from the WXT536 weather transmitter and extracts specific measurement values.
 *	\param[in] data String		containing the complete data message received from the WXT536.
 *	\param[in, out] windDirData	Reference to a variable storing the wind direction in degrees.
 *	\param[in, out] windSpData	Reference to a variable storing the wind speed in meters per second [m/s].
 *	\param[in, out] tempData	Reference to a variable storing the temperature in degrees Celsius [°C].
 *	\param[in, out] humiData	Reference to a variable storing the relative humidity in percentage [%].
 *	\param[in, out] presData	Reference to a variable storing the atmospheric pressure in hectopascals [hPa].
 *	\param[in, out] rainData	Reference to a variable storing the amount of rainfall in millimeters [mm].
 *	\param[in, out] heaterTData	Reference to a variable storing the heater temperature in degrees Celsius [°C].
 *	\param[in, out] heaterVData	Reference to a variable storing the heater voltage in volts [V].
 *	\author Kluka
 */
void parseWXT536Data(String data, float& windDirData, float& windSpData, float& tempData, float& humiData, float& presData, float& rainData, float& heaterTData, float& heaterVData);

/**	\brief Multiparametric weather transmitter WXT536 data extraction function
 *	\details Extracts a specific value from a data string based on a key and a delimiter.
 *	\param[in]	data		String containing the data message.
 *	\param[in]	key			String representing the key identifying the data to extract.
 *	\param[in]	delimiter	String representing the delimiter marking the end of the value.
 *	\return	String containing the extracted value.
 *	\author Kluka
 */
String extractValue(String data, String key, String delimiter);

#endif /* SENSORS_H */
