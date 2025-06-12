#include <Quectel_BC660.h>

#define QUECTEL_TX_PIN 17  // TX pin pre Quectel BC660K
#define QUECTEL_RX_PIN 18  // RX pin pre Quectel BC660K
#define SENSOR_POWER_PIN 6  // GPIO na riadenie napájania senzorov

HardwareSerial SERIAL_PORT(1);  // Použitie UART0 na ESP32-S3 pre Quectel

//#define SERIAL_PORT Serial1

QuectelBC660 quectel = QuectelBC660(5, true);


void setup() 
{
	Serial.begin(115200);

  pinMode(SENSOR_POWER_PIN, OUTPUT);
  digitalWrite(SENSOR_POWER_PIN, HIGH);  // Zapneme napájanie senzorov
  delay(1000);

	Serial.println("Quectel test sketch");
	Serial.println("===================");
	Serial.println("Initializing module");
  SERIAL_PORT.begin(115200, SERIAL_8N1, QUECTEL_RX_PIN, QUECTEL_TX_PIN); // Inicializácia UART1 s baud rate 115200
	quectel.begin(&SERIAL_PORT);
	quectel.setDeepSleep();
	Serial.print("RSSI: ");	
	Serial.println(quectel.getRSSI());
	Serial.print("BER: "); 
	Serial.println(quectel.getBER());
	Serial.print("Time: "); 
	Serial.println(quectel.getDateAndTime());
	Serial.print("Status code: "); 
	Serial.println(quectel.getStatusCode());
	Serial.print("Status: "); 
	Serial.println(quectel.getStatus());
	quectel.getData();
	Serial.print("RSRP: "); 
  	Serial.println(quectel.engineeringData.RSRP);
	Serial.print("RSRQ: "); 
  	Serial.println(quectel.engineeringData.RSRQ);
	Serial.print("RSSI: "); 
  	Serial.println(quectel.engineeringData.RSSI);
	Serial.print("SINR: "); 
  	Serial.println(quectel.engineeringData.SINR);
	Serial.print("Firmware: ");
	Serial.println(quectel.engineeringData.firmwareVersion);
	Serial.print("Epoch: ");
	Serial.println(quectel.engineeringData.epoch);
	Serial.print("Time zone: ");
	Serial.println(quectel.engineeringData.timezone);

  while(!quectel.getRegistrationStatus(5)){                // Wait until module is registered to network
    Serial.println("Waiting for network registration...");
    delay(1000);
  }
  Serial.println("Module is successfully registered to network");
	//quectel.setDeepSleep(1);
}


void loop()
{
	delay(10000);	
	quectel.getData();
	Serial.print("RSRP: "); 
  	Serial.println(quectel.engineeringData.RSRP);
	Serial.print("RSRQ: "); 
  	Serial.println(quectel.engineeringData.RSRQ);
	Serial.print("RSSI: "); 
  	Serial.println(quectel.engineeringData.RSSI);
	Serial.print("SINR: "); 
  	Serial.println(quectel.engineeringData.SINR);
	Serial.print("Epoch: "); 
	Serial.println(quectel.engineeringData.epoch);
}