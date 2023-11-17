#include <Arduino.h>
#include "Audio.h"
#include "WiFi.h"

// configs
#define I2S_BCK GPIO_NUM_27
#define I2S_WS GPIO_NUM_14
#define I2S_DATA GPIO_NUM_26

Audio audio;

// put function declarations here:
// void setUpAudioOutput();

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  Serial.println("connecting to wifi 1+");
  WiFi.begin("1+", "m5xgi83h");

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println("Wifi Connected");
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DATA);
  audio.setVolume(100);
  audio.connecttospeech("Hello my name is Diljit ", "en");
}

void loop()
{
  // put your main code here, to run repeatedly:
  audio.loop();
}

// put function definitions here:
// void setUpAudioOutput(){

// }
