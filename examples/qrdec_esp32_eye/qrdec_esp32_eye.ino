/*
* qrdec_esp32_eye.ino
*/
#include <Arduino.h>
#include <ESP32QRCodeReader.h>


//ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
// changed by nishi
ESP32QRCodeReader reader(CAMERA_MODEL_ESP_EYE);
//ESP32QRCodeReader reader(CAMERA_MODEL_ESP_EYE,FRAMESIZE_VGA); // 640x480  -> NG
//ESP32QRCodeReader reader(CAMERA_MODEL_ESP_EYE,FRAMESIZE_HVGA);  // 480x320
void onQrCodeTask(void *pvParameters)
{
  struct QRCodeData qrCodeData;

  while (true)
  {
    if (reader.receiveQrCode(&qrCodeData, 100))
    {
      Serial.println("Found QRCode");
      if (qrCodeData.valid)
      {
        Serial.print("Payload: ");
        Serial.println((const char *)qrCodeData.payload);
      }
      else
      {
        Serial.print("Invalid: ");
        Serial.println((const char *)qrCodeData.payload);
      }
    }
    //else{
    //    Serial.println("non");
    //}
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  reader.setup();   // camera setup

  Serial.println("Setup QRCode Reader");

  reader.beginOnCore(1);

  Serial.println("Begin on Core 1");

  xTaskCreate(onQrCodeTask, "onQrCode", 4 * 1024, NULL, 4, NULL);

}

void loop() {
  // put your main code here, to run repeatedly:
  delay(100);

}

