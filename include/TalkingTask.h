
#include <Arduino.h>
#include "base64.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "cJSON.h"
#include "ArduinoJson.h"
#include "lvgl.h"
#include "ui.h"
// #include "soc/rtc_wdt.h" 
#define RecordKey 0       //端口0
#define ADC 12      //端口40

String fenge(String str,String fen,int index);
void gain_token(void);  //获取token
void IRAM_ATTR onTimer();
void Talking_Task(void *pvParameter);
void GetWeather();