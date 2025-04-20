/*******************************************************************************
 * LVGL Widgets
 * This is a widgets demo for LVGL - Light and Versatile Graphics Library
 * import from: https://github.com/lvgl/lv_demos.git
 *
 * This was created from the project here 
 * https://www.makerfabs.com/sunton-esp32-s3-4-3-inch-ips-with-touch.html
 * 
 * Dependent libraries:
 * LVGL: https://github.com/lvgl/lvgl.git

 * Touch libraries:
 * FT6X36: https://github.com/strange-v/FT6X36.git
 * GT911: https://github.com/TAMCTec/gt911-arduino.git
 * XPT2046: https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
 *
 * LVGL Configuration file:
 * Copy your_arduino_path/libraries/lvgl/lv_conf_template.h
 * to your_arduino_path/libraries/lv_conf.h
 * Then find and set:
 * #define LV_COLOR_DEPTH     16
 * #define LV_TICK_CUSTOM     1
 *
 * For SPI display set color swap can be faster, parallel screen don't set!
 * #define LV_COLOR_16_SWAP   1
 *
 * Optional: Show CPU usage and FPS count
 * #define LV_USE_PERF_MONITOR 1
 ******************************************************************************/
//#include "lv_demo_widgets.h"
#include <lvgl.h>
#include <demos/lv_demos.h>
#include "TalkingTask.h"
#include "espnowTask.h"
extern struct HomeAssistant myData;

/*******************************************************************************
 ******************************************************************************/
#include <Arduino_GFX_Library.h>
extern const uint8_t BROADCAST_MAC[];
// Pin definitions
#define TFT_BL 6
#define GFX_BL DF_GFX_BL

// Display configuration
static uint32_t screenWidth;
static uint32_t screenHeight;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// Device control variables
static struct {
    bool pin1Flag = false;
    bool pin2Flag = false;
    uint8_t pin1MuxControl = 0;
    uint8_t pin2MuxControl = 0;
} deviceState;

// External declarations
extern struct HomeAssistant myData;
extern bool Switch1_cliked;
extern bool Switch2_cliked;
lv_timer_t* lvgl_task1 = nullptr;

/* Display bus configuration */
Arduino_DataBus *bus = new Arduino_ESP32LCD8(
    10, 11, 9 /* WR */, GFX_NOT_DEFINED /* RD */,
    46 /* D0 */, 3 /* D1 */, 8 /* D2 */, 18 /* D3 */,
    17 /* D4 */, 16 /* D5 */, 15 /* D6 */, 7 /* D7 */
);

Arduino_GFX *gfx = new Arduino_HX8369A(
    bus, -1, 1 /* rotation */, false /* IPS */,
    480, 800, 0, 0, 0, 0
);

#include "touch.h"
#include "ui.h"

// Change to your screen resolution

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    if (touch_has_signal()) {
        if (touch_touched()) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_last_x;
            data->point.y = touch_last_y;
        } else if (touch_released()) {
            data->state = LV_INDEV_STATE_REL;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
void handleDeviceControl(uint8_t deviceNumber, bool& flag, uint8_t& muxControl, int pin) {
    bool currentState = digitalRead(pin);
    
    if (currentState == HIGH && !flag) {
        flag = true;
        muxControl = 1;
    } else if (currentState == LOW && flag) {
        flag = false;
        muxControl = 0;
    } else {
        return;  // No state change
    }
    
    myData.DeviceNumber = deviceNumber;
    myData.PinMuxControl = muxControl;
    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t*)&myData, sizeof(myData));
    
    Serial0.print(deviceNumber == 1 ? "WoShi" : "ChuFang");
    Serial0.println(flag ? "Open" : "Off");
}

void handleSwitchControl(uint8_t deviceNumber, bool& clicked, uint8_t& muxControl) {
    if (!clicked) return;
    
    myData.DeviceNumber = deviceNumber;
    muxControl = ~muxControl;
    myData.PinMuxControl = muxControl;
    
    esp_err_t result = esp_now_send(BROADCAST_MAC, (uint8_t*)&myData, sizeof(myData));
    clicked = false;
}

void DeviceControlTask(lv_timer_t* tmr) {
    // Handle physical pin controls
    handleDeviceControl(1, deviceState.pin1Flag, deviceState.pin1MuxControl, 13);
    handleDeviceControl(2, deviceState.pin2Flag, deviceState.pin2MuxControl, 45);
    
    // Handle switch controls
    if (Switch1_cliked) handleSwitchControl(1, Switch1_cliked, deviceState.pin1MuxControl);
    if (Switch2_cliked) handleSwitchControl(2, Switch2_cliked, deviceState.pin2MuxControl);
}

SemaphoreHandle_t xGuiSemaphore;
#include "esp_task_wdt.h"
void Lvgl_Task(void *pvParameter){
  xGuiSemaphore = xSemaphoreCreateMutex();
    gfx->begin();
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
/*
  gfx->fillScreen(RED);
  delay(500);
  gfx->fillScreen(GREEN);
  delay(500);
  gfx->fillScreen(RED);
  delay(500);
  gfx->fillScreen(GREEN);
  delay(500);
  gfx->fillScreen(RED);
  delay(500);
*/
  lv_init();
  delay(10);
  touch_init();
  screenWidth = gfx->width();
  screenHeight = gfx->height();
#ifdef ESP32
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
  disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 4);
#endif
  if (!disp_draw_buf)
  {
    Serial0.println("LVGL disp_draw_buf allocate failed!");
  }
  else
  {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * screenHeight / 4);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Initialize the (dummy) input device driver */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // lv_demo_widgets();
    ui_init();

    
    Serial0.println("Setup done");

  }
  lv_timer_create(DeviceControlTask, 100, 0);
  while(1)
    {
    lv_timer_handler(); /* let the GUI do its work */
    // Serial0.print("Pin13:");
    // Serial0.println(digitalRead(13));
    // Serial0.print("Pin21:");
    // Serial0.println(digitalRead(21));
    vTaskDelay(5);
    }
}
// SemaphoreHandle_t xGuiSemaphore;
// #include "rtc_wdt.h"     //设置看门狗用

void WatchDogFeed(void *pvParameter){
  while(1){
    esp_task_wdt_reset();
    delay(800);
  }

}

// #include <esp_now.h>
// #include <WiFi.h>
 

void setup()
{
  // esp_task_wdt_deinit();
  // rtc_wdt_disable();
  Serial0.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin("vivo iQOO 10 Pro","Srd030308");//填写自己的wifi账号密码
  pinMode(0,INPUT_PULLUP);
  pinMode(13,INPUT);
  pinMode(45,INPUT);
  espnow_init();

  // rtc_wdt_protect_off();     //看门狗写保护关闭 关闭后可以喂狗
  //rtc_wdt_protect_on();    //看门狗写保护打开 打开后不能喂狗
  // rtc_wdt_disable();       //禁用看门狗
  // rtc_wdt_enable();          //启用看门狗
  // rtc_wdt_set_time(RTC_WDT_STAGE0, 7000);     // 设置看门狗超时 7000ms.

  // while (!Serial);
  // Serial0.println("LVGL Widgets Demo");
  // GetWeather();
  // pinMode(13,ANALOG);     
  // pinMode(0,INPUT_PULLUP);
  // Init touch device
// xGuiSemaphore = xSemaphoreCreateMutex();
  // Init Display

  xTaskCreatePinnedToCore(Talking_Task, "Talking_Task", 1024 * 8, NULL, 5, NULL,0);
  xTaskCreatePinnedToCore(Lvgl_Task,"Lvgl_Task", 1024 * 8, NULL, 5, NULL,1);
  // xTaskCreatePinnedToCore(Esp_now,"Esp_now", 1024, NULL, 5, NULL,1);
  xTaskCreatePinnedToCore(WatchDogFeed,"WatchDogFeed", 1024, NULL, 4, NULL,0);
  // xTaskCreatePinnedToCore(WatchDogFeed,"WatchDogFeed", 1024, NULL, 4, NULL,1);
  // xTaskCreate(Talking_Task, "Talking_Task", 1024 * 8, NULL, 5, NULL);
  // xTaskCreate(Lvgl_Task, "Lvgl_Task", 1024 * 8, NULL, 5, NULL);
}

void loop()
{
  
}
// void loop(){

// }
