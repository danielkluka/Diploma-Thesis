#include <Quectel_BC660.h>

#define SENSOR_POWER_PIN 6          // sensors voltage enable
#define BC660K_TX 17           // TX pin pre Quectel BC660K
#define BC660K_RX 18           // RX pin pre Quectel BC660K
QuectelBC660 quectel = QuectelBC660(NOT, true);

void setup() 
{
  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);

  HardwareSerial SERIAL_PORT(1);  
  SERIAL_PORT.begin(115200, SERIAL_8N1, BC660K_RX, BC660K_TX);
  quectel.begin(&SERIAL_PORT);

	Serial.begin(115200);
	Serial.println("Quectel MQTT connection test");
	Serial.println("===================");
	quectel.begin(&SERIAL_PORT);
    if(quectel.getRegistrationStatus(5))
    {
        Serial.println("Module is registered to network");
    }
    quectel.setDeepSleep();
    Serial.println("======MQTT SEND======");
    quectel.openMQTT("147.229.148.103", 1883);	// Replace 0.0.0.0 with address of your MQTT broker
    delay(1000);
    quectel.connectMQTT("Test-123456");
    delay(1000);
    quectel.publishMQTT("Hello world!", 12, "203251/test");
    delay(1000);
    quectel.closeMQTT();
    delay(1000);
    Serial.println("======MQTT SEND DONE======");
    quectel.setDeepSleep(1);
}

void loop()
{

}
