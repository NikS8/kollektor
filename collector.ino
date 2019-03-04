/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
                                                      collector.ino 
                                  Copyright © 2019, Zigfred & Nik.S
31.01.2019 v1
18.02.2019 V2 Прерывания PCINT0_vect, PCINT1_vect и PCINT2_vect
25.02.2019 V3 dell PCINT0_vect, PCINT1_vect и PCINT2_vect
26.02.2019 V4 замена на #include <EnableInterrupt.h>
26.02.2019 V5 коллектор теплого пола:cf-cook,-office,-dorm,-corridor
27.02.2019 V6 add enableInterrupt(2,flowSensorPulseCounter2,FALLING)
28.02.2019 v7 переименование на collector, cf-hall-f, cf-hall-t ...
04.03.2019 v8 время работы 
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*******************************************************************\
Сервер kollektor выдает данные: 
  цифровые: 
    датчики скорости потока воды YF-B5
    датчики температуры DS18B20
/*******************************************************************/

#include <Ethernet2.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RBD_Timer.h>
#include <EnableInterrupt.h>

#define DEVICE_ID "collector"
#define VERSION 8

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x88, 0x88};
EthernetServer httpServer(40159); //IPAddress ip(192, 168, 1, 159);

#define RESET_UPTIME_TIME 43200000  //  = 30 * 24 * 60 * 60 * 1000 
                                    // reset after 30 days uptime 
#define DS18_CONVERSION_TIME 750 // (1 << (12 - ds18Precision))
#define DS18_PRECISION = 11
#define PIN9_ONE_WIRE_BUS 9           
uint8_t ds18DeviceCount9;
OneWire ds18wireBus9(PIN9_ONE_WIRE_BUS);
DallasTemperature ds18Sensors9(&ds18wireBus9);

volatile long flowSensorPulseCountA0 = 0;
volatile long flowSensorPulseCountA1 = 0;
volatile long flowSensorPulseCountA2 = 0;
volatile long flowSensorPulseCountA3 = 0;
volatile long flowSensorPulseCountA4 = 0;
volatile long flowSensorPulseCountA5 = 0;
volatile long flowSensorPulseCount2 = 0;
//volatile long flowSensorPulseCount3 = 0;
// time
uint32_t flowSensorLastTimeA0;
uint32_t flowSensorLastTimeA1;
uint32_t flowSensorLastTimeA2;
uint32_t flowSensorLastTimeA3;
uint32_t flowSensorLastTimeA4;
uint32_t flowSensorLastTimeA5;
uint32_t flowSensorLastTime2;
//uint32_t flowSensorLastTime3;
// timers
RBD::Timer ds18ConversionTimer;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            setup
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  while (!Serial) continue;

  enableInterrupt(A0, flowSensorPulseCounterA0, FALLING);
  enableInterrupt(A1, flowSensorPulseCounterA1, FALLING);
  enableInterrupt(A2, flowSensorPulseCounterA2, FALLING);
  enableInterrupt(A3, flowSensorPulseCounterA3, FALLING);
  enableInterrupt(A4, flowSensorPulseCounterA4, FALLING);
  enableInterrupt(A5, flowSensorPulseCounterA5, FALLING);
  enableInterrupt(2, flowSensorPulseCounter2, FALLING);
//  enableInterrupt(3, flowSensorPulseCounter3, FALLING);

  httpServer.begin();
  Serial.println(F("Server is ready."));
  Serial.print(F("Please connect to http://"));
  Serial.println(Ethernet.localIP());

  ds18Sensors9.begin();
  ds18DeviceCount9 = ds18Sensors9.getDeviceCount();
    
  getSettings();
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            Settings
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void getSettings() {
   
  ds18Sensors9.setWaitForConversion(false);

  ds18Sensors9.requestTemperatures();
  
  ds18ConversionTimer.setTimeout(DS18_CONVERSION_TIME);
  ds18ConversionTimer.restart();
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            loop
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void loop() {
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
    ds18Sensors9.requestTemperatures();
}
}
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            flowSensorPulseCounter
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void flowSensorPulseCounterA0()  { flowSensorPulseCountA0++;  }
void flowSensorPulseCounterA1()  { flowSensorPulseCountA1++;  }
void flowSensorPulseCounterA2()  { flowSensorPulseCountA2++;  }
void flowSensorPulseCounterA3()  { flowSensorPulseCountA3++;  }
void flowSensorPulseCounterA4()  { flowSensorPulseCountA4++;  }
void flowSensorPulseCounterA5()  { flowSensorPulseCountA5++;  }
void flowSensorPulseCounter2()  { flowSensorPulseCount2++;  }
//  void flowSensorPulseCounter3()  { flowSensorPulseCount3++;  }

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            createDataString
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
String createDataString() {
  String resultData;

  resultData.concat(F("{"));
  resultData.concat(F("\n\"deviceId\":\""));
  resultData.concat(DEVICE_ID);
//  resultData.concat(F("\"collector\""));
  resultData.concat(F("\",\n\"version\":"));
  resultData.concat(VERSION);

  resultData.concat(F(",\n\"data\": {"));

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
  resultData.concat(F("\n\t\"cf-hall-f\":"));
  resultData.concat(getFlowDataA3());
  resultData.concat(F(",\n\t\"cf-tv-f\":"));
  resultData.concat(getFlowDataA2());
  resultData.concat(F(",\n\t\"cf-bed-f\":"));
  resultData.concat(getFlowDataA1());
  resultData.concat(F(",\n\t\"cf-st-f\":"));
  resultData.concat(getFlowDataA0());
  resultData.concat(F(",\n\t\"cf-kitch-f\":"));
  resultData.concat(getFlowData2());
 // resultData.concat(F(",\n\t\"cf-radiator-f\":"));
 // resultData.concat(getFlowData3());
  resultData.concat(F(",\n\t\"cf-vbath-f\":"));
  resultData.concat(getFlowDataA5());
  resultData.concat(F(",\n\t\"cf-vbed-f\":"));
  resultData.concat(getFlowDataA4());
    resultData.concat(F("\n\t }"));
  resultData.concat(F(",\n\"freeRam\":"));
  resultData.concat(freeRam());
  resultData.concat(F(",\n\"upTime\":\""));
  resultData.concat(upTime(millis()));
  resultData.concat(F("\"\n }"));

  return resultData;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            getFlowData
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
int getFlowData2()
{
  unsigned long flowSensorPulsesPerSecond2;
  unsigned long deltaTime = millis() - flowSensorLastTime2;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecond2 = flowSensorPulseCount2 * 1000 / deltaTime;
  flowSensorLastTime2 = millis();
  flowSensorPulseCount2 = 0;

  return flowSensorPulsesPerSecond2;
}
/*
/////////////////////
int getFlowData3()
{
  unsigned long flowSensorPulsesPerSecond3;
  unsigned long deltaTime = millis() - flowSensorLastTime3;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecond3 = flowSensorPulseCount3 * 1000 / deltaTime;
  flowSensorLastTime3 = millis();
  flowSensorPulseCount3 = 0;

  return flowSensorPulsesPerSecond3;
}
*/
/////////////////////
int getFlowDataA0()
{
  unsigned long flowSensorPulsesPerSecondA0;
  unsigned long deltaTime = millis() - flowSensorLastTimeA0;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecondA0 = flowSensorPulseCountA0 * 1000 / deltaTime;
  flowSensorLastTimeA0 = millis();
  flowSensorPulseCountA0 = 0;

  return flowSensorPulsesPerSecondA0;
}

/////////////////////
int getFlowDataA1()
{
  unsigned long flowSensorPulsesPerSecondA1;
  unsigned long deltaTime = millis() - flowSensorLastTimeA1;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecondA1 = flowSensorPulseCountA1 * 1000 / deltaTime;
  flowSensorLastTimeA1 = millis();
  flowSensorPulseCountA1 = 0;

  return flowSensorPulsesPerSecondA1;
}

/////////////////////
int getFlowDataA2()
{
  unsigned long flowSensorPulsesPerSecondA2;
  unsigned long deltaTime = millis() - flowSensorLastTimeA2;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecondA2 = flowSensorPulseCountA2 * 1000 / deltaTime;
  flowSensorLastTimeA2 = millis();
  flowSensorPulseCountA2 = 0;

  return flowSensorPulsesPerSecondA2;
}


/////////////////////
int getFlowDataA3()
{
  unsigned long flowSensorPulsesPerSecondA3;
  unsigned long deltaTime = millis() - flowSensorLastTimeA3;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecondA3 = flowSensorPulseCountA3 * 1000 / deltaTime;
  flowSensorLastTimeA3 = millis();
  flowSensorPulseCountA3 = 0;

  return flowSensorPulsesPerSecondA3;
}

/////////////////////
int getFlowDataA4()
{
  unsigned long flowSensorPulsesPerSecondA4;
  unsigned long deltaTime = millis() - flowSensorLastTimeA4;

  if (deltaTime < 1000)  { return; }
  flowSensorPulsesPerSecondA4 = flowSensorPulseCountA4 * 1000 / deltaTime;
  flowSensorLastTimeA4 = millis();
  flowSensorPulseCountA4 = 0;

  return flowSensorPulsesPerSecondA4;
}

/////////////////////
int getFlowDataA5()
{
  unsigned long flowSensorPulsesPerSecondA5;
  unsigned long deltaTime = millis() - flowSensorLastTimeA5;

  if (deltaTime < 1000)  { return; }

  flowSensorPulsesPerSecondA5 = flowSensorPulseCountA5 * 1000 / deltaTime;
  flowSensorLastTimeA5 = millis();
  flowSensorPulseCountA5 = 0;

  return flowSensorPulsesPerSecondA5;
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            Время работы после старта или рестарта
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
String upTime(uint32_t lasttime)
{
  lasttime /= 1000;
  String lastStartTime;
  
  if (lasttime > 86400) {
    uint8_t lasthour = lasttime/86400;
    lastStartTime.concat(lasthour);
    lastStartTime.concat(F("d "));
    lasttime = (lasttime-(86400*lasthour));
  }
  if (lasttime > 3600) {
    if (lasttime/3600<10) { lastStartTime.concat(F("0")); }
  lastStartTime.concat(lasttime/3600);
  lastStartTime.concat(F(":"));
  }
  if (lasttime/60%60<10) { lastStartTime.concat(F("0")); }
lastStartTime.concat((lasttime/60)%60);
lastStartTime.concat(F(":"));
  if (lasttime%60<10) { lastStartTime.concat(F("0")); }
lastStartTime.concat(lasttime%60);
//lastStartTime.concat(F("s"));

return lastStartTime;
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
