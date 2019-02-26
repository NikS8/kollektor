/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
                                                    kollektor.ino 
                                Copyright © 2019, Zigfred & Nik.S
31.01.2019 v1
18.02.2019 V2 Прерывания PCINT0_vect, PCINT1_vect и PCINT2_vect
25.02.2019 V3 dell PCINT0_vect и PCINT2_vect
26.02.2019 V4 add #include <EnableInterrupt.h>
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/*******************************************************************\
Сервер kollektor выдает данные: 
  цифровые: 
    датчик скорости потока воды YF-B5
    датчики температуры DS18B20
/*******************************************************************/

#include <Ethernet2.h> //IPAddress ip(192, 168, 1, 159); 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RBD_Timer.h>
#include <EnableInterrupt.h>

#define DEVICE_ID "kollektor";
#define VERSION 4

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x88, 0x88};
EthernetServer httpServer(40159);
#define RESET_UPTIME_TIME 43200000  //  = 30 * 24 * 60 * 60 * 1000 
                                    // reset after 30 days uptime 
#define DS18_CONVERSION_TIME 750 // (1 << (12 - ds18Precision))
#define DS18_PRECISION = 11
#define PIN9_ONE_WIRE_BUS 9           
uint8_t ds18DeviceCount9;
OneWire ds18wireBus9(PIN9_ONE_WIRE_BUS);
DallasTemperature ds18Sensors9(&ds18wireBus9);
/*
#define PIN_FLOW_SENSOR2 2
#define PIN_INTERRUPT_FLOW_SENSOR2 0
#define FLOW_SENSOR_CALIBRATION_FACTOR 5
volatile long flowSensorPulseCount2 = 0;
#define PIN_FLOW_SENSOR3 3
#define PIN_INTERRUPT_FLOW_SENSOR3 1
#define FLOW_SENSOR_CALIBRATION_FACTOR 5
volatile long flowSensorPulseCount3 = 0;
*/

volatile long flowSensorPulseCountA0 = 0;
volatile long flowSensorPulseCountA1 = 0;
volatile long flowSensorPulseCountA2 = 0;
volatile long flowSensorPulseCountA3 = 0;
volatile long flowSensorPulseCountA4 = 0;
volatile long flowSensorPulseCountA5 = 0;
// time
//uint32_t flowSensorLastTime2;
//uint32_t flowSensorLastTime3;
uint32_t flowSensorLastTimeA0;
uint32_t flowSensorLastTimeA1;
uint32_t flowSensorLastTimeA2;
uint32_t flowSensorLastTimeA3;
uint32_t flowSensorLastTimeA4;
uint32_t flowSensorLastTimeA5;

// timers
RBD::Timer ds18ConversionTimer;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            setup
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  while (!Serial) continue;
/*
  pinMode(PIN_FLOW_SENSOR2, INPUT);
    attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR2, flowSensorPulseCounter2, FALLING);
  sei();
  pinMode(PIN_FLOW_SENSOR3, INPUT);
    attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCounter3, FALLING);
  sei();
  */
  enableInterrupt(A0, flowSensorPulseCounterA0, FALLING);
  enableInterrupt(A1, flowSensorPulseCounterA1, FALLING);
  enableInterrupt(A2, flowSensorPulseCounterA2, FALLING);
  enableInterrupt(A3, flowSensorPulseCounterA3, FALLING);
  enableInterrupt(A4, flowSensorPulseCounterA4, FALLING);
  enableInterrupt(A5, flowSensorPulseCounterA5, FALLING);

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
/*void flowSensorPulseCounter2()
{
    flowSensorPulseCount2++;// INT0
}

void flowSensorPulseCounter3()
{
    flowSensorPulseCount3++;// INT1
}
*/
void flowSensorPulseCounterA0()  { flowSensorPulseCountA0++;  }
void flowSensorPulseCounterA1()  { flowSensorPulseCountA1++;  }
void flowSensorPulseCounterA2()  { flowSensorPulseCountA2++;  }
void flowSensorPulseCounterA3()  { flowSensorPulseCountA3++;  }
void flowSensorPulseCounterA4()  { flowSensorPulseCountA4++;  }
void flowSensorPulseCounterA5()  { flowSensorPulseCountA5++;  }

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

 /* //  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-2\":"));
  resultData.concat(getFlowData2());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-3\":"));
  resultData.concat(getFlowData3());
    resultData.concat(F(","));
 */ resultData.concat(F("\n\t\"k-flow-A0\":"));
  resultData.concat(getFlowDataA0());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-A1\":"));
  resultData.concat(getFlowDataA1());
    resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-A2\":"));
  resultData.concat(getFlowDataA2());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-A3\":"));
  resultData.concat(getFlowDataA3());
    resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-A4\":"));
  resultData.concat(getFlowDataA4());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-A5\":"));
  resultData.concat(getFlowDataA5());
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
/*int getFlowData2()
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
*/
/////////////////////
int getFlowDataA0()
{
  unsigned long flowSensorPulsesPerSecondA0;

  unsigned long deltaTime = millis() - flowSensorLastTimeA0;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSORA0);
  flowSensorPulsesPerSecondA0 = flowSensorPulseCountA0 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTimeA0 = millis();
  flowSensorPulseCountA0 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSORA0, flowSensorPulseCounterA0, FALLING);

  return flowSensorPulsesPerSecondA0;
}

/////////////////////
int getFlowDataA1()
{
  unsigned long flowSensorPulsesPerSecondA1;

  unsigned long deltaTime = millis() - flowSensorLastTimeA1;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSORA1);
  flowSensorPulsesPerSecondA1 = flowSensorPulseCountA1 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTimeA1 = millis();
  flowSensorPulseCountA1 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSORA1, flowSensorPulseCounterA1, FALLING);

  return flowSensorPulsesPerSecondA1;
}

/////////////////////
int getFlowDataA2()
{
  unsigned long flowSensorPulsesPerSecondA2;

  unsigned long deltaTime = millis() - flowSensorLastTimeA2;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSORA2);
  flowSensorPulsesPerSecondA2 = flowSensorPulseCountA2 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTimeA2 = millis();
  flowSensorPulseCountA2 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSORA2, flowSensorPulseCounterA2, FALLING);

  return flowSensorPulsesPerSecondA2;
}


/////////////////////
int getFlowDataA3()
{
  unsigned long flowSensorPulsesPerSecondA3;

  unsigned long deltaTime = millis() - flowSensorLastTimeA3;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSORA3);
  flowSensorPulsesPerSecondA3 = flowSensorPulseCountA3 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTimeA3 = millis();
  flowSensorPulseCountA3 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSORA3, flowSensorPulseCounterA3, FALLING);

  return flowSensorPulsesPerSecondA3;
}

/////////////////////
int getFlowDataA4()
{
  unsigned long flowSensorPulsesPerSecondA4;

  unsigned long deltaTime = millis() - flowSensorLastTimeA4;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSORA4);
  flowSensorPulsesPerSecondA4 = flowSensorPulseCountA4 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTimeA4 = millis();
  flowSensorPulseCountA4 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSORA4, flowSensorPulseCounterA4, FALLING);

  return flowSensorPulsesPerSecondA4;
}

/////////////////////
int getFlowDataA5()
{
  unsigned long flowSensorPulsesPerSecondA5;

  unsigned long deltaTime = millis() - flowSensorLastTimeA5;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSORA5);
  flowSensorPulsesPerSecondA5 = flowSensorPulseCountA5 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTimeA5 = millis();
  flowSensorPulseCountA5 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSORA5, flowSensorPulseCounterA5, FALLING);

  return flowSensorPulsesPerSecondA5;
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
