# MBusinoP1 Tutorial

1. Download the .bin files from https://github.com/Zeppelin500/MBusinoP1/tree/main/src/MBusinoP1/build/esp32.esp32.makergo_c3_supermini

2. use the flash [tutorial](https://github.com/Zeppelin500/MBusinoP1/blob/main/tutorial/Flashing_ESP32_C3_Supermini/README.md)

3. Search the MBusino Wlan Access-Point "MBusino Setup Portal" or connect Ethernet and go to the GUI. If Ethernet already connected at power on, WiFi will be disabled and no Accesspoint is running.

4. Configure your Network, Mqtt-Broker and decryption key. If you want to use Ethernet, leave the WiFi fields empty.

![SetupPortal](https://github.com/Zeppelin500/MBusino/blob/main/pictures/MBusino_Setup_Portal.jpg)

* With multible MBusino in network, you have to change the name or it caouse in network problems .
* At MQTT Broker you can set an IP or a address. E.g. **192.168.1.7** or **test.mosquitto.org** no https:// oder mqtt:// only addresse
* The MQTT password (optional) and WIFI password may be a maximum of 29 characters long.


## MQTT and HomeAssistant

### Autodiscover

**comming soon!**

MBusinoP1 supports Home Assistant autodiscover. You need only the MQTT integration and MBusino will be find as device with all records.
The third record message is a autodiscover message and every 256th recurring.

### per hand

With the Software **MQTT Explorer** at PC or **MyMQTT** at your mobile you will see the mqtt messages.
in attachement you will find a HA Config file as example. But you have to change something to your MQTT messages.

Every value has its own sensor. **state_topic** is the MQTT topic, depends on the topics at MQTT Explorer.
```
mqtt:
 sensor:
     - unique_id: MBusino_Leistung
      name: "Leistung"
      device_class: power
      state_topic: "MBusino/MBus/4_power"     
      unit_of_measurement: "W"
```

## MBusino update

With the Flash Webtool, you will loose al your datas during update. Use instead  **[IP_of_MBusino]/update** and all datas will retain.

![Update](https://github.com/Zeppelin500/MBusinoP1/blob/main/pictures/update.png)


