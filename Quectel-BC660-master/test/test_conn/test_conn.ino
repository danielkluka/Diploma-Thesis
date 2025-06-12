#include <Quectel_BC660.h>

HardwareSerial SERIAL_PORT(1);  // Použitie UART0 na ESP32-S3 pre Quectel
#define QUECTEL_TX_PIN 17  // TX pin pre Quectel BC660K
#define QUECTEL_RX_PIN 18  // RX pin pre Quectel BC660K
#define SENSOR_POWER_PIN 6          // sensors voltage enable

QuectelBC660 quectel = QuectelBC660(NOT, true);

void setup() 
{
	Serial.begin(115200);
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);
	Serial.println("Quectel MQTT connection test");
	Serial.println("===================");
	SERIAL_PORT.begin(115200, SERIAL_8N1, QUECTEL_RX_PIN, QUECTEL_TX_PIN); // Inicializácia UART1 s baud rate 115200
  quectel.begin(&SERIAL_PORT);
    if(quectel.getRegistrationStatus(5))
    {
        Serial.println("Module is registered to network");
    }
    quectel.setDeepSleep();
    Serial.println("=====Deregister from network=====");
    if(quectel.deregisterFromNetwork())
    {
        Serial.println("Done!");
    }
    else 
    {
        Serial.println("ERROR!");
    }
    delay(1000);
    Serial.println("=====Set bands to 8 and 20=====");
    uint8_t bands[2] = {8, 20};
    if(quectel.setManualBand(2,bands))
    {
        Serial.println("Done!");
    }
    else 
    {
        Serial.println("ERROR!");
    }
    delay(1000);
    Serial.println("=====Register to specific network=====");
    if(quectel.manualRegisterToNetwork(23003))
    {
        Serial.println("Done!");
    } 
    else 
    {
        Serial.println("ERROR!");
    }
    delay(1000);
    Serial.println("======TEST DONE======");
    quectel.setDeepSleep(1);
}

void loop()
{

}