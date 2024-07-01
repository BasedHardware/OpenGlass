#define CAMERA_MODEL_XIAO_ESP32S3
#include <I2S.h>
// #include "FS.h"
// #include "SD.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "esp_camera.h"
#include "camera_pins.h"
#include "mulaw.h"

// Audio

// Uncomment to build with support for Opus codec
// #define OPUS_CODEC

#ifdef OPUS_CODEC

#include "OpusEncoder.h"

#define CHANNELS 1
#define FRAME_SIZE 160 // 20ms at 8kHz
#define MAX_PACKET_SIZE 1000

#define SAMPLE_RATE 16000
#define SAMPLE_BITS 16

#else

#define SAMPLE_RATE 8000
#define SAMPLE_BITS 16

#endif

//
// BLE
//

// Device Information Service
#define DEVICE_INFORMATION_SERVICE_UUID (uint16_t)0x180A
#define MANUFACTURER_NAME_STRING_CHAR_UUID (uint16_t)0x2A29
#define MODEL_NUMBER_STRING_CHAR_UUID (uint16_t)0x2A24
#define FIRMWARE_REVISION_STRING_CHAR_UUID (uint16_t)0x2A26
#define HARDWARE_REVISION_STRING_CHAR_UUID (uint16_t)0x2A27

// Battery Level Service
#define BATTERY_SERVICE_UUID (uint16_t)0x180F
#define BATTERY_LEVEL_CHAR_UUID (uint16_t)0x2A19

// Main Friend Service
static BLEUUID serviceUUID("19B10000-E8F2-537E-4F6C-D104768A1214");
static BLEUUID audioCharUUID("19B10001-E8F2-537E-4F6C-D104768A1214");
static BLEUUID audioCodecUUID("19B10002-E8F2-537E-4F6C-D104768A1214");
static BLEUUID photoCharUUID("19B10005-E8F2-537E-4F6C-D104768A1214");

BLECharacteristic *audio;
BLECharacteristic *photo;
bool connected = false;

BLECharacteristic *pBatteryLevelCharacteristic;
uint8_t batteryLevel = 100;
unsigned long lastBatteryUpdate = 0;

class ServerHandler: public BLEServerCallbacks
{
  void onConnect(BLEServer *server)
  {
    connected = true;
    Serial.println("Connected");
  }

  void onDisconnect(BLEServer *server)
  {
    connected = false;
    Serial.println("Disconnected");
    BLEDevice::startAdvertising();
  }
};

class MessageHandler: public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param)
  {
    // Currently unused
  }
};

void configure_ble() {
  BLEDevice::init("OpenGlass");
  BLEServer *server = BLEDevice::createServer();
  BLEService *service = server->createService(serviceUUID);

  // Audio service
  audio = service->createCharacteristic(
    audioCharUUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  BLE2902 *ccc = new BLE2902();
  ccc->setNotifications(true);
  audio->addDescriptor(ccc);

  // Photo service
  photo = service->createCharacteristic(
    photoCharUUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  ccc = new BLE2902();
  ccc->setNotifications(true);
  photo->addDescriptor(ccc);

  // Codec service
  BLECharacteristic *codec = service->createCharacteristic(
    audioCodecUUID,
    BLECharacteristic::PROPERTY_READ
  );
#ifdef OPUS_CODEC
  uint8_t codecId = 20; // Opus 16khz
#else
  uint8_t codecId = 11; // MuLaw 8khz
#endif
  codec->setValue(&codecId, 1);

  // Device Information Service
  BLEService *deviceInfoService = server->createService(DEVICE_INFORMATION_SERVICE_UUID);
  BLECharacteristic *pManufacturerNameCharacteristic = deviceInfoService->createCharacteristic(
      MANUFACTURER_NAME_STRING_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pModelNumberCharacteristic = deviceInfoService->createCharacteristic(
      MODEL_NUMBER_STRING_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pFirmwareRevisionCharacteristic = deviceInfoService->createCharacteristic(
      FIRMWARE_REVISION_STRING_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pHardwareRevisionCharacteristic = deviceInfoService->createCharacteristic(
      HARDWARE_REVISION_STRING_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ);

  pManufacturerNameCharacteristic->setValue("Based Hardware");
  pModelNumberCharacteristic->setValue("OpenGlass");
  pFirmwareRevisionCharacteristic->setValue("1.0.1");
  pHardwareRevisionCharacteristic->setValue("Seeed Xiao ESP32S3 Sense");

  // Battery Service
  BLEService *batteryService = server->createService(BATTERY_SERVICE_UUID);
  pBatteryLevelCharacteristic = batteryService->createCharacteristic(
      BATTERY_LEVEL_CHAR_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pBatteryLevelCharacteristic->addDescriptor(new BLE2902());

  // Service
  service->start();
  deviceInfoService->start();
  batteryService->start();

  server->setCallbacks(new ServerHandler());

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BATTERY_SERVICE_UUID);
  advertising->addServiceUUID(DEVICE_INFORMATION_SERVICE_UUID);
  advertising->addServiceUUID(service->getUUID());
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

// Save pictures to SD card
// void photo_share(const char * fileName) {
//   // Take a photo
//   camera_fb_t *fb = esp_camera_fb_get();
//   if (!fb) {
//     Serial.println("Failed to get camera frame buffer");
//     return;
//   }
//   // Save photo to file
//   writeFile(SD, fileName, fb->buf, fb->len);

//   // Release image buffer
//   esp_camera_fb_return(fb);

//   Serial.println("Photo saved to file");
// }

camera_fb_t *fb;
// int images_written = 0;

// void writeFile(fs::FS &fs, const char * path, uint8_t * data, size_t len){
//     Serial.printf("Writing file: %s\n", path);

//     File file = fs.open(path, FILE_WRITE);
//     if(!file){
//         Serial.println("Failed to open file for writing");
//         return;
//     }
//     if(file.write(data, len) == len){
//         Serial.println("File written");
//     } else {
//         Serial.println("Write failed");
//     }
//     file.close();
// }

bool take_photo() {

  // Release buffer
  if (fb) {
    Serial.println("Release FB");
    esp_camera_fb_return(fb);
  }

  // Take a photo
  Serial.println("Taking photo...");
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Failed to get camera frame buffer");
    return false;
  }

  // Write to SD
  // char filename[32];
  // sprintf(filename, "/image_%d.jpg", images_written);
  // writeFile(SD, filename, fb->buf, fb->len);
  // images_written++;

  return true;
}

//
// Microphone
//

#define VOLUME_GAIN 2

#ifdef OPUS_CODEC
static size_t recording_buffer_size = FRAME_SIZE * 2; // 16-bit samples
static size_t compressed_buffer_size = MAX_PACKET_SIZE;
#else
static size_t recording_buffer_size = 400;
static size_t compressed_buffer_size = 400 + 3; /* header */
#endif
static uint8_t *s_recording_buffer = nullptr;
static uint8_t *s_compressed_frame = nullptr;
static uint8_t *s_compressed_frame_2 = nullptr;

void configure_microphone() {

  // start I2S at 16 kHz with 16-bits per sample
  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    while (1); // do nothing
  }

  // Allocate buffers
  s_recording_buffer = (uint8_t *) ps_calloc(recording_buffer_size, sizeof(uint8_t));
  s_compressed_frame = (uint8_t *) ps_calloc(compressed_buffer_size, sizeof(uint8_t));
  s_compressed_frame_2 = (uint8_t *) ps_calloc(compressed_buffer_size, sizeof(uint8_t));
}

size_t read_microphone() {
  size_t bytes_recorded = 0;
  esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, s_recording_buffer, recording_buffer_size, &bytes_recorded, portMAX_DELAY);
  return bytes_recorded;
}

//
// Camera
//

void configure_camera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.fb_count = 1;

  // High quality (psram)
  // config.jpeg_quality = 10;
  // config.fb_count = 2;
  // config.grab_mode = CAMERA_GRAB_LATEST;

  // Low quality (and in local ram)
  config.jpeg_quality = 10;
  config.frame_size = FRAMESIZE_SVGA;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  // config.fb_location = CAMERA_FB_IN_DRAM;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void updateBatteryLevel()
{
  // TODO:
  pBatteryLevelCharacteristic->setValue(&batteryLevel, 1);
  pBatteryLevelCharacteristic->notify();
}

//
// Main
//

// static uint8_t *s_compressed_frame_2 = nullptr;
// static size_t compressed_buffer_size = 400 + 3;
void setup() {
  Serial.begin(921600);
  // SD.begin(21);
  Serial.println("Setup");
  Serial.println("Starting BLE...");
  configure_ble();
  // s_compressed_frame_2 = (uint8_t *) ps_calloc(compressed_buffer_size, sizeof(uint8_t));
#ifdef OPUS_CODEC
  if (!encoder.begin(SAMPLE_RATE, CHANNELS))
  {
    Serial.println("Failed to initialize Opus encoder!");
    while (1)
      ; // do nothing
  }
#endif
  Serial.println("Starting Microphone...");
  configure_microphone();
  Serial.println("Starting Camera...");
  configure_camera();
  Serial.println("OK");
}

uint16_t frame_count = 0;
unsigned long lastCaptureTime = 0;
size_t sent_photo_bytes = 0;
size_t sent_photo_frames = 0;
bool need_send_photo = false;

void loop() {

  // Read from mic
  size_t bytes_recorded = read_microphone();

  // Push to BLE
  if (bytes_recorded > 0 && connected)
  {
#ifdef OPUS_CODEC
    // Convert to 16-bit samples
    int16_t samples[FRAME_SIZE];
    for (size_t i = 0; i < bytes_recorded; i += 2)
    {
      samples[i / 2] = ((s_recording_buffer[i + 1] << 8) | s_recording_buffer[i]) << VOLUME_GAIN;
    }

    // Encode with Opus
    int encoded_bytes = encoder.encode(samples, FRAME_SIZE, &s_compressed_frame[3], MAX_PACKET_SIZE - 3);

    if (encoded_bytes > 0)
    {
      size_t out_buffer_size = encoded_bytes / 2 + 3;
#else
    for (size_t i = 0; i < bytes_recorded; i += 2) {
      int16_t sample = ((s_recording_buffer[i + 1] << 8) | s_recording_buffer[i]) << VOLUME_GAIN;
      s_compressed_frame[i / 2 + 3] = linear2ulaw(sample);
    }

    int encoded_bytes = bytes_recorded / 2;
#endif

    s_compressed_frame[0] = frame_count & 0xFF;
    s_compressed_frame[1] = (frame_count >> 8) & 0xFF;
    s_compressed_frame[2] = 0;

    size_t out_buffer_size = encoded_bytes + 3;
    audio->setValue(s_compressed_frame, out_buffer_size);
    audio->notify();
    frame_count++;
#ifdef OPUS_CODEC
    }
#endif
  }

  // Take a photo
  unsigned long now = millis();
  if ((now - lastCaptureTime) >= 5000 && !need_send_photo && connected) {
    if (take_photo()) {
      need_send_photo = true;
      sent_photo_bytes = 0;
      sent_photo_frames = 0;
      lastCaptureTime = now;
    }
  }

  // Push to BLE
  if (need_send_photo) {
    size_t remaining = fb->len - sent_photo_bytes;
    if (remaining > 0) {
      // Populate buffer
      s_compressed_frame_2[0] = sent_photo_frames & 0xFF;
      s_compressed_frame_2[1] = (sent_photo_frames >> 8) & 0xFF;
      size_t bytes_to_copy = remaining;
      if (bytes_to_copy > 200) {
        bytes_to_copy = 200;
      }
      memcpy(&s_compressed_frame_2[2], &fb->buf[sent_photo_bytes], bytes_to_copy);

      // Push to BLE
      photo->setValue(s_compressed_frame_2, bytes_to_copy + 2);
      photo->notify();
      sent_photo_bytes += bytes_to_copy;
      sent_photo_frames++;
    } else {

      // End flag
      s_compressed_frame_2[0] = 0xFF;
      s_compressed_frame_2[1] = 0xFF;
      photo->setValue(s_compressed_frame_2, 2);
      photo->notify();

      Serial.println("Photo sent");
      need_send_photo = false;
    }
  }

  if (millis() - lastBatteryUpdate > 60000)
  {
    updateBatteryLevel();
    lastBatteryUpdate = millis();
  }

  // Delay
  delay(4);
}