#include <stdint.h>

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Hash.h>

#include "smart_basket.hpp"
#include "accelerometer.hpp"

#include "config.h"

uint32_t g_basket_id;

WiFiClient g_wifi_client; // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiClient.h

int32_t g_basket_fsm_state = SMP_BASKET_FSM_NOT_DETECTED;
int32_t g_basket_detected_timestamp = 0;

int32_t g_custom_button_fsm_state = 0;

struct smp_packet_basket_payload
{
};

struct smp_packet_accelerometer_data_payload
{
    float m_accel_x, m_accel_y, m_accel_z;
    float m_temperature;
};

struct smp_packet_custom_button_payload
{
    uint32_t m_button_idx;
};

// ------------------------------------------------------------------------------------------------

void establish_wifi_connection()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.print("Connecting to WiFi...");
        WiFi.begin(SMP_BRIDGE_AP_SSID, SMP_BRIDGE_AP_PASSWORD);
        
        while (WiFi.status() != WL_CONNECTED)
        {
            Serial.print(".");
        }
        Serial.println("");
    }
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
    sent &= g_wifi_client.write(packet_type) > 0;
    sent &= g_wifi_client.write(SMP_BASKET_COURT) > 0;
    sent &= g_wifi_client.write(reinterpret_cast<uint8_t*>(payload), payload_size) > 0;
    return sent;
}

#define SMP_SEND_PACKET_BASKET(_payload)             smp_send_packet_wifi(SMP_PACKET_TYPE_BASKET, &_payload, sizeof(_payload))
#define SMP_SEND_PACKET_ACCELEROMETER_DATA(_payload) smp_send_packet_wifi(SMP_PACKET_TYPE_ACCELEROMETER_DATA, &_payload, sizeof(_payload))
#define SMP_SEND_PACKET_CUSTOM_BUTTON(_payload)      smp_send_packet_wifi(SMP_PACKET_TYPE_CUSTOM_BUTTON, &_payload, sizeof(_payload))

void smp_custom_button_handle(
    int32_t& fsm_state,
    uint32_t custom_button_idx,
    uint8_t pin,
    void(*pressed_callback)(uint32_t)
)
{
    int status = digitalRead(pin);
    if (status)
    {
        fsm_state = custom_button_idx;
        pressed_callback(custom_button_idx);
    }
    else if (fsm_state == custom_button_idx && !status)
    {
        fsm_state = -1;
    }
}

// ------------------------------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------------------------------

void smp_on_custom_button_press(uint32_t custom_button_idx)
{
    smp_packet_custom_button_payload custom_button_payload{};
    custom_button_payload.m_button_idx = custom_button_idx;

    SMP_SEND_PACKET_CUSTOM_BUTTON(custom_button_payload);
}

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

void setup()
{ 
    Serial.begin(115200);

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

#ifdef SMP_CUSTOM_BUTTON_0
    pinMode(SMP_CUSTOM_BUTTON_0_PIN, INPUT);
#endif

#ifdef SMP_CUSTOM_BUTTON_1
    pinMode(SMP_CUSTOM_BUTTON_1_PIN, INPUT);
#endif

#ifdef SMP_ACCELEROMETER
    smp_accelerometer_init();
#endif
}

void loop()
{
    smp_packet_basket_payload basket_payload{};
    smp_accelerometer_data accelerometer_data{};
    smp_packet_accelerometer_data_payload accelerometer_data_payload{};

    // Re-try to connect if the connection was lost
    if (!g_wifi_client.connected())
    {
        //Serial.print("(Re-)establishing TCP connection\n");
        g_wifi_client.connect(SMP_BRIDGE_IP_ADDRESS, SMP_BRIDGE_PORT);
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

#ifdef SMP_CUSTOM_BUTTON_0
    smp_custom_button_handle(g_custom_button_fsm_state, 0, SMP_CUSTOM_BUTTON_0_PIN, smp_on_custom_button_press);
#endif

#ifdef SMP_CUSTOM_BUTTON_1
    smp_custom_button_handle(g_custom_button_fsm_state, 1, SMP_CUSTOM_BUTTON_1_PIN, smp_on_custom_button_press);
#endif

    // ----------------------------------------------------------------
    // Accelerometer
    // ----------------------------------------------------------------

#ifdef SMP_ACCELEROMETER
    smp_accelerometer_read_data(accelerometer_data);
    
    /*
    SMP_DEBUG_SERIAL_PRINTF("ACCELEROMETER AcX: %d, AcY: %d, AcZ: %d, Tmp: %d, GyX: %d, GyY: %d\n",
        accelerometer_data.m_AcX,
        accelerometer_data.m_AcY,
        accelerometer_data.m_AcZ,
        accelerometer_data.m_Tmp,
        accelerometer_data.m_GyX,
        accelerometer_data.m_GyY,
        accelerometer_data.m_GyZ
    );
    */

    accelerometer_data_payload.m_accel_x = 21.0; // TODO
    accelerometer_data_payload.m_accel_y = 22.0; // TODO
    accelerometer_data_payload.m_accel_z = 23.0; // TODO
    accelerometer_data_payload.m_temperature = 32.0; // TODO

    SMP_SEND_PACKET_ACCELEROMETER_DATA(accelerometer_data_payload);
#endif
}
