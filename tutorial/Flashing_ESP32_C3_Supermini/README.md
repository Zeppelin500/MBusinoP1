# Flashing ESP32 C3 supermini for MBusino Nano

For the ESP32 C3 are 3 different .bin files created.

**MBusinoNano**

* MBusinoP1.ino.bootloader.bin
* MBusinoP1.ino.partitions.bin
* MBusinoP1.ino.bin 

https://github.com/Zeppelin500/MBusinoP1/tree/main/src/MBusinoP1/build/esp32.esp32.makergo_c3_supermini


### Use for Flashing https://adafruit.github.io/Adafruit_WebSerial_ESPTool/

* connect the C3 Supermini with USB
* press and hold the boot button, press the reset button, release the boot botton. 
* press **Connect** and choose the USP Port
* if connected click **Erase** and wait for finish
* may you have to connect again
* if connected, load all 3 .bin files as follows with the offset

**MBusino**

* Offset 0x 0000    MBusinoP1.ino.bootloader.bin
* Offset 0x 8000    MBusinoP1.ino.partitions.bin
* Offset 0x 10000    MBusinoP1.ino.bin 

--> done

**The offset of the bootloader in the screenshots are wrong. Use The values above.** 

<img src="https://github.com/Zeppelin500/MBusino/blob/main/pictures/AdafruitESPtool.png" width="400">



