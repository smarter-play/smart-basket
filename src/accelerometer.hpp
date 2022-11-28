#pragma once

#include <Wire.h>

#include "smart_basket.hpp"

// GY-521 communication code:
// https://olivertechnologydevelopment.wordpress.com/2017/08/17/esp8266-sensor-series-gy-521-imu-part-1/

struct smp_accelerometer_data
{
    int16_t m_AcX, m_AcY, m_AcZ;
    int16_t m_Tmp;
    int16_t m_GyX, m_GyY, m_GyZ;
};

void smp_accelerometer_init()
{
    Wire.begin();
    Wire.beginTransmission(SMP_ACCELEROMETER_I2C_ADDRESS);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0);    // Set to zero (wakes up the MPU-6050)
    Wire.endTransmission(true);
}

void smp_accelerometer_read_data(smp_accelerometer_data& data)
{
    Wire.beginTransmission(SMP_ACCELEROMETER_I2C_ADDRESS);
    Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
    Wire.endTransmission(false);

    Wire.requestFrom(SMP_ACCELEROMETER_I2C_ADDRESS, 14, true); // request a total of 14 registers

    data.m_AcX = Wire.read() << 8 | Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
    data.m_AcY = Wire.read() << 8 | Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    data.m_AcZ = Wire.read() << 8 | Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    data.m_Tmp = Wire.read() << 8 | Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    data.m_GyX = Wire.read() << 8 | Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    data.m_GyY = Wire.read() << 8 | Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    data.m_GyZ = Wire.read() << 8 | Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

    data.m_Tmp = data.m_Tmp / 340.0 + 36.53;
}
