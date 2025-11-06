

// Decode data
uint8_t decode(uint8_t* plaintext, uint8_t payload_length, JsonArray& root) {
  uint16_t current_position = DECODER_START_OFFSET;
  time_t unix_timestamp = -1;

  char obisString[21] = {0}; //Obis name
  char obisName[21] = {0}; //Obis name 
  char obisUnit[10] = {0}; //Unit
  char jsonTimestamp[21] = {0};
  bool timestampAvailable = false;


  Serial.println("recieved plaintext");
  Serial.println("");
  char telegram3[1024] = { 0 };
  for (uint16_t i = 0; i <= 243; i++) {     //|
    char buffer[3];                         //|
    sprintf(buffer, "%02X", plaintext[i]);  //|
    telegram3[i * 3] = buffer[0];
    telegram3[(i * 3) + 1] = buffer[1];
    telegram3[(i * 3) + 2] = ' ';
  }
  for (uint16_t i = 0; i < 729; i++) {  //|
    Serial.print(telegram3[i]);
  }
  Serial.println("");
  uint8_t count = 0;
  do {
    count ++;
    if (plaintext[current_position + OBIS_TYPE_OFFSET] != DATA_OCTET_STRING) {
      Serial.println("Unsupported OBIS header type!");
      //receive_buffer_index = 0;
      return 0;
    }

    uint8_t data_length = plaintext[current_position + OBIS_LENGTH_OFFSET];

    if (data_length != 0x06) {
      // read timestamp
      if ((data_length == 0x0C) && (current_position == DECODER_START_OFFSET)) {
        uint8_t dateTime[data_length];
        memcpy(&dateTime[0], &plaintext[current_position + 2], data_length);

        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;

        year = (plaintext[current_position + 2] << 8) + plaintext[current_position + 3];
        month = plaintext[current_position + 4];
        day = plaintext[current_position + 5];
        hour = plaintext[current_position + 7];
        minute = plaintext[current_position + 8];
        second = plaintext[current_position + 9];

        char timeStamp[21];
        sprintf(timeStamp, "%02u.%02u.%04u %02u:%02u:%02u", day, month, year, hour, minute, second);

        strlcpy(jsonTimestamp, timeStamp,21);
        timestampAvailable = true;        

        current_position = 34;
        data_length = plaintext[current_position + OBIS_LENGTH_OFFSET];
      } else if ((data_length == 0x0C) && (current_position > 225)) {
        uint8_t meterNumber[data_length];
        memcpy(&meterNumber[0], &plaintext[current_position + 2], data_length);

        // THIS IS THE END OF THE PACKET
        break;
      } else {
        Serial.println("Unsupported OBIS header length");
        //receive_buffer_index = 0;
        return 0;
      }
    }

    uint8_t obis_code[data_length];
    memcpy(&obis_code[0], &plaintext[current_position + OBIS_CODE_OFFSET], data_length);  // Copy OBIS code to array

    current_position += data_length + 2;  // Advance past code, position and type

    uint8_t obis_data_type = plaintext[current_position];
    current_position++;  // Advance past data type

    uint8_t obis_data_length = 0x00;
    uint8_t code_type = TYPE_UNKNOWN;

    if (obis_code[OBIS_A] == 0x01) {
      // Compare C and D against code
      if (memcmp(&obis_code[OBIS_C], OBIS_VOLTAGE_L1, 2) == 0) {
        code_type = TYPE_VOLTAGE_L1;
      } else if (memcmp(&obis_code[OBIS_C], OBIS_VOLTAGE_L2, 2) == 0) {
        code_type = TYPE_VOLTAGE_L2;
      } else if (memcmp(&obis_code[OBIS_C], OBIS_VOLTAGE_L3, 2) == 0) {
        code_type = TYPE_VOLTAGE_L3;
      }

      else if (memcmp(&obis_code[OBIS_C], OBIS_CURRENT_L1, 2) == 0) {
        code_type = TYPE_CURRENT_L1;
      } else if (memcmp(&obis_code[OBIS_C], OBIS_CURRENT_L2, 2) == 0) {
        code_type = TYPE_CURRENT_L2;
      } else if (memcmp(&obis_code[OBIS_C], OBIS_CURRENT_L3, 2) == 0) {
        code_type = TYPE_CURRENT_L3;
      }

      else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_POWER_PLUS, 2) == 0) {
        code_type = TYPE_ACTIVE_POWER_PLUS;
      } else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_POWER_MINUS, 2) == 0) {
        code_type = TYPE_ACTIVE_POWER_MINUS;
      }

      else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_ENERGY_PLUS, 2) == 0) {
        code_type = TYPE_ACTIVE_ENERGY_PLUS;
      } else if (memcmp(&obis_code[OBIS_C], OBIS_ACTIVE_ENERGY_MINUS, 2) == 0) {
        code_type = TYPE_ACTIVE_ENERGY_MINUS;
      }

      else if (memcmp(&obis_code[OBIS_C], OBIS_POWER_FACTOR, 2) == 0) {
        code_type = TYPE_POWER_FACTOR;
      } else {
        Serial.println("Unsupported OBIS code");
      }
    } else {
      Serial.println("Unsupported OBIS medium");
      //receive_buffer_index = 0;
      return 0;
    }

    uint16_t uint16_value;
    uint32_t uint32_value;
    float float_value;

    switch (obis_data_type) {
      case DATA_LONG_DOUBLE_UNSIGNED:
        obis_data_length = 4;

        memcpy(&uint32_value, &plaintext[current_position], 4);  // Copy uint8_ts to integer
        uint32_value = swap_uint32(uint32_value);                // Swap uint8_ts

        float_value = uint32_value;  // Ignore decimal digits for now

        switch (code_type) {
          case TYPE_ACTIVE_POWER_PLUS:
            Serial.print("ActivePowerPlus ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "ActivePowerPlus").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "ActivePowerPlus");
            strcpy(obisUnit, "W");
            strcpy(obisString, "1.0.1.7.0.255");
            break;

          case TYPE_ACTIVE_POWER_MINUS:
            Serial.print("ActivePowerMinus ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "ActivePowerMinus").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "ActivePowerMinus");
            strcpy(obisUnit, "W");
            strcpy(obisString, "1.0.1.7.0.255");
            break;

          case TYPE_ACTIVE_ENERGY_PLUS:
            Serial.print("ActiveEnergyPlus ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "ActiveEnergyPlus").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "ActiveEnergyPlus");
            strcpy(obisUnit, "Wh");
            strcpy(obisString, "1.0.1.8.0.255");
            break;

          case TYPE_ACTIVE_ENERGY_MINUS:
            Serial.print("ActiveEnergyMinus ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "ActiveEnergyMinus").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "ActiveEnergyMinus");
            strcpy(obisUnit, "Wh");
            strcpy(obisString, "1.0.2.8.0.255");
            break;
        }
        break;

      case DATA_LONG_UNSIGNED:
        obis_data_length = 2;

        memcpy(&uint16_value, &plaintext[current_position], 2);  // Copy uint8_ts to integer
        uint16_value = swap_uint16(uint16_value);                // Swap uint8_ts

        if (plaintext[current_position + 5] == SCALE_TENTHS)
          float_value = uint16_value / 10.0;
        else if (plaintext[current_position + 5] == SCALE_HUNDREDTHS)
          float_value = uint16_value / 100.0;
        else if (plaintext[current_position + 5] == SCALE_THOUSANDS)
          float_value = uint16_value / 1000.0;
        else
          float_value = uint16_value;

        switch (code_type) {
          case TYPE_VOLTAGE_L1:
            Serial.print("VoltageL1 ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "VoltageL1").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "VoltageL1");
            strcpy(obisUnit, "V");
            strcpy(obisString, "1.0.32.7.0.255");
            break;

          case TYPE_VOLTAGE_L2:
            Serial.print("VoltageL2 ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "VoltageL2").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "VoltageL2");
            strcpy(obisUnit, "V");
            strcpy(obisString, "1.0.52.7.0.255");
            break;

          case TYPE_VOLTAGE_L3:
            Serial.print("VoltageL3 ");
            Serial.println(float_value);
            ////client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "VoltageL3").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "VoltageL3");
            strcpy(obisUnit, "V");
            strcpy(obisString, "1.0.72.7.0.255");
            break;

          case TYPE_CURRENT_L1:
            Serial.print("CurrentL1 ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "CurrentL1").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "CurrentL1");
            strcpy(obisUnit, "A");
            strcpy(obisString, "1.0.31.7.0.255");            
            break;

          case TYPE_CURRENT_L2:
            Serial.print("CurrentL2 ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "CurrentL2").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "CurrentL2");
            strcpy(obisUnit, "A");
            strcpy(obisString, "1.0.51.7.0.255");
            break;

          case TYPE_CURRENT_L3:
            Serial.print("CurrentL3 ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "CurrentL3").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "CurrentL3");
            strcpy(obisUnit, "A");
            strcpy(obisString, "1.0.71.7.0.255");
            break;

          case TYPE_POWER_FACTOR:
            Serial.print("PowerFactor ");
            Serial.println(float_value);
            //client.publish(String(String(userData.mbusinoName) + "/DLMS/" + "PowerFactor").c_str(), String(float_value, 3).c_str());  // send the value if a real value is aviable (standard)
            strcpy(obisName, "PowerFactor");
            strcpy(obisUnit, "No Unit");
            strcpy(obisString, "1.0.13.7.0.255");
            break;
        }
        break;

      case DATA_OCTET_STRING:
        obis_data_length = plaintext[current_position];
        current_position++;  // Advance past string length
        break;

      default:
        Serial.println("Unsupported OBIS data type");
        //receive_buffer_index = 0;
        return 0;
        break;
    }
    
    // Init object
    
    if(timestampAvailable == true){
      JsonObject data = root.add<JsonObject>();
      timestampAvailable = false;
      data["timestamp"] = String(jsonTimestamp);
    }
    JsonObject data = root.add<JsonObject>();
    data["obis"] = String(obisString);
    data["name"] = String(obisName);
    data["value_scaled"] = float_value;
    data["units"] = String(obisUnit);    


    current_position += obis_data_length;  // Skip data length

    current_position += 2;  // Skip pause after data

    if (plaintext[current_position] == 0x0F){     // There is still additional data for this type, skip it
      current_position += 4; 
    }                    // Skip additional data and additional break; this will jump out of bounds on last frame
  } while (current_position <= payload_length);  // Loop until end of packet

  //receive_buffer_index = 0;
  Serial.println("Received valid data!");
  return count;
}
