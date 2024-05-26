# Using arduino-cli

### Install the board

```bash
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core install esp32:esp32
```

### Get board details

On Windows 11 board should be showing as ```esp32:esp32:XIAO_ESP32S3```
but instead shows as ```esp32:esp32:nora_w10```

If the board is showing as ```esp32:esp32:XIAO_ESP32S3``` modify the commands below

```bash
arduino-cli board list
arduino-cli board details -b esp32:esp32:nora_w10
```

### Compile and upload

Change COM6 to the port name from the board list output

```bash
arduino-cli compile -u -p COM6 -b esp32:esp32:nora_w10:PSRAM=opi
```