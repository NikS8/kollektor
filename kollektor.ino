/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
                                                    kollektor.ino 
                                Copyright © 2019, Zigfred & Nik.S
31.01.2019 v1
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*******************************************************************\
Сервер kollektor выдает данные: 
  цифровые: 
    датчик скорости потока воды YF-B5
    датчики температуры DS18B20
/*******************************************************************/

#include <Ethernet.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RBD_Timer.h>

#define DEVICE_ID "kollektor";
#define VERSION 1

#define RESET_UPTIME_TIME 43200000  //  = 30 * 24 * 60 * 60 * 1000 
                                    // reset after 30 days uptime 

//IPAddress ip(192, 168, 1, 144); 
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x88, 0x88};
EthernetServer httpServer(80);
#define SWITCH_TO_W5100 digitalWrite(4,HIGH); digitalWrite(10,LOW)    //  Переключение на интернет

#define DS18_CONVERSION_TIME 750 // (1 << (12 - ds18Precision))
#define DS18_PRECISION = 11
#define PIN8_ONE_WIRE_BUS 8      //  коллектор
uint8_t ds18DeviceCount8;
OneWire ds18wireBus8(PIN8_ONE_WIRE_BUS);
DallasTemperature ds18Sensors8(&ds18wireBus8);
#define PIN9_ONE_WIRE_BUS 9           //  бойлер
uint8_t ds18DeviceCount9;
OneWire ds18wireBus9(PIN9_ONE_WIRE_BUS);
DallasTemperature ds18Sensors9(&ds18wireBus9);

#define PIN_FLOW_SENSOR2 2
#define PIN_INTERRUPT_FLOW_SENSOR2 0
#define FLOW_SENSOR_CALIBRATION_FACTOR 5
volatile long flowSensorPulseCount2 = 0;
#define PIN_FLOW_SENSOR3 3
#define PIN_INTERRUPT_FLOW_SENSOR3 1
#define FLOW_SENSOR_CALIBRATION_FACTOR 5
volatile long flowSensorPulseCount3 = 0;
// time
uint32_t flowSensorLastTime2;
uint32_t flowSensorLastTime3;

// timers
RBD::Timer ds18ConversionTimer;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            setup
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  while (!Serial) continue;

  pinMode(PIN_FLOW_SENSOR2, INPUT);
    attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR2, flowSensorPulseCount2, FALLING);
  sei();
  pinMode(PIN_FLOW_SENSOR3, INPUT);
    attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCount3, FALLING);
  sei();

  httpServer.begin();
  Serial.println(F("Server is ready."));
  Serial.print(F("Please connect to http://"));
  Serial.println(Ethernet.localIP());

  ds18Sensors8.begin();
  ds18DeviceCount8 = ds18Sensors8.getDeviceCount();
    ds18Sensors9.begin();
  ds18DeviceCount9 = ds18Sensors9.getDeviceCount();
    
  getSettings();
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            Settings
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void getSettings() {
   
  ds18Sensors8.setWaitForConversion(false);
  ds18Sensors9.setWaitForConversion(false);

  ds18Sensors8.requestTemperatures();
  ds18Sensors9.requestTemperatures();
  
  ds18ConversionTimer.setTimeout(DS18_CONVERSION_TIME);
  ds18ConversionTimer.restart();
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            loop
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void loop() {
 // SWITCH_TO_W5100;

   resetWhen30Days();

    realTimeService();
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            realTimeService
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void realTimeService() {

  EthernetClient reqClient = httpServer.available();
  if (!reqClient) return;
  ds18RequestTemperatures();
//      currentTime = millis();

  while (reqClient.available()) reqClient.read();

  String data = createDataString();

  reqClient.println(F("HTTP/1.1 200 OK"));
  reqClient.println(F("Content-Type: application/json"));
  reqClient.print(F("Content-Length: "));
  reqClient.println(data.length());
  reqClient.println();
  reqClient.print(data);

  reqClient.stop();
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            ds18RequestTemperatures
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void ds18RequestTemperatures () {
  if (ds18ConversionTimer.onRestart()) {
    ds18Sensors8.requestTemperatures();
    ds18Sensors9.requestTemperatures();
}
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            flowSensorPulseCounter
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void flowSensorPulseCounter2()
{
  // Increment the pulse counter
  flowSensorPulseCount2++;
}

void flowSensorPulseCounter3()
{
  // Increment the pulse counter
  flowSensorPulseCount3++;
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            createDataString
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
String createDataString() {
  String resultData;

  resultData.concat(F("{"));
  resultData.concat(F("\n\"deviceId\":"));
//  resultData.concat(String(DEVICE_ID));
  resultData.concat(F("\"kollektor\""));
  resultData.concat(F(","));
  resultData.concat(F("\n\"version\":"));
  resultData.concat(VERSION);

  resultData.concat(F(","));
  resultData.concat(F("\n\"data\": {"));

  for (uint8_t index9 = 0; index9 < ds18DeviceCount9; index9++)
  {
    DeviceAddress deviceAddress9;
    ds18Sensors9.getAddress(deviceAddress9, index9);
    resultData.concat(F("\n\t\""));
    for (uint8_t i = 0; i < 8; i++)
    {
      if (deviceAddress9[i] < 16) resultData.concat("0");

      resultData.concat(String(deviceAddress9[i], HEX));
    }
    resultData.concat(F("\":"));
    resultData.concat(ds18Sensors9.getTempC(deviceAddress9));
    resultData.concat(F(","));
  }

  for (uint8_t index8 = 0; index8 < ds18DeviceCount8; index8++)
  {
    DeviceAddress deviceAddress8;
    ds18Sensors8.getAddress(deviceAddress8, index8);
        resultData.concat(F("\n\t\""));
    for (uint8_t i = 0; i < 8; i++)
    {
      if (deviceAddress8[i] < 16) resultData.concat("0");

      resultData.concat(String(deviceAddress8[i], HEX));
    }
    resultData.concat(F("\":"));
    resultData.concat(ds18Sensors8.getTempC(deviceAddress8));
    resultData.concat(F(","));
  }
  
//  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-2\":"));
  resultData.concat(getFlowData2());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-3\":"));
  resultData.concat(getFlowData3());
  resultData.concat(F("\n\t }"));
  resultData.concat(F(","));
  resultData.concat(F("\n\"freeRam\":"));
  resultData.concat(freeRam());
  resultData.concat(F("\n }"));
 // resultData.concat(F("}"));

  return resultData;

}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            getFlowData
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int getFlowData2()
{
  //  static int flowSensorPulsesPerSecond;
  unsigned long flowSensorPulsesPerSecond2;

  unsigned long deltaTime2 = millis() - flowSensorLastTime2;
  //  if ((millis() - flowSensorLastTime2) < 1000) {
  if (deltaTime2 < 1000)
  {
    return;
  }

  //detachInterrupt(flowSensorInterrupt);
  detachInterrupt(PIN_INTERRUPT_FLOW_SENSOR2);
  //     flowSensorPulsesPerSecond = (1000 * flowSensorPulseCount / (millis() - flowSensorLastTime));
  //    flowSensorPulsesPerSecond = (flowSensorPulseCount * 1000 / deltaTime);
  flowSensorPulsesPerSecond2 = flowSensorPulseCount2 * 1000 / deltaTime2;
 /// flowSensorPulsesPerSecond2 *= 1000;
 /// flowSensorPulsesPerSecond2 /= deltaTime; //  количество за секунду

  flowSensorLastTime2 = millis();
  flowSensorPulseCount2 = 0;
  //attachInterrupt(flowSensorInterrupt, flowSensorPulseCounter, FALLING);
  attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR2, flowSensorPulseCounter2, FALLING);

  return flowSensorPulsesPerSecond2;

}

/////////////////////
int getFlowData3()
{
  //  static int flowSensorPulsesPerSecond;
  unsigned long flowSensorPulsesPerSecond3;

  unsigned long deltaTime3 = millis() - flowSensorLastTime3;
  //  if ((millis() - flowSensorLastTime3) < 1000) {
  if (deltaTime3 < 1000)
  {
    return;
  }

  //detachInterrupt(flowSensorInterrupt);
  detachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3);
  //     flowSensorPulsesPerSecond = (1000 * flowSensorPulseCount / (millis() - flowSensorLastTime));
  //    flowSensorPulsesPerSecond = (flowSensorPulseCount * 1000 / deltaTime);
  flowSensorPulsesPerSecond3 = flowSensorPulseCount3 * 1000 / deltaTime3;;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTime3 = millis();
  flowSensorPulseCount3 = 0;
  //attachInterrupt(flowSensorInterrupt, flowSensorPulseCounter, FALLING);
  attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCounter3, FALLING);

  return flowSensorPulsesPerSecond3;

}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            UTILS
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  void resetWhen30Days()
  {
    if (millis() > (RESET_UPTIME_TIME))
    {
      // do reset
    }
  }

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            Количество свободной памяти
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            end
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
