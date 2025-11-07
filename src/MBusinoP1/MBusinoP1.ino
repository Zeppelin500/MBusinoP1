/*
   MBusino DLMS/COSEM Smartmeter decoder for MBUS Slave by Zeppelin500 Copyright (C) 2025 

   Decrypt and decote code is from Hartmut Wendt  www.zihatec.de
   
   This application will decode the data by an dlms smartmeter like Kaifa MA309M
   or SAGEMCOM T210-D

   You will need a special decrypt key for your meter by your electricity supplier.


   Additional libraries: Arduino Crypto Library by Rhys Weatherley 
                         https://rweather.github.io/arduinolibs/crypto.html


   
   Based on ESP32/8266 Smartmeter decoder for Kaifa MA309M code by FKW9, EIKSEUand others
   https://github.com/FKW9/esp-smartmeter-netznoe/


   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <SPI.h>
#include <ETH.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

#include <Arduino.h>
#include "DlmsCosemLib.h"

#include <Crypto.h>
#include <AES.h>
#include <GCM.h>

#include <Wire.h>

#define MBUSINO_VERSION "0.1.0"

HardwareSerial MBusSerial(1);

// Pins for an ESP32 C3 Supermini standard SPI
#ifndef ETH_PHY_CS
#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 0
#define ETH_PHY_CS 7
#define ETH_PHY_IRQ 2
#define ETH_PHY_RST 3
#define ETH_PHY_SPI_HOST SPI2_HOST
#define ETH_PHY_SPI_SCK 4
#define ETH_PHY_SPI_MISO 5
#define ETH_PHY_SPI_MOSI 6
#endif

static bool eth_connected = false;

NetworkClient ethClient;
HardwareSerial MbusSerial(1);
WiFiClient wfClient;
PubSubClient client;
DNSServer dnsServer;
AsyncWebServer server(80);
DlmsCosemLib DlmsCosem;

struct settings {
  char ssid[30];
  char password[30];
  char mbusinoName[21];
  char broker[64];
  uint16_t mqttPort;
  uint16_t extension;
  char mqttUser[30];
  char mqttPswrd[30];
  char key[33];
  bool haAutodisc;
  bool telegramDebug;
} userData = { "SSID", "Password", "MBusino", "192.168.1.8", 1883, 5, "mqttUser", "mqttPasword", "empty", true, false };

bool mqttcon = false;
bool apMode = false;
bool credentialsReceived = false;
uint16_t conCounter = 0;

uint8_t mbusLoopStatus = 0;
int8_t fields = 0;
uint8_t recordCounter = 0;    // count the received records for multible telegrams
char jsonstring[4096] = { 0 };
bool waitForRestart = false;
bool wifiReconnect = false;
uint8_t usedMQTTconection = 0;
bool networkLost = false;
bool gotIP = false;

unsigned long timerMQTT = 15000;
unsigned long timerDebug = 0;
unsigned long timerReconnect = 0;
unsigned long timerWifiReconnect = 0;
unsigned long timerReboot = 0;
unsigned long timerAutodiscover = 0;
unsigned long timerNetworkChange = 0;
unsigned long timerETHmessage = 0;

void setupServer();

uint8_t eeAddrCalibrated = 0;
uint8_t eeAddrCredentialsSaved = 32;
uint16_t credentialsSaved = 123;  // shows if EEPROM used befor for credentials

uint8_t adMbusMessageCounter = 0;  // Counter for autodiscouver mbus message.

uint32_t minFreeHeap = 0;

uint32_t last_read = 0;                       // Timestamp when data was last read
uint16_t receive_buffer_index = 0;            // Current position in the receive buffer
uint8_t receive_buffer[RECEIVE_BUFFER_SIZE];  // Stores the received data

#define packet_size 256  // Sagemcom

GCM<AES128> *gcmaes128 = 0;

uint8_t hexKey[16] = { 0 };


//outsourced program parts
#include "networkEvents.h"
#include "html.h"
#include "mqtt.h"
#include "guiServer.h"
#include "autodiscover.h"

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);  // LED on if MQTT connectet to server
  digitalWrite(LED_BUILTIN, HIGH);
  // Debug port
  Serial.begin(9600);

  // init decryption
  gcmaes128 = new GCM<AES128>();

  // MBus input from MBus Wing
  MBusSerial.begin(2400, SERIAL_8E1, 20, 21);
  MBusSerial.setTimeout(2);

  minFreeHeap = ESP.getFreeHeap();

  EEPROM.begin(512);
  EEPROM.get(eeAddrCredentialsSaved, credentialsSaved);
  if (credentialsSaved == 500) {
    EEPROM.get(100, userData);
  }
  EEPROM.commit();
  EEPROM.end();

  if (userData.telegramDebug > 1) {
    userData.telegramDebug = 0;
  }

  sprintf(html_buffer, index_html, userData.ssid, userData.mbusinoName, userData.haAutodisc, userData.telegramDebug, userData.key, userData.broker, userData.mqttPort, userData.mqttUser);

  WiFi.onEvent(WiFiEvent);
  WiFi.hostname(userData.mbusinoName);
  WiFi.mode(WIFI_STA);
  WiFi.begin(userData.ssid, userData.password);

  Network.onEvent(onEvent);
  delay(100);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, ETH_PHY_SPI_HOST, ETH_PHY_SPI_SCK, ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI);
  delay(2000);

  byte tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (tries++ > 2) {
      if (eth_connected == false) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("MBusino Setup Portal");  //, "secret");
        apMode = true;
        break;
      } else {
        WiFi.mode(WIFI_OFF);
        Serial.println("WIFI set to off, no known Network and Ethernet is already connected");
        break;
      }
    }
  }

  if (eth_connected == true) {
    client.setClient(ethClient);
    usedMQTTconection = 1;
    Serial.println("MQTT set connection via Ethernet");
  } else {
    client.setClient(wfClient);
    usedMQTTconection = 2;
    Serial.println("MQTT set connection via Wifi");
  }

  client.setServer(userData.broker, userData.mqttPort);

  setupServer();
  if (apMode == true) {
    dnsServer.start(53, "*", WiFi.softAPIP());
  }

  setupServer();
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  //only when requested from AP


  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", (uint8_t *)update_html, update_htmlLength);
  });
  server.on(
    "/update", HTTP_POST, [](AsyncWebServerRequest *request) {
      waitForRestart = !Update.hasError();
      if (Update.hasError() == true) {
        timerReboot = millis();
      }
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", waitForRestart ? "success, restart now" : "FAIL");
      response->addHeader("Connection", "close");
      request->send(response);
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
          Update.printError(Serial);
        }
      }
      if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        }
      }
      if (final) {
        if (Update.end(true)) {
          Serial.printf("Update Success: %uB\n", index + len);
        } else {
          Update.printError(Serial);
        }
      }
    });

  ArduinoOTA.setPassword((const char *)"mbusino");
  server.onNotFound(onRequest);
  ArduinoOTA.begin();
  server.begin();

  client.setBufferSize(6000);



  Serial.println(F("Start"));
  for (int i = 0; i < strlen(userData.key); i += 2) {
    hexKey[i / 2] = H2Digit2Dez(userData.key, i);
    Serial.print(hexKey[i / 2], HEX);
    Serial.print(" ");
  }
  Serial.println();
}


void loop() {

  heapCalc();

  ArduinoOTA.handle();


  if (gotIP == false && eth_connected == true && (millis() - timerETHmessage > 2000) && waitForRestart == false) {
    Serial.println("no IP received, restart ETH");
    timerETHmessage = millis();
    ETH.end();
    delay(200);
    //SPI.end();
    delay(200);
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, ETH_PHY_SPI_HOST, ETH_PHY_SPI_SCK, ETH_PHY_SPI_MISO, ETH_PHY_SPI_MOSI, 16);
  }
  /*
    if(gotIP == false && eth_connected == true && (millis() - timerETHmessage > 1000) && waitForRestart == false){
        Serial.println("no IP received, restart soon");
        timerReboot = millis();
        waitForRestart = true;
    }

    */

  if (wifiReconnect == true && (millis() - timerWifiReconnect > 2000)) {
    Serial.println("try to reconnect");
    wifiReconnect = false;
    WiFi.reconnect();
  }


  if (credentialsReceived == true && waitForRestart == false) {
    Serial.println("credentials received, save and restart soon");
    EEPROM.begin(512);
    EEPROM.put(100, userData);
    credentialsSaved = 500;
    EEPROM.put(eeAddrCredentialsSaved, credentialsSaved);
    EEPROM.commit();
    EEPROM.end();
    timerReboot = millis();
    waitForRestart = true;
  }

  if (waitForRestart == true && (millis() - timerReboot) > 1000) {
    Serial.println("restart");
    ESP.restart();
  }

  if ((apMode == true && eth_connected == false) && millis() > 300000) {
    ESP.restart();
  }

  if (apMode == true) {
    dnsServer.processNextRequest();
  }

  //to notice Changes in Network an assign the right connection to the MQTT client
  if ((eth_connected == false) && (usedMQTTconection == 1) && (millis() - timerNetworkChange) > 5000) {
    client.setClient(wfClient);
    usedMQTTconection = 2;
    Serial.println("MQTT set connection via Wifi");
    reconnect();
    timerNetworkChange = millis();
  }
  if ((eth_connected == true) && (usedMQTTconection == 2) && (millis() - timerNetworkChange) > 5000) {
    client.setClient(ethClient);
    usedMQTTconection = 1;
    Serial.println("MQTT set connection via Ethernet");
    reconnect();
    timerNetworkChange = millis();
  }

  if (!client.connected() && ((millis() - timerReconnect) > 5000)) {
    Serial.println("MQTT no connection");
    reconnect();
    timerReconnect = millis();
  } else {          // the whole main code run only if MQTT is connectet
    client.loop();  //MQTT Funktion
    if ((millis() - timerDebug) > 10000) {
      timerDebug = millis();

      client.publish(String(String(userData.mbusinoName) + "/settings/wl_Connected").c_str(), String(WL_CONNECTED).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/apMode").c_str(), String(apMode).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/eth_connected").c_str(), String(eth_connected).c_str());

      client.publish(String(String(userData.mbusinoName) + "/settings/ssid").c_str(), userData.ssid);
      //client.publish(String(String(userData.mbusinoName) + "/settings/password").c_str(), String(userData.password));
      client.publish(String(String(userData.mbusinoName) + "/settings/broker").c_str(), userData.broker);
      client.publish(String(String(userData.mbusinoName) + "/settings/port").c_str(), String(userData.mqttPort).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/user").c_str(), userData.mqttUser);
      //client.publish(String(String(userData.mbusinoName) + "/settings/pswd").c_str(), userData.mqttPswrd);
      client.publish(String(String(userData.mbusinoName) + "/settings/name").c_str(), userData.mbusinoName);
      client.publish(String(String(userData.mbusinoName) + "/settings/key").c_str(), userData.key);
      client.publish(String(String(userData.mbusinoName) + "/settings/wifiIP").c_str(), String(WiFi.localIP().toString()).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/ethIP").c_str(), String(ETH.localIP().toString()).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/MQTTreconnections").c_str(), String(conCounter - 1).c_str());
      if (usedMQTTconection == 1) {
        client.publish(String(String(userData.mbusinoName) + "/settings/MQTTconnectedVia").c_str(), "ethernet");
      }
      if (usedMQTTconection == 2) {
        client.publish(String(String(userData.mbusinoName) + "/settings/MQTTconnectedVia").c_str(), "WiFi");
      }
      long rssi = WiFi.RSSI();
      client.publish(String(String(userData.mbusinoName) + "/settings/RSSI").c_str(), String(rssi).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/version").c_str(), MBUSINO_VERSION);
      client.publish(String(String(userData.mbusinoName) + "/settings/freeHeap").c_str(), String(ESP.getFreeHeap()).c_str());
      client.publish(String(String(userData.mbusinoName) + "/settings/minFreeHeap").c_str(), String(minFreeHeap).c_str());

      Serial.println(userData.key);
    }
  }


  uint32_t current_time = millis();

  // Read while data is available
  while (MBusSerial.available()) {
    if (receive_buffer_index >= RECEIVE_BUFFER_SIZE) {
      Serial.println("Buffer overflow!");
      receive_buffer_index = 0;
    }

    receive_buffer[receive_buffer_index++] = MBusSerial.read();

    last_read = current_time;
  }

  if (receive_buffer_index > 0 && current_time - last_read > READ_TIMEOUT) {
    if (receive_buffer_index < packet_size) {
      Serial.println("Received packet with invalid size!");
      Serial.println(receive_buffer_index);
      receive_buffer_index = 0;
      return;
    }

    /**
         * @TODO: ADD ROUTINE TO DETERMINE PAYLOAD LENGTHS AUTOMATICALLY    Vorschlag       wenn Byte 20 <= 127 dann Byte 20 -5
                                                                                            wenn Byte 20 0x81 dann Byte 21 -5 
                                                                                            wenn Byte 20 0x82 dann Byte 21/22-5
                                                                                            erledigt, funktioniert
         */

    uint8_t ciField = receive_buffer[6];
    bool segmentation = false;       // segementation of the payöoad in 2 telegrams
    if ((ciField & 0x08) == 0x00) {  // Segmentation active
      segmentation = true;
    }

    uint16_t payload_length = 0;    //243
    uint8_t lengthFieldOffset = 0;  // aplication length field differs from 1 to 3 Byte
    if (receive_buffer[19] < 127) {
      payload_length = receive_buffer[19] - 5;
    } else if (receive_buffer[19] = 127) {
      payload_length = receive_buffer[20] - 5;
      lengthFieldOffset = 1;
    } else if (receive_buffer[19] > 127) {
      payload_length = ((uint16_t)receive_buffer[20] << 8 | receive_buffer[21]) - 5;
      lengthFieldOffset = 2;
    }

    //test
    uint16_t payload_lengthTest = ((uint16_t)receive_buffer[20] << 8 | receive_buffer[21]) - 5;
    Serial.println("length of payload");
    Serial.println(receive_buffer[19]);
    Serial.println(receive_buffer[20]);
    Serial.println(receive_buffer[21]);
    Serial.println(payload_lengthTest);
    //test

    uint16_t payload_length_msg1 = receive_buffer[1] - MBUS_DLMS_HADER - lengthFieldOffset;  // 228 Length of the first M-Bus telegram minus M-Bus header;
    uint16_t payload_length_msg2 = payload_length - payload_length_msg1;

    uint8_t iv[12];  // Initialization vector

    memcpy(&iv[0], &receive_buffer[DLMS_SYST_OFFSET], DLMS_SYST_LENGTH);  // Copy system title to IV
    memcpy(&iv[8], &receive_buffer[DLMS_IC_OFFSET], DLMS_IC_LENGTH);      // Copy invocation counter to IV

    uint8_t ciphertext[payload_length];
    memcpy(&ciphertext[0], &receive_buffer[DLMS_HEADER1_LENGTH], payload_length_msg1);
    if (segmentation == true) {
      memcpy(&ciphertext[payload_length_msg1], &receive_buffer[DLMS_HEADER2_OFFSET + DLMS_HEADER2_LENGTH], payload_length_msg2);
    }
    // Start decrypting
    uint8_t plaintext[payload_length];

    gcmaes128->setKey(hexKey, gcmaes128->keySize());
    gcmaes128->setIV(iv, 12);
    gcmaes128->decrypt(plaintext, ciphertext, payload_length);

    if (plaintext[0] != 0x0F || plaintext[5] != 0x0C) {
      Serial.println("Packet was decrypted but data is invalid!");
      receive_buffer_index = 0;
      return;
    }

    //decode data
    JsonDocument jsonBuffer;
    JsonArray root = jsonBuffer.add<JsonArray>();
    fields = DlmsCosem.decode(&plaintext[0], payload_length, root);
    Serial.println(fields);
    serializeJson(root, jsonstring);  // store the json in a global array
    client.publish(String(String(userData.mbusinoName) + "/DLMS/jsonstring").c_str(), jsonstring);




    // Send Records
    if (fields > 0) {
      adMbusMessageCounter++;

      const char *timestamp = root[0]["timestamp"];
      const char *meternumber = root[fields]["meter_number"]; // after the last Record  
      client.publish(String(String(userData.mbusinoName) + "/DLMS/Meternumber").c_str(), meternumber);
      Serial.println(String("MeterNumber = " + String(meternumber)).c_str());  
      client.publish(String(String(userData.mbusinoName) + "/DLMS/Timestamp").c_str(), timestamp);
      Serial.println(String("Timestamp = " + String(timestamp)).c_str());

      if(userData.haAutodisc == true && adMbusMessageCounter == 3){  //every 264 message is a HA autoconfig message
        strcpy(adVariables.haName,"Meternumber");
        strcpy(adVariables.haUnits,"");         
        strcpy(adVariables.stateClass,"total");
        strcpy(adVariables.deviceClass,"");     
        haHandoverMbus(0);

        strcpy(adVariables.haName,"Timestamp");
        strcpy(adVariables.haUnits,"");         
        strcpy(adVariables.stateClass,"total");
        strcpy(adVariables.deviceClass,"timestamp");     // the format is not HA compatible yet strcpy(adVariables.deviceClass,"timestamp");   
        haHandoverMbus(0);        
      }


      for (uint8_t i = 1; i < fields; i++) {
        uint8_t code = root[i]["code"].as<int>();
        const char *obisString = root[i]["obis"];
        const char *name = root[i]["name"];
        const char *units = root[i]["units"];
        double value = root[i]["value_scaled"].as<double>();

        client.publish(String(String(userData.mbusinoName) + "/DLMS/" + String(name) + "_OBIScode").c_str(), obisString);
        client.publish(String(String(userData.mbusinoName) + "/DLMS/" + String(name)).c_str(), String(value, 3).c_str());
        client.publish(String(String(userData.mbusinoName) + "/DLMS/" + String(name) + "_unit").c_str(), units);

        //Serial.println(String("OBIScode = " + String(obisString)).c_str());
        Serial.println(String(String(name) + " = " + String(value, 3)).c_str());
        //Serial.println(String(String(name) + " = " + String(units)).c_str());

        if(userData.haAutodisc == true && adMbusMessageCounter == 3){  //every 264 message is a HA autoconfig message
          strcpy(adVariables.haName,name);
          if(units != NULL){
            strcpy(adVariables.haUnits,units);
          }else{
            strcpy(adVariables.haUnits,""); 
          }
          strcpy(adVariables.stateClass,DlmsCosem.getStateClass(code));
          strcpy(adVariables.deviceClass,DlmsCosem.getDeviceClass(code));     
          haHandoverMbus(i+1);
        }
      }
    }else{
        Serial.println(String("Failure Code = " + String(fields)).c_str());
        delay(5);
        char error[40] = {0};//DlmsCosem.getError(fields);
        strcpy(error,DlmsCosem.getError(fields));
        client.publish(String(String(userData.mbusinoName) + "/DLMS/error").c_str(), error);
        Serial.println(String("error = " + String(error)).c_str());
    }
    fields = 0;
    receive_buffer_index = 0;
  }
}


void heapCalc() {
  if (minFreeHeap > ESP.getFreeHeap()) {
    minFreeHeap = ESP.getFreeHeap();
  }
}

byte HDigit2Dez(const char c) {
  if (c >= 'A') return 10 + c - 'A';
  return c - '0';
}

byte H2Digit2Dez(const char *arr, byte idx) {
  return HDigit2Dez(arr[idx]) * 16 + HDigit2Dez(arr[idx+1]);
}
