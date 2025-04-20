#include "espnowTask.h"

// ESP-NOW 配置
namespace Config {
    // 目标设备的 MAC 地址
    const uint8_t BROADCAST_MAC[] = {0xA8, 0x42, 0xE3, 0x4C, 0x35, 0xA4};
}

// 全局变量
HomeAssistant myData;

// ESP-NOW 发送回调函数
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// ESP-NOW 初始化函数
void espnow_init() {
    // 初始化数据结构
    myData.PinMuxControl = 0;
    myData.DeviceNumber = 0;

    // 初始化 ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // 注册发送回调函数
    esp_now_register_send_cb(OnDataSent);

    // 配置通信对等节点
    esp_now_peer_info_t peerInfo = {
        .channel = 0,            // 通信通道,
        .ifidx = WIFI_IF_STA,   // 使用 STA 接口,
        .encrypt = false        // 不加密
        
        
    };
    memcpy(peerInfo.peer_addr, Config::BROADCAST_MAC, 6);

    // 添加对等节点
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}