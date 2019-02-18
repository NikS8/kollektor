/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
                                                    kollektor.ino 
                                Copyright © 2019, Zigfred & Nik.S
31.01.2019 v1
18.02.2019 V2 Прерывания PCINT0_vect, PCINT1_vect и PCINT2_vect
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
#define VERSION 2

#define RESET_UPTIME_TIME 43200000  //  = 30 * 24 * 60 * 60 * 1000 
                                    // reset after 30 days uptime 

//IPAddress ip(192, 168, 1, 159); 
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x88, 0x88};
EthernetServer httpServer(40159);
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

volatile long flowSensorPulseCount4 = 0;
volatile long flowSensorPulseCount5 = 0;
volatile long flowSensorPulseCount6 = 0;
volatile long flowSensorPulseCount7 = 0;
volatile long flowSensorPulseCountA0 = 0;
volatile long flowSensorPulseCountA1 = 0;
volatile long flowSensorPulseCountA2 = 0;
volatile long flowSensorPulseCountA3 = 0;
volatile long flowSensorPulseCountA4 = 0;
volatile long flowSensorPulseCountA5 = 0;
// time
uint32_t flowSensorLastTime2;
uint32_t flowSensorLastTime3;
uint32_t flowSensorLastTime4;
uint32_t flowSensorLastTime5;
uint32_t flowSensorLastTime6;
uint32_t flowSensorLastTime7;

// timers
RBD::Timer ds18ConversionTimer;
/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            PCINT0_vect, PCINT1_vect и PCINT2_vect
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  //////  Обработчики прерываний три, и их векторы
  //////  называются PCINT0_vect, PCINT1_vect и PCINT2_vect. 
  ISR(PCINT2_vect){
///    if (!(PIND & (1 << PD0))) {/* Arduino pin 0 interrupt*/} //  RX
///    if (!(PIND & (1 << PD1))) {/* Arduino pin 1 interrupt*/} //  TX
///    if (!(PIND & (1 << PD2))) {/* Arduino pin 2 interrupt*/} //  INT0
///    if (!(PIND & (1 << PD3))) {/* Arduino pin 3 interrupt*/} //  INT1
    if (!(PIND & (1 << PD4))) {flowSensorPulseCounter4();}
    if (!(PIND & (1 << PD5))) {flowSensorPulseCounter5();}
    if (!(PIND & (1 << PD6))) {flowSensorPulseCounter6();}
    if (!(PIND & (1 << PD7))) {flowSensorPulseCounter7();}
  }
 
///  isr(PCINT1_vect) {
///    if (!(PINC & (1 << PC0))) {flowSensorPulseCounter4();}
///    if (!(PINC & (1 << PC1))) {flowSensorPulseCounter5();}
///    if (!(PINC & (1 << PC2))) {flowSensorPulseCounter6();}
///    if (!(PINC & (1 << PC3))) {flowSensorPulseCounter7();}
///    if (!(PINC & (1 << PC4))) {flowSensorPulseCounter4();}
///    if (!(PINC & (1 << PC5))) {flowSensorPulseCounter5();}
///  }
 
///  isr(PCINT0_vect) { //  занято DS18B20 и W5100 или W5500
///    if (!(PINB & (1 << PB0))) {/* Arduino pin 8 interrupt*/}
///    if (!(PINB & (1 << PB1))) {/* Arduino pin 9 interrupt*/}
///    if (!(PINB & (1 << PB2))) {/* Arduino pin 10 interrupt*/}
///    if (!(PINB & (1 << PB3))) {/* Arduino pin 11 interrupt*/}
///    if (!(PINB & (1 << PB4))) {/* Arduino pin 12 interrupt*/}
///    if (!(PINB & (1 << PB5))) {/* Arduino pin 13 interrupt*/}
///  }

  //////

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*\
            setup
\*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setup() {
  Serial.begin(9600);
  Ethernet.begin(mac);
  while (!Serial) continue;
/*
  pinMode(PIN_FLOW_SENSOR2, INPUT);
    attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR2, flowSensorPulseCount2, FALLING);
  sei();
  pinMode(PIN_FLOW_SENSOR3, INPUT);
    attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCount3, FALLING);
  sei();
*/
  //////  Прерывания, которые называются PCINT (Pin Change Interrupts). 
  //////  Эти прерывания герерируются, когда изменяется состояния пина. 
  //////  Все такие прерывания делятся на три группы
  //////  Переведем пины в режим вход с подтяжкой
  PORTB = 0b00111111;
  PORTC = 0b01111111;
  PORTD = 0b11111111;
  //////  Разрешим прерывание на нужной линии, 
  //////  сделав запись в регистры PCMSK0, PCMSK1 или PCMSK2:
  PCMSK0 = 0b00111111;
  PCMSK1 = 0b01111111;
  PCMSK2 = 0b11111111;
  //////  Разрешим уже сами прерывания в общем:
  //////  Бит 0 в регистре PCICR отвечает за группу 0, 
  //////  бит 1 за группу 1, бит 2 за группу 2. 
  PCICR = 0b00000111; //  Разрешили все три группы.
  //////

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
  SWITCH_TO_W5100;

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
    flowSensorPulseCount2++;// INT0
}

void flowSensorPulseCounter3()
{
    flowSensorPulseCount3++;// INT1
}

void flowSensorPulseCounter4()  { flowSensorPulseCount4++;  }  // PCINT2
void flowSensorPulseCounter5()  { flowSensorPulseCount5++;  }  // PCINT2
void flowSensorPulseCounter6()  { flowSensorPulseCount6++;  }  // PCINT2
void flowSensorPulseCounter7()  { flowSensorPulseCount7++;  }  // PCINT2

void flowSensorPulseCounterA0()  { flowSensorPulseCountA0++;  }  // PCINT1
void flowSensorPulseCounterA1()  { flowSensorPulseCountA1++;  }  // PCINT1
void flowSensorPulseCounterA2()  { flowSensorPulseCountA2++;  }  // PCINT1
void flowSensorPulseCounterA3()  { flowSensorPulseCountA3++;  }  // PCINT1
void flowSensorPulseCounterA4()  { flowSensorPulseCountA4++;  }  // PCINT1
void flowSensorPulseCounterA5()  { flowSensorPulseCountA5++;  }  // PCINT1


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
    resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-4\":"));
  resultData.concat(getFlowData4());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-5\":"));
  resultData.concat(getFlowData5());
    resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-6\":"));
  resultData.concat(getFlowData6());
  resultData.concat(F(","));
  resultData.concat(F("\n\t\"k-flow-7\":"));
  resultData.concat(getFlowData7());
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

/////////////////////
int getFlowData4()
{
  unsigned long flowSensorPulsesPerSecond4;

  unsigned long deltaTime = millis() - flowSensorLastTime4;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3);
  flowSensorPulsesPerSecond4 = flowSensorPulseCount4 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTime4 = millis();
  flowSensorPulseCount4 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCounter3, FALLING);

  return flowSensorPulsesPerSecond4;
}

/////////////////////
int getFlowData5()
{
  unsigned long flowSensorPulsesPerSecond5;

  unsigned long deltaTime = millis() - flowSensorLastTime5;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3);
  flowSensorPulsesPerSecond5 = flowSensorPulseCount5 * 1000 / deltaTime;
  ///flowSensorPulsesPerSecond3 *= 1000;
  ///flowSensorPulsesPerSecond3 /= deltaTime; //  количество за секунду

  flowSensorLastTime5 = millis();
  flowSensorPulseCount5 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCounter3, FALLING);

  return flowSensorPulsesPerSecond5;
}
/////////////////////
int getFlowData6()
{
  unsigned long flowSensorPulsesPerSecond6;

  unsigned long deltaTime = millis() - flowSensorLastTime6;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3);
///  flowSensorPulsesPerSecond6 = flowSensorPulseCount6 * 1000 / deltaTime;
  flowSensorPulsesPerSecond6 *= 1000;
  flowSensorPulsesPerSecond6 /= deltaTime; //  количество за секунду

  flowSensorLastTime6 = millis();
  flowSensorPulseCount6 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCounter3, FALLING);

  return flowSensorPulsesPerSecond6;
}
/////////////////////
int getFlowData7()
{
  unsigned long flowSensorPulsesPerSecond7;

  unsigned long deltaTime = millis() - flowSensorLastTime7;
  if (deltaTime < 1000)  { return; }

///   detachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3);
///  flowSensorPulsesPerSecond7 = flowSensorPulseCount7 * 1000 / deltaTime;
  flowSensorPulsesPerSecond7 *= 1000;
  flowSensorPulsesPerSecond7 /= deltaTime; //  количество за секунду

  flowSensorLastTime7 = millis();
  flowSensorPulseCount7 = 0;
///  attachInterrupt(PIN_INTERRUPT_FLOW_SENSOR3, flowSensorPulseCounter3, FALLING);

  return flowSensorPulsesPerSecond7;
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
