#include <Arduino.h>
#include <RadioLib.h>
#include "sensor.h"
#include "detector.h"

#define INITIAL_CONNECTION 

// no need to configure pins, signals are routed to the radio internally
STM32WLx radio = new STM32WLx_Module();
Adafruit_BMP3XX bmp390;
Adafruit_ADXL343 adxl343(ACCEL_SENSOR_ID);
Detector detector;

// set RF switch configuration for Nucleo WL55JC1
// NOTE: other boards may be different!
//       Some boards may not have either LP or HP.
//       For those, do not set the LP/HP entry in the table.
static const uint32_t rfswitch_pins[] =
                         {PA4,  PA5, RADIOLIB_NC};
static const Module::RfSwitchMode_t rfswitch_table[] = {
  {STM32WLx::MODE_IDLE,  {LOW,  LOW,  LOW}},
  {STM32WLx::MODE_RX,    {HIGH, LOW, LOW}},
  {STM32WLx::MODE_TX_LP, {HIGH, HIGH, LOW}},
  {STM32WLx::MODE_TX_HP, {LOW, HIGH,  LOW}},
  END_OF_MODE_TABLE,
};

// create the node instance on the EU-868 band
// using the radio module and the encryption key
// make sure you are using the correct band
// based on your geographical location!
LoRaWANNode node(&radio, &US915);


void setup() {
  Serial.begin(115200);

  // setup the sensors
  setupSensors();

  // initialize SX1278 with default settings
  Serial.print(F("[SX1278] Initializing ... "));
  radio.setRfSwitchTable(rfswitch_pins, rfswitch_table);
  int state = radio.setTCXO(1.7);
  state = radio.begin();
  
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while(true);
  }

  #ifdef INITIAL_CONNECTION
    delay(1000);
    node.wipe();
  #endif

  // application identifier - pre-LoRaWAN 1.1.0, this was called appEUI
  // when adding new end device in TTN, you will have to enter this number
  // you can pick any number you want, but it has to be unique
  uint64_t joinEUI = 0x0000000000001234;

  // device identifier - this number can be anything
  // when adding new end device in TTN, you can generate this number,
  // or you can set any value you want, provided it is also unique
  uint64_t devEUI = 0x0080e115003bb1e0;

  // select some encryption keys which will be used to secure the communication
  // there are two of them - network key and application key
  // because LoRaWAN uses AES-128, the key MUST be 16 bytes (or characters) long

  // network key is the ASCII string "topSecretKey1234"
  uint8_t nwkKey[] = { 0x74, 0x6F, 0x70, 0x53, 0x65, 0x63, 0x72, 0x65,
                       0x74, 0x4B, 0x65, 0x79, 0x31, 0x32, 0x33, 0x34 };
                       //746F705365637265744B657931323334

  // application key is the ASCII string "aDifferentKeyABC"
  uint8_t appKey[] = { 0x61, 0x44, 0x69, 0x66, 0x66, 0x65, 0x72, 0x65,
                       0x6E, 0x74, 0x4B, 0x65, 0x79, 0x41, 0x42, 0x43 };
                        //61446966666572656E744B6579414243

  // prior to LoRaWAN 1.1.0, only a single "nwkKey" is used
  // when connecting to LoRaWAN 1.0 network, "appKey" will be disregarded
  // and can be set to NULL

  // some frequency bands only use a subset of the available channels
  // you can set the starting channel and their number
  // for example, the following corresponds to US915 FSB2 in TTN
  
  //  node.startChannel = 8;
  //  node.numChannels = 8;
  
  #ifdef INITIAL_CONNECTION 
    // now we can start the activation
    // this can take up to 20 seconds, and requires a LoRaWAN gateway in range
    Serial.print(F("[LoRaWAN] Attempting over-the-air activation ... "));
    bool connected = false;
    while(!connected) {
      state = node.beginOTAA(joinEUI, devEUI, nwkKey, appKey);
      if(state == RADIOLIB_ERR_NONE) {
        connected = true;
        Serial.println(F("success!"));
      } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        delay(500);
      }
    }
  #else
    Serial.print(F("[LoRaWAN] Resuming previous session ... "));
    state = node.begin();
    if(state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while(true);
    }
  #endif
}

// counter to keep track of transmitted packets
int count = 0;

void loop() {
  // send uplink to port 10
  Serial.print(F("[LoRaWAN] Sending uplink packet ... "));
  String strUp = "Hello World! #" + String(count++);
  int state = node.uplink(strUp, 10);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("send message success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while(1);
  }
  
  // after uplink, you can call downlink(),
  // to receive any possible reply from the server
  // this function must be called within a few seconds
  // after uplink to receive the downlink!
  Serial.print(F("[LoRaWAN] Waiting for downlink ... "));
  String strDown;
  state = node.downlink(strDown);
  if(state == RADIOLIB_ERR_NONE) {
    Serial.println(F("downlink success!"));

    // print data of the packet (if there are any)
    Serial.print(F("[LoRaWAN] Data:\t\t"));
    if(strDown.length() > 0) {
      Serial.println(strDown);
    } else {
      Serial.println(F("<MAC commands only>"));
    }

    // print RSSI (Received Signal Strength Indicator)
    Serial.print(F("[LoRaWAN] RSSI:\t\t"));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));

    // print SNR (Signal-to-Noise Ratio)
    Serial.print(F("[LoRaWAN] SNR:\t\t"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));

    // print frequency error
    Serial.print(F("[LoRaWAN] Frequency error:\t"));
    Serial.print(radio.getFrequencyError());
    Serial.println(F(" Hz"));
  
  } else if(state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println(F("downlink timeout!"));
  
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
  }

  // wait before sending another packet
  delay(10000);
}