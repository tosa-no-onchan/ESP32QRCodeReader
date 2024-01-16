#include "ESP32QRCodeReader.h"

#include "Arduino.h"

//#define USE_QUIRC

#if !defined(USE_QUIRC)
  #include "quirc/quirc_internal.h"
  # if defined(__cplusplus)
  extern "C" {
  # endif
      #include "qrdec/src/binarize.h"
      //#include "image.h"
  # if defined(__cplusplus)
  }
  # endif
  #include "qrdec/src/qrcode.h"
#else
  #include "quirc/quirc.h"
#endif

ESP32QRCodeReader::ESP32QRCodeReader() : ESP32QRCodeReader(CAMERA_MODEL_AI_THINKER, FRAMESIZE_QVGA)
{
}

ESP32QRCodeReader::ESP32QRCodeReader(framesize_t frameSize) : ESP32QRCodeReader(CAMERA_MODEL_AI_THINKER, frameSize)
{
}

ESP32QRCodeReader::ESP32QRCodeReader(CameraPins pins) : ESP32QRCodeReader(pins, FRAMESIZE_QVGA)
{
}

ESP32QRCodeReader::ESP32QRCodeReader(CameraPins pins, framesize_t frameSize) : pins(pins), frameSize(frameSize)
{
  qrCodeQueue = xQueueCreate(10, sizeof(struct QRCodeData));
}

ESP32QRCodeReader::~ESP32QRCodeReader()
{
  end();
}

QRCodeReaderSetupErr ESP32QRCodeReader::setup()
{
  if (!psramFound())
  {
    return SETUP_NO_PSRAM_ERROR;
  }

  cameraConfig.ledc_channel = LEDC_CHANNEL_0;
  cameraConfig.ledc_timer = LEDC_TIMER_0;
  cameraConfig.pin_d0 = pins.Y2_GPIO_NUM;
  cameraConfig.pin_d1 = pins.Y3_GPIO_NUM;
  cameraConfig.pin_d2 = pins.Y4_GPIO_NUM;
  cameraConfig.pin_d3 = pins.Y5_GPIO_NUM;
  cameraConfig.pin_d4 = pins.Y6_GPIO_NUM;
  cameraConfig.pin_d5 = pins.Y7_GPIO_NUM;
  cameraConfig.pin_d6 = pins.Y8_GPIO_NUM;
  cameraConfig.pin_d7 = pins.Y9_GPIO_NUM;
  cameraConfig.pin_xclk = pins.XCLK_GPIO_NUM;
  cameraConfig.pin_pclk = pins.PCLK_GPIO_NUM;
  cameraConfig.pin_vsync = pins.VSYNC_GPIO_NUM;
  cameraConfig.pin_href = pins.HREF_GPIO_NUM;
  //cameraConfig.pin_sscb_sda = pins.SIOD_GPIO_NUM;
  // changed by nishi 2024.1.12
  cameraConfig.pin_sccb_sda = pins.SIOD_GPIO_NUM;
  //cameraConfig.pin_sscb_scl = pins.SIOC_GPIO_NUM;
  // changed by nishi 2024.1.12
  cameraConfig.pin_sccb_scl = pins.SIOC_GPIO_NUM;
  cameraConfig.pin_pwdn = pins.PWDN_GPIO_NUM;
  cameraConfig.pin_reset = pins.RESET_GPIO_NUM;
  cameraConfig.xclk_freq_hz = 10000000;   // original 2024.1.12
	//cameraConfig.xclk_freq_hz = 20000000;   // test by nishi 2023.11.1
  //cameraConfig.xclk_freq_hz = 15000000;
  cameraConfig.pixel_format = PIXFORMAT_GRAYSCALE;

  //cameraConfig.frame_size = FRAMESIZE_VGA;
  cameraConfig.frame_size = frameSize;  // FRAMESIZE_QVGA 320x240
  //cameraConfig.jpeg_quality = 13;
  cameraConfig.jpeg_quality = 15;   // original 2024.1.12
  //cameraConfig.jpeg_quality = 18;
  cameraConfig.fb_count = 1;      // original 2024.1.12
  //cameraConfig.fb_count = 2;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&cameraConfig);
  if (err != ESP_OK)
  {
    return SETUP_CAMERA_INIT_ERROR;
  }
  return SETUP_OK;
}

void dumpData(const struct quirc_data *data)
{
  Serial.printf("Version: %d\n", data->version);
  Serial.printf("ECC level: %c\n", "MLHQ"[data->ecc_level]);
  Serial.printf("Mask: %d\n", data->mask);
  Serial.printf("Length: %d\n", data->payload_len);
  Serial.printf("Payload: %s\n", data->payload);
}

#if defined(USE_QUIRC)
//---------------
// for quirc
//---------------
void qrCodeDetectTask(void *taskData)
{
  ESP32QRCodeReader *self = (ESP32QRCodeReader *)taskData;
  camera_config_t camera_config = self->cameraConfig;
  if (camera_config.frame_size > FRAMESIZE_SVGA)
  {
    if (self->debug)
    {
      Serial.println("Camera Size err");
    }
    vTaskDelete(NULL);
    return;
  }

  struct quirc *q = NULL;
  uint8_t *image = NULL;
  camera_fb_t *fb = NULL;

  uint16_t old_width = 0;
  uint16_t old_height = 0;

  if (self->debug)
  {
    Serial.printf("begin to qr_recoginze\r\n");
  }
  q = quirc_new();
  if (q == NULL)
  {
    if (self->debug)
    {
      Serial.print("can't create quirc object\r\n");
    }
    vTaskDelete(NULL);
    return;
  }

  // add by nishi
  //sensor_t * s = esp_camera_sensor_get();
  //s->set_hmirror(s, 1); // 左右反転
  //s->set_vflip(s, 1);//flip it back
  //s->set_brightness(s, 1); //up the blightness just a bit
  //s->set_saturation(s, -2); //lower the saturation


  while (true)
  {

    if (self->debug)
    {
      Serial.printf("alloc qr heap: %u\r\n", xPortGetFreeHeapSize());
      Serial.printf("uxHighWaterMark = %d\r\n", uxTaskGetStackHighWaterMark(NULL));
      Serial.print("begin camera get fb\r\n");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);

    fb = esp_camera_fb_get();
    if (!fb)
    {
      if (self->debug)
      {
        Serial.println("Camera capture failed");
      }
      continue;
    }

    if (old_width != fb->width || old_height != fb->height)
    {
      if (self->debug)
      {
        Serial.printf("Recognizer size change w h len: %d, %d, %d \r\n", fb->width, fb->height, fb->len);
        Serial.println("Resize the QR-code recognizer.");
        // Resize the QR-code recognizer.
      }
      if (quirc_resize(q, fb->width, fb->height) < 0)
      {
        if (self->debug)
        {
          Serial.println("Resize the QR-code recognizer err (cannot allocate memory).");
        }
        esp_camera_fb_return(fb);
        fb = NULL;
        image = NULL;
        continue;
      }
      else
      {
        old_width = fb->width;
        old_height = fb->height;
      }
    }

    // Serial.printf("quirc_begin\r\n");
    image = quirc_begin(q, NULL, NULL);
    if (self->debug)
    {
      Serial.printf("Frame w h len: %d, %d, %d \r\n", fb->width, fb->height, fb->len);
    }
    memcpy(image, fb->buf, fb->len);
    quirc_end(q);

    if (self->debug)
    {
      Serial.printf("quirc_end\r\n");
    }
    int count = quirc_count(q);
    if (count == 0)
    {
      if (self->debug)
      {
        Serial.printf("Error: not a valid qrcode\n");
      }
      esp_camera_fb_return(fb);
      fb = NULL;
      image = NULL;
      continue;
    }

    for (int i = 0; i < count; i++)
    {
      struct quirc_code code;
      struct quirc_data data;
      quirc_decode_error_t err;

      quirc_extract(q, i, &code);
      err = quirc_decode(&code, &data);

      struct QRCodeData qrCodeData;

      if (err)
      {
        const char *error = quirc_strerror(err);
        int len = strlen(error);
        if (self->debug)
        {
          Serial.printf("Decoding FAILED: %s\n", error);
        }
        for (int i = 0; i < len; i++)
        {
          qrCodeData.payload[i] = error[i];
        }
        qrCodeData.valid = false;
        qrCodeData.payload[len] = '\0';
        qrCodeData.payloadLen = len;
      }
      else
      {
        if (self->debug)
        {
          Serial.printf("Decoding successful:\n");
          dumpData(&data);
        }

        qrCodeData.dataType = data.data_type;
        for (int i = 0; i < data.payload_len; i++)
        {
          qrCodeData.payload[i] = data.payload[i];
        }
        qrCodeData.valid = true;
        qrCodeData.payload[data.payload_len] = '\0';
        qrCodeData.payloadLen = data.payload_len;
      }
      xQueueSend(self->qrCodeQueue, &qrCodeData, (TickType_t)0);

      if (self->debug)
      {
        Serial.println();
      }
    }

    //Serial.printf("finish recoginize\r\n");
    esp_camera_fb_return(fb);
    fb = NULL;
    image = NULL;
  }
  quirc_destroy(q);
  vTaskDelete(NULL);
}
#else
//---------------
// for qrdec
//---------------
static int qr_code_data_cmp(const void *_a, const void *_b) {
    const qr_code_data *a = (const qr_code_data *)_a;
    const qr_code_data *b = (const qr_code_data *)_b;
    int ai = 0;
    int bi = 0;

    /*Find the top-left corner of each bounding box.*/
    for (int i = 1; i < 4; i++) {
        if (a->bbox[i][1] < a->bbox[ai][1]
         || (a->bbox[i][1] == a->bbox[ai][1] && a->bbox[i][0] < a->bbox[ai][0])) {
            ai = i;
        }
        if (b->bbox[i][1] < b->bbox[bi][1]
         || (b->bbox[i][1] == b->bbox[bi][1] && b->bbox[i][0] < b->bbox[bi][0])) {
            bi = i;
        }
    }
    /*Sort the codes in top-down, left-right order.*/
    return (
        (((a->bbox[ai][1] > b->bbox[bi][1]) - (a->bbox[ai][1] < b->bbox[bi][1])) << 1)
      + (a->bbox[ai][0] > b->bbox[bi][0])
      - (a->bbox[ai][0] < b->bbox[bi][0])
    );
}

void qrCodeDetectTask(void *taskData)
{
  ESP32QRCodeReader *self = (ESP32QRCodeReader *)taskData;
  camera_config_t camera_config = self->cameraConfig;
  if (camera_config.frame_size > FRAMESIZE_SVGA)
  {
    if (self->debug)
    {
      Serial.println("Camera Size err");
    }
    vTaskDelete(NULL);
    return;
  }

  uint8_t *image = NULL;
  camera_fb_t *fb = NULL;

  uint16_t old_width = 0;
  uint16_t old_height = 0;

  qr_reader *reader;
  qr_code_data_list qrlist;
  char **text;

  struct QRCodeData qrCodeData;

  if (self->debug)
  {
    Serial.printf("begin to qr_recoginze\r\n");
  }

  // add by nishi
  //sensor_t * s = esp_camera_sensor_get();
  //s->set_hmirror(s, 1); // 左右反転
  //s->set_vflip(s, 1);//flip it back
  //s->set_brightness(s, 1); //up the blightness just a bit
  //s->set_brightness(s, -1); //down the blightness just a bit
  //s->set_saturation(s, -2); //lower the saturation

  while (true)
  {

    if (self->debug)
    {
      Serial.printf("alloc qr heap: %u\r\n", xPortGetFreeHeapSize());
      Serial.printf("uxHighWaterMark = %d\r\n", uxTaskGetStackHighWaterMark(NULL));
      Serial.print("begin camera get fb\r\n");
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);

    fb = esp_camera_fb_get();
    if (!fb)
    {
      if (self->debug)
      {
        Serial.println("Camera capture failed");
      }
      continue;
    }
    // fb->format == PIXFORMAT_GRAYSCALE  -> ::setup() で設定済。


    if (old_width != fb->width || old_height != fb->height)
    {
      if (self->debug)
      {
        Serial.printf("Recognizer size change w h len: %d, %d, %d \r\n", fb->width, fb->height, fb->len);
        Serial.println("Resize the QR-code recognizer.");
        // Resize the QR-code recognizer.
      }
      old_width = fb->width;
      old_height = fb->height;
    }

    if (self->debug)
    {
      Serial.printf("Frame w h len: %d, %d, %d \r\n", fb->width, fb->height, fb->len);
    }

    image=(uint8_t *)malloc(fb->width * fb->height);

    memcpy(image, fb->buf, fb->len);

    // 白黒反転か?
    qr_binarize(image, fb->width, fb->height, QR_BINARIZE_INVERT);

    reader = qr_reader_alloc();
    qr_code_data_list_init(&qrlist);
    if (qr_reader_locate(reader, &qrlist, image, fb->width, fb->height) > 0) {

        /*Sort the codes to make test results reproducible.*/
        qsort(
            qrlist.qrdata,
            qrlist.nqrdata,
            sizeof(*qrlist.qrdata),
            qr_code_data_cmp
        );
        int ntext = qr_code_data_list_extract_text(&qrlist, &text, 1);

        #define TEST_MM1
        #if defined(TEST_MM1)
        Serial.printf("ntext: %d\r\n", ntext);
        for (int qridx = 0; qridx < qrlist.nqrdata; qridx++) {
            struct qr_code_data *const qr_code = qrlist.qrdata + qridx;
            Serial.printf("QR Code %d:\n", qridx);
            Serial.printf("  Version: %d\n", qr_code->version);
            Serial.printf("  ECC level: %c\n", "LMQH"[qr_code->ecc_level]);
            Serial.printf("  Mask: %d\n", qr_code->mask);
            Serial.printf(
                "  Bounds: (%d, %d) (%d, %d) (%d, %d) (%d, %d)\n",
                qr_code->bbox[0][0], qr_code->bbox[0][1],
                qr_code->bbox[1][0], qr_code->bbox[1][1],
                qr_code->bbox[3][0], qr_code->bbox[3][1],
                qr_code->bbox[2][0], qr_code->bbox[2][1]
            );
            Serial.printf("  Center: (%d, %d)\n", qr_code->center[0], qr_code->center[1]);

            for (int jdx = 0; jdx < (qr_code->nentries); jdx++) {
                struct qr_code_data_entry *const entry = qr_code->entries + jdx;
                Serial.printf("  Data entry %d:\n", jdx);
                Serial.printf("    type: %d\n", entry->mode);
                if (QR_MODE_HAS_DATA(entry->mode)) {
                    Serial.printf("    length: %d\n", entry->payload.data.len);
                    Serial.printf("    data: ");
                    int bufdx = 0;
                    for (; bufdx < (entry->payload.data.len - 1); bufdx++) {
                        Serial.printf("%02X ", entry->payload.data.buf[bufdx]);
                    }
                    Serial.printf("%02X\n", entry->payload.data.buf[bufdx]);
                }
            }
        }
        #endif

        for (int i = 0; i < ntext; i++) {
          //#define TEST_MM2
          #if defined(TEST_MM2)
          Serial.printf("strlen(text[i]): %d\n", strlen(text[i]));
          for(int j=0;j < 10;j++){
              Serial.printf("%02X ", text[i][j]);
              if(text[i][j]==0x00)
                break;
          }
          Serial.printf("\n");
          #endif

          Serial.printf("Text: %s\n", text[i]);
          if(strlen(text[i]) > 0){
            qrCodeData.valid = true;
            //qrCodeData.payload[data.payload_len] = '\0';
            //strcpy((char *)qrCodeData.payload,text[i]);
            memcpy(qrCodeData.payload, text[i], strlen(text[i])+1);
            qrCodeData.payloadLen = strlen(text[i]);
          }
          else{
            qrCodeData.valid = false;
            qrCodeData.payload[0] = '\0';
            qrCodeData.payloadLen = 0;
          }
          xQueueSend(self->qrCodeQueue, &qrCodeData, (TickType_t)0);
        }

        qr_text_list_free(text, ntext);
        qr_code_data_list_clear(&qrlist);
    }
    qr_reader_free(reader);

    //Serial.printf("finish recoginize\r\n");
    esp_camera_fb_return(fb);
    fb = NULL;
    free(image);
    image = NULL;
  }

  //quirc_destroy(q);
  vTaskDelete(NULL);
}
#endif


void ESP32QRCodeReader::begin()
{
  beginOnCore(0);
}

void ESP32QRCodeReader::beginOnCore(BaseType_t core)
{
  if (!begun)
  {
    xTaskCreatePinnedToCore(qrCodeDetectTask, "qrCodeDetectTask", QR_CODE_READER_STACK_SIZE, this, QR_CODE_READER_TASK_PRIORITY, &qrCodeTaskHandler, core);
    begun = true;
  }
}

bool ESP32QRCodeReader::receiveQrCode(struct QRCodeData *qrCodeData, long timeoutMs)
{
  return xQueueReceive(qrCodeQueue, qrCodeData, (TickType_t)pdMS_TO_TICKS(timeoutMs)) != 0;
}

void ESP32QRCodeReader::end()
{
  if (begun)
  {
    TaskHandle_t tmpTask = qrCodeTaskHandler;
    if (qrCodeTaskHandler != NULL)
    {
      qrCodeTaskHandler = NULL;
      vTaskDelete(tmpTask);
    }
  }
  begun = false;
}

void ESP32QRCodeReader::setDebug(bool on)
{
  debug = on;
}
