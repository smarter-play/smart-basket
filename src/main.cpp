#include <stdint.h>
#include <math.h>

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Hash.h>
#include <MPU6050.h>

#include "smart_basket.hpp"

#include "config.h"

#define SMP_MPU_FSM_INIT      0
#define SMP_MPU_FSM_READ_DATA 1

#define SMP_BASKET_FSM_NOT_DETECTED 0
#define SMP_BASKET_FSM_DETECTED     1

#define SMP_MPU_MOVEMENT_THRESHOLD 1.0f
#define SMP_MPU_SEND_PACKET_DELAY 500

uint32_t g_basket_id;

WiFiClient g_wifi_client; // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClient.h
MPU6050 g_mpu; // https://github.com/pinchy/MPU6050

int32_t g_basket_fsm_state = SMP_BASKET_FSM_NOT_DETECTED;
uint64_t g_basket_detected_timestamp = 0;

int g_mpu_state = SMP_MPU_FSM_INIT;
uint64_t g_mpu_query_timestamp = 0;
uint64_t g_mpu_sent_packet_timestamp = 0;

int32_t g_custom_button_fsm_state = -1;

struct smp_packet_basket_payload
{
};

struct smp_packet_accelerometer_data_payload
{
    float m_accel_x, m_accel_y, m_accel_z;
    float m_gyro_x, m_gyro_y, m_gyro_z;
    float m_temp;
};

struct smp_packet_custom_button_payload
{
    uint32_t m_button_idx;
};

// ------------------------------------------------------------------------------------------------

uint32_t smp_derive_basket_id()
{
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    uint8_t digest[20];
    uint32_t basket_id = 0;

    WiFi.macAddress(mac);
    sha1(mac, WL_MAC_ADDR_LENGTH * sizeof(uint8_t), digest);

    for (int i = 0; i < 20; i++)
    {
        reinterpret_cast<uint8_t*>(&basket_id)[i % 4] ^= digest[i];
    }

    return basket_id;
}

bool smp_enter_detected_basket_state()
{
    uint32_t detected = 0;

#ifdef SMP_PHOTO_DIODE_0
    detected |= digitalRead(SMP_PHOTO_DIODE_0_PIN);
#endif

#ifdef SMP_PHOTO_DIODE_1
    detected |= digitalRead(SMP_PHOTO_DIODE_1_PIN);
#endif

    return !detected;
}

bool smp_exit_detected_basket_state()
{
    return !smp_enter_detected_basket_state();
}

bool smp_send_packet_wifi(uint8_t packet_type, void* payload, size_t payload_size)
{
    bool sent = true;
    sent &= g_wifi_client.write(packet_type) == 1;
    sent &= g_wifi_client.write(reinterpret_cast<uint8_t*>(&g_basket_id), sizeof(uint32_t)) == sizeof(uint32_t);
    if (payload_size > 0)
        sent &= g_wifi_client.write(reinterpret_cast<uint8_t*>(payload), payload_size) == payload_size;

    if (!sent)
        Serial.printf("ERROR: Packet of type %d couldn't be sent\n", packet_type);

    return sent;
}

#define SMP_SEND_PACKET_BASKET(_payload)             smp_send_packet_wifi(SMP_PACKET_TYPE_BASKET, &_payload, sizeof(_payload))
#define SMP_SEND_PACKET_ACCELEROMETER_DATA(_payload) smp_send_packet_wifi(SMP_PACKET_TYPE_ACCELEROMETER_DATA, &_payload, sizeof(_payload))
#define SMP_SEND_PACKET_CUSTOM_BUTTON(_payload)      smp_send_packet_wifi(SMP_PACKET_TYPE_CUSTOM_BUTTON, &_payload, sizeof(_payload))

void smp_custom_button_handle(int32_t& fsm_state, uint32_t custom_button_idx, uint8_t pin, void(*pressed_callback)(uint32_t))
{
    int pressed = (~digitalRead(pin)) & 1;
    if (fsm_state != (int32_t) custom_button_idx && pressed)
    {
        fsm_state = (int32_t) custom_button_idx;
        pressed_callback(custom_button_idx);
    }
    else if (fsm_state == (int32_t) custom_button_idx && !pressed)
    {
        fsm_state = -1;
    }
}

void smp_on_custom_button_press(uint32_t custom_button_idx)
{
    smp_packet_custom_button_payload custom_button_payload{};
    custom_button_payload.m_button_idx = custom_button_idx;

    SMP_SEND_PACKET_CUSTOM_BUTTON(custom_button_payload);
}

void smp_die()
{
    while (true)
        yield(); // Allow the execution of background functions
}

// ------------------------------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------------------------------

void setup()
{ 
    Serial.begin(115200);
    Wire.begin();

    // ----------------------------------------------------------------
    // Calculate Basket ID based on MAC address
    // ----------------------------------------------------------------

    // Basket ID is derived on initialization from WiFi module's MAC address. This is done
    // in order to ensure to always have a unique and permanent Basket ID
    g_basket_id = smp_derive_basket_id();
    Serial.printf("Derived basket ID: %d\n", g_basket_id);

#ifdef SMP_PHOTO_DIODE_0
    pinMode(SMP_PHOTO_DIODE_0_PIN, INPUT);
#endif

#ifdef SMP_PHOTO_DIODE_1
    pinMode(SMP_PHOTO_DIODE_1_PIN, INPUT);
#endif

    pinMode(SMP_CUSTOM_BUTTON_0_PIN, INPUT);
    pinMode(SMP_CUSTOM_BUTTON_1_PIN, INPUT);

    // ----------------------------------------------------------------
    // Connect to AP
    // ----------------------------------------------------------------

    Serial.printf("Connecting to AP (SSID: %s)...\n", SMP_BRIDGE_AP_SSID);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);

    WiFi.begin(SMP_BRIDGE_AP_SSID, SMP_BRIDGE_AP_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
        yield();

    WiFi.setAutoReconnect(true);

    Serial.printf("Connected! SSID: %s, RSSI: %d, IP: %s\n",
        WiFi.SSID().c_str(),
        WiFi.RSSI(),
        WiFi.localIP().toString().c_str()
    );
}

void loop()
{
    smp_packet_basket_payload basket_payload{};
    smp_packet_accelerometer_data_payload accelerometer_data_payload{};

    // ----------------------------------------------------------------
    // Connect to bridge
    // ----------------------------------------------------------------

    if (WiFi.status() == WL_CONNECTED && !g_wifi_client.connected())
    {
        Serial.printf("Establishing TCP connection to %s:%d...", SMP_BRIDGE_IP_ADDRESS, SMP_BRIDGE_PORT);

        if (g_wifi_client.connect(SMP_BRIDGE_IP_ADDRESS, SMP_BRIDGE_PORT))
            Serial.printf(" Connected\n");
        else
            Serial.printf(" Failed\n");
    }

    // ----------------------------------------------------------------
    // Detect the basket
    // ----------------------------------------------------------------

#ifdef SMP_DETECT_BASKET
    if (g_basket_fsm_state == SMP_BASKET_FSM_NOT_DETECTED && smp_enter_detected_basket_state())
    {
        Serial.printf("Basket detected\n");

        g_basket_fsm_state = SMP_BASKET_FSM_DETECTED;
        g_basket_detected_timestamp = millis();

        // Empty payload
        SMP_SEND_PACKET_BASKET(basket_payload);
    }
    else if (
        g_basket_fsm_state == SMP_BASKET_FSM_DETECTED &&
        smp_exit_detected_basket_state() &&
        (millis() - g_basket_detected_timestamp) > 1500 // Wait a delay to avoid bouncing
    )
    {
        g_basket_fsm_state = SMP_BASKET_FSM_NOT_DETECTED;
    }
#endif

    // ----------------------------------------------------------------
    // Custom buttons
    // ----------------------------------------------------------------

    smp_custom_button_handle(g_custom_button_fsm_state, 0, SMP_CUSTOM_BUTTON_0_PIN, smp_on_custom_button_press);
    smp_custom_button_handle(g_custom_button_fsm_state, 1, SMP_CUSTOM_BUTTON_1_PIN, smp_on_custom_button_press);

    // ----------------------------------------------------------------
    // Accelerometer
    // ----------------------------------------------------------------

#ifdef SMP_ACCELEROMETER
    if (
        Wire.status() == I2C_OK &&
        !g_mpu.available()
    )
    {
        //Serial.printf("MPU6050 lost\n");

        g_mpu_state = SMP_MPU_FSM_INIT;
    }

    if (
        g_mpu_state == SMP_MPU_FSM_READ_DATA &&
        Wire.status() == I2C_OK &&
        (g_mpu_query_timestamp - millis()) >= 50)
    {
        g_mpu.update();

        vector_t accel = g_mpu.getAcceleration();
        vector_t gyro = g_mpu.getGyro();
        //attitude_t attitude = g_mpu.getAttitude();
        float temp = g_mpu.getTemperature();

        if (
            (abs(accel.x) > SMP_MPU_MOVEMENT_THRESHOLD || abs(accel.y) > SMP_MPU_MOVEMENT_THRESHOLD || abs(accel.z) > SMP_MPU_MOVEMENT_THRESHOLD) &&
            (millis() - g_mpu_sent_packet_timestamp) >= SMP_MPU_SEND_PACKET_DELAY
        )
        {
            Serial.printf("MPU6050: AccelX: %f, AccelY: %f, AccelZ: %f, GyroX: %f, GyroY: %f, GyroZ: %f, Temp: %f\n",
                accel.x, accel.y, accel.z,
                gyro.x, gyro.y, gyro.z,
                temp
            );

            accelerometer_data_payload.m_accel_x = accel.x;
            accelerometer_data_payload.m_accel_y = accel.y;
            accelerometer_data_payload.m_accel_z = accel.z;
            accelerometer_data_payload.m_gyro_x  = gyro.x;
            accelerometer_data_payload.m_gyro_y  = gyro.y;
            accelerometer_data_payload.m_gyro_z  = gyro.z;
            accelerometer_data_payload.m_temp    = temp;

            SMP_SEND_PACKET_ACCELEROMETER_DATA(accelerometer_data_payload);

            g_mpu_sent_packet_timestamp = millis();
        }

        g_mpu_query_timestamp = millis();
    }
    else if (
        g_mpu_state == SMP_MPU_FSM_INIT &&
        Wire.status() == I2C_OK
    )
    {
        //Serial.printf("MPU6050 init...\n");

        if (g_mpu.begin())
        {
            g_mpu.calibrate();

            g_mpu_state = SMP_MPU_FSM_READ_DATA;
        }
    }
#endif
}
