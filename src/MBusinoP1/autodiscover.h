struct autodiscover {
  char bufferValue[500] = {0};
  char bufferTopic[100] = {0};
  char haName[30] = {0};
  char haUnits[30] = {0};
  char stateClass[30] = {0};
  char deviceClass[30] = {0};
  char deviceClassString[50] = {0};
} adVariables; // home assistand auto discover

const char adValueMbus[] PROGMEM = R"rawliteral({"unique_id":"%s_%s","object_id":"%s_%s","state_topic":"%s/DLMS/%s","name":"%s","value_template":"{{value_json if value_json is defined else 0}}","unit_of_meas":"%s","state_class":"%s","device":{"ids": ["%s"],"name":"%s","manufacturer": "MBusino","mdl":"V%s"},%s"availability_mode":"all"})rawliteral";
const char adTopicMbus[] PROGMEM = R"rawliteral(homeassistant/sensor/%s/%s/config)rawliteral";

void haHandoverMbus(uint8_t haCounter){ // haCounter is the "i+1" at the for() in main

  if(adVariables.deviceClass[0] != 0){
    strcpy(adVariables.deviceClassString,String("\"device_class\": \"" + String(adVariables.deviceClass) + "\",").c_str());
  }
  sprintf(adVariables.bufferValue,adValueMbus,userData.mbusinoName,adVariables.haName,userData.mbusinoName,adVariables.haName,userData.mbusinoName,adVariables.haName,adVariables.haName,adVariables.haUnits,adVariables.stateClass,userData.mbusinoName,userData.mbusinoName,MBUSINO_VERSION,adVariables.deviceClassString);
  sprintf(adVariables.bufferTopic,adTopicMbus,userData.mbusinoName,adVariables.haName);
  client.publish(adVariables.bufferTopic, adVariables.bufferValue, true); 

  adVariables.bufferTopic[0] = 0;
  adVariables.bufferValue[0] = 0;
  adVariables.deviceClass[0] = 0;
  adVariables.deviceClassString[0] = 0;
  adVariables.stateClass[0] = 0;
  adVariables.haUnits[0] = 0;
}




