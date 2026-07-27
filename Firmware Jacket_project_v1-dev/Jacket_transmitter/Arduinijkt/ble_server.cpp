// Requires the "NimBLE-Arduino" library (h2zero), v2.x callback API
// (NimBLECharacteristicCallbacks::onRead/onWrite take a NimBLEConnInfo& parameter).
#include "ble_server.h"
#include <NimBLEDevice.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "esp_log.h"

static const char *GATTS_TAG = "GATTS_SERVER";
static const char *DEVICE_NAME = "ESP-BLE";

// Service/characteristic UUIDs (kept identical to the original firmware)
#define GATTS_SERVICE_UUID_TEST_A     0x00FF  // Vitals: HR & SpO2
#define GATTS_CHAR_UUID_TEST_A_VITALS 0xFF01
#define GATTS_SERVICE_UUID_TEST_B     0x00EE  // Location: Lat & Lon
#define GATTS_CHAR_UUID_TEST_B_LOCATION 0xEE01
#define GATTS_SERVICE_UUID_TEST_C     0x00DD  // Temperature
#define GATTS_CHAR_UUID_TEST_C_TEMP   0xDD01
#define GATTS_SERVICE_UUID_TEST_D     0x00CC  // Text
#define GATTS_CHAR_UUID_TEST_D_TEXT   0xCC01

static NimBLEServer *pServer = nullptr;
static NimBLEAdvertising *pAdvertising = nullptr;
static NimBLECharacteristic *pCharVitals = nullptr;
static NimBLECharacteristic *pCharLocation = nullptr;
static NimBLECharacteristic *pCharTemp = nullptr;
static NimBLECharacteristic *pCharText = nullptr;

// Sensor data (mirrors the original static globals)
static float heart_rate = 0.0f;
static float spo2_level = 0.0f;
static float latitude = 0.0f;
static float longitude = 0.0f;
static float temperature = 0.0f;
static char user_text[100] = "Initial Text";

void send_notification_to_all_services(void)
{
    char buf[100];

    if (pCharVitals) {
        int len = snprintf(buf, sizeof(buf), "HR:%.2f,SpO2:%.2f", heart_rate, spo2_level);
        pCharVitals->setValue((uint8_t *)buf, len);
        pCharVitals->notify();
        ESP_LOGI(GATTS_TAG, "Sent Vitals notification: HR=%.2f, SpO2=%.2f", heart_rate, spo2_level);
    }

    if (pCharLocation) {
        int len = snprintf(buf, sizeof(buf), "%.6f,%.6f", latitude, longitude);
        pCharLocation->setValue((uint8_t *)buf, len);
        pCharLocation->notify();
        ESP_LOGI(GATTS_TAG, "Sent Location notification: Lat=%.6f, Lon=%.6f", latitude, longitude);
    }

    if (pCharTemp) {
        int len = snprintf(buf, sizeof(buf), "Temp:%.2f", temperature);
        pCharTemp->setValue((uint8_t *)buf, len);
        pCharTemp->notify();
        ESP_LOGI(GATTS_TAG, "Sent Temperature notification: %.2f", temperature);
    }

    if (pCharText) {
        int len = snprintf(buf, sizeof(buf), "Text:%s", user_text);
        pCharText->setValue((uint8_t *)buf, len);
        pCharText->notify();
        ESP_LOGI(GATTS_TAG, "Sent Text notification: %s", user_text);
    }
}

void update_sensor_data_and_notify(float bpm, float spo2, float lat, float lon, float temp)
{
    heart_rate = bpm;
    spo2_level = spo2;
    latitude = lat;
    longitude = lon;
    temperature = temp;
    send_notification_to_all_services();
}

void update_max30102_sensor_data(float bpm, float spo2)
{
    heart_rate = bpm;
    spo2_level = spo2;
    send_notification_to_all_services();
}

void update_gps_sensor_data(float lat, float lon)
{
    latitude = lat;
    longitude = lon;
    // no notify here, matches original firmware
}

void update_tpm116_sensor_data(float temp)
{
    temperature = temp;
    // no notify here, matches original firmware
}

void update_user_text(const char *text)
{
    strncpy(user_text, text, sizeof(user_text) - 1);
    user_text[sizeof(user_text) - 1] = '\0';
    send_notification_to_all_services();
}

// ---------------- Characteristic callbacks ----------------

class VitalsCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        char hr_str[16], spo2_str[16], buf[64];
        snprintf(hr_str, sizeof(hr_str), "%.2f", heart_rate);
        snprintf(spo2_str, sizeof(spo2_str), "%.2f", spo2_level);
        int len = snprintf(buf, sizeof(buf), "HR:%s,SpO2:%s", hr_str, spo2_str);
        pCharacteristic->setValue((uint8_t *)buf, len);
    }
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        char received[100];
        size_t len = value.size() < sizeof(received) - 1 ? value.size() : sizeof(received) - 1;
        memcpy(received, value.data(), len);
        received[len] = '\0';

        // Parse received data (format: "HR=72.5,SpO2=98.2")
        char *token = strtok(received, ",");
        while (token != NULL) {
            if (strstr(token, "HR=")) {
                heart_rate = atof(token + 3);
                ESP_LOGI(GATTS_TAG, "Updated HR: %.2f", heart_rate);
            } else if (strstr(token, "SpO2=")) {
                spo2_level = atof(token + 5);
                ESP_LOGI(GATTS_TAG, "Updated SpO2: %.2f", spo2_level);
            }
            token = strtok(NULL, ",");
        }
        send_notification_to_all_services();
    }
};

class LocationCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        char lat_str[20], lon_str[20], buf[64];
        snprintf(lat_str, sizeof(lat_str), "%.6f", latitude);
        snprintf(lon_str, sizeof(lon_str), "%.6f", longitude);
        int written = snprintf(buf, sizeof(buf), "%s,%s", lat_str, lon_str);
        int max_len = 22;
        int len = (written > max_len) ? max_len : written;
        pCharacteristic->setValue((uint8_t *)buf, len);
    }
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        char received[100];
        size_t len = value.size() < sizeof(received) - 1 ? value.size() : sizeof(received) - 1;
        memcpy(received, value.data(), len);
        received[len] = '\0';

        // Parse received data (format: "Lat=7.7749,Lon=2.4194")
        char *token = strtok(received, ",");
        while (token != NULL) {
            if (strstr(token, "Lat=")) {
                latitude = atof(token + 4);
                ESP_LOGI(GATTS_TAG, "Updated Latitude: %.4f", latitude);
            } else if (strstr(token, "Lon=")) {
                longitude = atof(token + 4);
                ESP_LOGI(GATTS_TAG, "Updated Longitude: %.4f", longitude);
            }
            token = strtok(NULL, ",");
        }
        send_notification_to_all_services();
    }
};

class TempCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        char temp_str[20], buf[32];
        snprintf(temp_str, sizeof(temp_str), "%.4f", temperature);
        int len = snprintf(buf, sizeof(buf), "Temp:%s", temp_str);
        pCharacteristic->setValue((uint8_t *)buf, len);
    }
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        char temp_text[20];
        size_t len = value.size() < sizeof(temp_text) - 1 ? value.size() : sizeof(temp_text) - 1;
        memcpy(temp_text, value.data(), len);
        temp_text[len] = '\0';

        if (strstr(temp_text, "Temp=")) {
            temperature = atof(temp_text + 5);
        } else {
            temperature = atof(temp_text);
        }
        ESP_LOGI(GATTS_TAG, "Updated Temperature: %.2f", temperature);
        send_notification_to_all_services();
    }
};

class TextCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        char buf[110];
        int len = snprintf(buf, sizeof(buf), "Text:%s", user_text);
        pCharacteristic->setValue((uint8_t *)buf, len);
    }
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        size_t len = value.size() < sizeof(user_text) - 1 ? value.size() : sizeof(user_text) - 1;
        memcpy(user_text, value.data(), len);
        user_text[len] = '\0';
        ESP_LOGI(GATTS_TAG, "Updated Text: %s", user_text);
        printf("Received text from user: %s\n", user_text);
        send_notification_to_all_services();
    }
};

static VitalsCallbacks vitalsCallbacks;
static LocationCallbacks locationCallbacks;
static TempCallbacks tempCallbacks;
static TextCallbacks textCallbacks;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override {
        ESP_LOGI(GATTS_TAG, "BLE client connected");
        // Send initial notifications on connect (matches original)
        send_notification_to_all_services();
    }
    void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override {
        ESP_LOGI(GATTS_TAG, "BLE client disconnected, reason 0x%x", reason);
        if (pAdvertising) pAdvertising->start();
    }
};

static ServerCallbacks serverCallbacks;

void ble_setup(void)
{
    NimBLEDevice::init(DEVICE_NAME);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    NimBLEService *svcVitals = pServer->createService(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_A));
    pCharVitals = svcVitals->createCharacteristic(
        NimBLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_A_VITALS),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    pCharVitals->setCallbacks(&vitalsCallbacks);
    pCharVitals->setValue("HR:0.00,SpO2:0.00");
    svcVitals->start();

    NimBLEService *svcLocation = pServer->createService(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_B));
    pCharLocation = svcLocation->createCharacteristic(
        NimBLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_B_LOCATION),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    pCharLocation->setCallbacks(&locationCallbacks);
    pCharLocation->setValue("0.000000,0.000000");
    svcLocation->start();

    NimBLEService *svcTemp = pServer->createService(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_C));
    pCharTemp = svcTemp->createCharacteristic(
        NimBLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_C_TEMP),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    pCharTemp->setCallbacks(&tempCallbacks);
    pCharTemp->setValue("Temp:0.00");
    svcTemp->start();

    NimBLEService *svcText = pServer->createService(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_D));
    pCharText = svcText->createCharacteristic(
        NimBLEUUID((uint16_t)GATTS_CHAR_UUID_TEST_D_TEXT),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    pCharText->setCallbacks(&textCallbacks);
    pCharText->setValue("Text:Initial Text");
    svcText->start();

    // Advertise the device name + all 4 service UUIDs.
    // (Original firmware omitted the Temperature/0x00DD UUID from its advertised
    // list - fixed here per the confirmed low-risk-bugfix decision.)
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_A));
    pAdvertising->addServiceUUID(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_B));
    pAdvertising->addServiceUUID(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_C));
    pAdvertising->addServiceUUID(NimBLEUUID((uint16_t)GATTS_SERVICE_UUID_TEST_D));
    pAdvertising->start();

    ESP_LOGI(GATTS_TAG, "BLE GATT server started, advertising as \"%s\"", DEVICE_NAME);
}
