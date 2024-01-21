# ESP-EYE QR Code Scanner with qrdec.

This library provides a interface to read QR Codes using an ESP-EYE with a camera and qrdec.  
I would like to recomend qrdec a little bit rather than quirc.  
See [qrdec](https://github.com/torque/qrdec).  

Original 
[alvarowolfx/ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader)  
Internally this lib uses a slight modified version of the [alvarowolfx/ESP32QRCodeReader](https://github.com/alvarowolfx/ESP32QRCodeReader) , [Quirc library](https://github.com/dlbeer/quirc) , [qrdec](https://github.com/torque/qrdec)  and  [iconv.c](https://github.com/arduino/linino/blob/master/trunk/package/libs/libiconv/src/iconv.c).

## Usage

I recommend to use PlatformIO to install this lib. Add `qrdec_esp32_eye.ino` and `platformio.ini` file to PlatformIO workspace.

On PIO Home , New Project  
Name: qrdec_esp32_eye  
Board: Espressif ESP-WORKER-KIT  
Framework: Arduino  

$ cd qrdec_esp32_eye/lib    
$ git clone https://github.com/tosa-no-onchan/ESP32QRCodeReader.git    
copy qrdec_esp32_eye/lib/ESP32QRCodeReader/examples/qrdec_esp32_eye/qrdec_esp32_eye.ino to qrdec_esp32_eye/src/  
$ rm main.cpp  
copy qrdec_esp32_eye/lib/ESP32QRCodeReader/examples/platformio.ini to qrdec_esp32_eye/  
build  
upload  
  
platformio.ini:

```
[env:esp-wrover-kit]
platform = espressif32
;platform = espressif32@5.2.0
board = esp-wrover-kit
framework = arduino

monitor_speed = 115200

board_build.partitions = huge_app.csv

board_build.flash_mode = qio
; set frequency to 80MHz
board_build.f_flash = 80000000L

; https://github.com/dlbeer/quirc
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DQUIRC_FLOAT_TYPE=float

;    -DQUIRC_MAX_REGIONS=65534


; https://github.com/Links2004/arduinoWebSockets/issues/635
lib_deps = 
	WiFi
    WiFiClientSecure

upload_port = /dev/ttyUSB0
```


## License

This code is released under the MIT License.

### References
- https://github.com/alvarowolfx/ESP32QRCodeReader
- https://github.com/torque/qrdec
- https://github.com/arduino/linino/blob/master/trunk/package/libs/libiconv/src/iconv.c
- https://github.com/dlbeer/quirc
- https://github.com/sipeed/MaixPy
- https://github.com/Schaggo/QR-ARDUINO
- https://github.com/donny681/ESP32_CAMERA_QR
