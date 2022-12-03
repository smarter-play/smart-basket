#ifndef _SMP_SMART_BASKET_H
#define _SMP_SMART_BASKET_H

// ------------------------------------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------------------------------------

#if defined(SMP_PHOTO_DIODE_0) || defined(SMP_PHOTO_DIODE_1)
#   define SMP_DETECT_BASKET
#endif

// D1: I2C's SCL
// D2: I2C's SDA
#define SMP_PHOTO_DIODE_0_PIN D5
#define SMP_PHOTO_DIODE_1_PIN D6
#define SMP_CUSTOM_BUTTON_0_PIN D7
#define SMP_CUSTOM_BUTTON_1_PIN D8
#define SMP_ACCELEROMETER_I2C_ADDRESS 0x68

#define SMP_PACKET_TYPE_BASKET             0x00
#define SMP_PACKET_TYPE_ACCELEROMETER_DATA 0x01
#define SMP_PACKET_TYPE_CUSTOM_BUTTON      0x02
#define SMP_PACKET_TYPE_MAC_ADDRESS        0x03

// ------------------------------------------------------------------------------------------------
// Firmware configuration
// ------------------------------------------------------------------------------------------------

// Comment out one of the following lines to disable a peripheral:

#define SMP_PHOTO_DIODE_0
//#define SMP_PHOTO_DIODE_1
//#define SMP_CUSTOM_BUTTON_0
//#define SMP_CUSTOM_BUTTON_1
#define SMP_ACCELEROMETER

#define SMP_DEBUG

#if defined(SMP_PHOTO_DIODE_0) || defined(SMP_PHOTO_DIODE_1)
#   define SMP_DETECT_BASKET
#endif

// ------------------------------------------------------------------------------------------------

#endif // _SMP_SMART_BASKET_H
