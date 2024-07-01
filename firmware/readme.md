# Using arduino-cli

### Install the board

```bash
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core install esp32:esp32@2.0.17
```

### Get board details

On Windows 11 board should be showing as ```esp32:esp32:XIAO_ESP32S3```
but instead might show show as ```esp32:esp32:nora_w10``` or ```esp32:esp32:wifiduino32c3```

```bash
arduino-cli board list
arduino-cli board details -b esp32:esp32:XIAO_ESP32S3
```

### Compile and upload

Change COM5 to the port name from the board list output

```bash
arduino-cli compile --build-path build --output-dir dist -e -u -p COM5 -b esp32:esp32:XIAO_ESP32S3:PSRAM=opi
```