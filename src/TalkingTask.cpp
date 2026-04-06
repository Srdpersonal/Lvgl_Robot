#include "TalkingTask.h"
// #include "soc/rtc_wdt.h"
// #include "esp_task_wdt.h"

// 配置常量
namespace Config {
    constexpr char WIFI_SSID[] = "*******";
    constexpr char WIFI_PASSWORD[] = "*******";
    constexpr char API_HOST[] = "api.seniverse.com";
    constexpr char WEATHER_PRIVATE_KEY[] = "*********";
    constexpr char WEATHER_CITY[] = "ningbo";
    constexpr char WEATHER_LANGUAGE[] = "en";
    constexpr size_t ADC_DATA_LEN = 16000;  // 8K采样率，2秒录音时间
    constexpr size_t JSON_BUFFER_SIZE = 90000;  // 45000*2
}

// 全局变量
struct {
    hw_timer_t* timer = nullptr;
    uint16_t adcData[Config::ADC_DATA_LEN] = {0};
    char dataJson[Config::JSON_BUFFER_SIZE] = {0};
    bool adcStartFlag = false;
    bool adcCompleteFlag = false;
    HTTPClient httpClient;
} g_state;

// 天气数据结构
struct WeatherData {
    char city[32];
    char weather[64];
    char high[32];
    char low[32];
    char humidity[32];
};

// API URLs
const String ERNIE_API_URL = "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/ernie-speed-128k?access_token=****************************************";
const String WEATHER_API_URL = "https://api.seniverse.com/v3/weather/daily.json?key=*********************&location=ningbo&language=zh-Hans&unit=c&start=0&days=1";

extern char recordkey;

// 函数声明
String fenge(String str, String fen, int index);
void IRAM_ATTR onTimer();
void gain_token();

// AI对话功能实现
String getGPTAnswer(const String& inputText) {
    HTTPClient http;
    http.setTimeout(20000);
    http.begin(ERNIE_API_URL);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"messages\":[{\"role\": \"user\",\"content\": \"" + inputText + 
                    "\"}],\"disable_search\": false,\"enable_citation\": false}";

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
        String response = http.getString();
        http.end();
        Serial0.println(response);

        DynamicJsonDocument jsonDoc(1024);
        deserializeJson(jsonDoc, response);
        return jsonDoc["result"].as<String>();
    } 
    
    http.end();
    Serial0.printf("Error %i \n", httpResponseCode);
    return "<e>";
}
void updateWeatherUI(const WeatherData& weather);
// 天气获取功能实现
void GetWeather() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial0.println("WiFi not connected!");
        return;
    }

    WiFiClient client;
    if (!client.connect(Config::API_HOST, 80)) {
        Serial0.println("Connect host failed!");
        return;
    }

    // 构建请求URL
    String getUrl = "/v3/weather/daily.json?key=";
    getUrl += Config::WEATHER_PRIVATE_KEY;
    getUrl += "&location=";
    getUrl += Config::WEATHER_CITY;
    getUrl += "&language=";
    getUrl += Config::WEATHER_LANGUAGE;
    
    // 发送HTTP请求
    client.print(String("GET ") + WEATHER_API_URL + " HTTP/1.1\r\n" + 
                "Host: " + Config::API_HOST + "\r\n" + 
                "Connection: close\r\n\r\n");

    // 跳过HTTP头
    if (!client.find("\r\n\r\n")) {
        Serial0.println("Invalid response!");
        client.stop();
        return;
    }

    // 读取响应
    String response = client.readStringUntil('\n');
    DynamicJsonDocument doc(1400);
    
    if (deserializeJson(doc, response)) {
        Serial0.println("JSON parsing failed");
        client.stop();
        return;
    }

    // 解析天气数据
    WeatherData weather;
    JsonObject results = doc["results"][0];
    JsonObject daily = results["daily"][0];
    
    strlcpy(weather.city, results["location"]["name"], sizeof(weather.city));
    strlcpy(weather.weather, daily["text_day"], sizeof(weather.weather));
    strlcpy(weather.high, daily["high"], sizeof(weather.high));
    strlcpy(weather.low, daily["low"], sizeof(weather.low));
    strlcpy(weather.humidity, daily["humidity"], sizeof(weather.humidity));

    // 更新UI显示
    updateWeatherUI(weather);
    client.stop();
}

// UI更新函数
void updateWeatherUI(const WeatherData& weather) {
    lv_label_set_text(ui_WeatherDayDisplayLable, "");
    lv_label_set_text(ui_CityLabel, "城市：");
    lv_label_set_text(ui_CityDisplayLable, weather.city);
    lv_label_set_text(ui_FanLable, "厨房灯");
    lv_label_set_text(ui_LightLable, "卧室灯");
    lv_label_set_text(ui_WindSpeedDisplayLable, "天气：");
    lv_label_set_text(ui_WindSpeedLable, weather.weather);
    lv_label_set_text(ui_TempHighLable, "最高气温：");
    lv_label_set_text(ui_TempHighDisplayLable, weather.high);
    lv_label_set_text(ui_TempLowLable, "最低温度：");
    lv_label_set_text(ui_TempLowDisplayLable, weather.low);
}

// 主任务函数
void Talking_Task(void* pvParameter) {
    uint8_t wifiRetries = 0;
    const uint8_t MAX_WIFI_RETRIES = 75;

    while (WiFi.status() != WL_CONNECTED) {
        Serial0.print(".");
        if (++wifiRetries >= MAX_WIFI_RETRIES) {
            Serial0.println("\r\n-- WiFi connect fail! --");
            break;
        }
        vTaskDelay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial0.println("\r\n-- WiFi connect success! --");
    }

    GetWeather();

    g_state.timer = timerBegin(0, 80, true);    //  80M的时钟 80分频 1M
    timerAlarmWrite(g_state.timer, 125, true);  //  1M  计125个数进中断  8K
    timerAttachInterrupt(g_state.timer, &onTimer, true);
    timerAlarmEnable(g_state.timer);
    timerStop(g_state.timer);   //先暂停

    Serial0.println("Enter a prompt:");
    while(1){
        if(recordkey==1) //按键按下
        {
            recordkey=0;
            Serial0.printf("Start recognition\r\n\r\n");
            g_state.adcStartFlag=1;
            timerStart(g_state.timer);

            while(!g_state.adcCompleteFlag)  //等待采集完成
            {
                ets_delay_us(10);
            }
            g_state.adcCompleteFlag=0;        //清标志

            timerStop(g_state.timer);

            memset(g_state.dataJson,'\0',strlen(g_state.dataJson));   //将数组清空
            strcat(g_state.dataJson,"{");
            strcat(g_state.dataJson,"\"format\":\"pcm\",");
            strcat(g_state.dataJson,"\"rate\":8000,");         //采样率    如果采样率改变了，记得修改该值，只有16000、8000两个固定采样率
            strcat(g_state.dataJson,"\"dev_pid\":1537,");      //中文普通话
            strcat(g_state.dataJson,"\"channel\":1,");         //单声道
            strcat(g_state.dataJson,"\"cuid\":\"75414\",");   //识别码    随便打几个字符，但最好唯一
            strcat(g_state.dataJson,"\"token\":\"24.023d2b6e952c1eb38eac89ebcea3c35e.2592000.1718713590.282335-56466454\",");  //token	这里需要修改成自己申请到的token
            strcat(g_state.dataJson,"\"len\":32000,");         //数据长度  如果传输的数据长度改变了，记得修改该值，该值是ADC采集的数据字节数，不是base64编码后的长度
            strcat(g_state.dataJson,"\"speech\":\"");
            strcat(g_state.dataJson,base64::encode((uint8_t *)g_state.adcData,sizeof(g_state.adcData)).c_str());     //base64编码数据
            strcat(g_state.dataJson,"\"\"");
            strcat(g_state.dataJson,"}");

            int httpCode;
            g_state.httpClient.begin("http://vop.baidu.com/server_api");
            g_state.httpClient.addHeader("Content-Type","application/json");
            httpCode = g_state.httpClient.POST(g_state.dataJson);

            if(httpCode > 0) {
                if(httpCode == HTTP_CODE_OK) {
                    String payload = g_state.httpClient.getString();
                    Serial0.println(payload);

                    DynamicJsonDocument jsonDoc(1024);
                    deserializeJson(jsonDoc, payload);
                    String outputText = jsonDoc["result"];
                    Serial0.println(outputText);

                    String inputText = outputText;
                    inputText.remove(inputText.length()-2,2);
                    inputText.remove(0,2);
                    Serial0.println("\n Input:"+inputText);
                    const char * Quest;
                    Quest = inputText.c_str();
                    lv_textarea_set_text(ui_Request,Quest);

                    String answer = getGPTAnswer(inputText);
                    Serial0.println("Answer: " + answer);
                    Serial0.println("Enter a prompt:");

                    const char * ASW;
                    ASW = answer.c_str();
                    lv_textarea_set_text(ui_TextArea1,ASW);
                }
            }
            else {
                Serial0.printf("[HTTP] GET... failed, error: %s\n", g_state.httpClient.errorToString(httpCode).c_str());
            }
            g_state.httpClient.end();

            Serial0.printf("Recognition complete\r\n");
        }

        delay(5);
    }
}
uint32_t num=0;
// 定时器中断处理函数
void IRAM_ATTR onTimer() {
    portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL_ISR(&timerMux);
    if(g_state.adcStartFlag==1)
    {
        g_state.adcData[num]=analogRead(ADC);
        num++;
        if(num>=Config::ADC_DATA_LEN)
        {
            g_state.adcCompleteFlag=1;
            g_state.adcStartFlag=0;
            num=0;
        }
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

// Token获取函数
void gain_token()   //获取token
{
    int httpCode;
    g_state.httpClient.begin("https://aip.baidubce.com/oauth/2.0/token?grant_type=client_credentials&client_id=HTNdJhmW0S0xiXn2dDkpeKHb&client_secret=2CogwNbJWkQbsn2Fpaq0cSZUpx9dgkJd");
    httpCode = g_state.httpClient.GET();
    if(httpCode > 0) {
        if(httpCode == HTTP_CODE_OK) {
            String payload = g_state.httpClient.getString();
            Serial0.println(payload);
        }
    }
    else {
        Serial0.printf("[HTTP] GET... failed, error: %s\n", g_state.httpClient.errorToString(httpCode).c_str());
    }
    g_state.httpClient.end();
}

// 字符串分割函数
String fenge(String str,String fen,int index)
{
 int weizhi;
 String temps[str.length()];
 int i=0;
 do
 {
    weizhi = str.indexOf(fen);
    if(weizhi != -1)
    {
      temps[i] =  str.substring(0,weizhi);
      str = str.substring(weizhi+fen.length(),str.length());
      i++;
      }
      else {
        if(str.length()>0)
        temps[i] = str;
      }
 }
  while(weizhi>=0);

  if(index>i)return "-1";
  return temps[index];
}
