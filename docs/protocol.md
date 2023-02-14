---
layout: default
---

# Protocol

Every packet consists of the same header:

| Field | Type | Description |
| --- | --- | --- |
| `Type` | `uint8_t` | |
| `BasketID` | `uint32_t` | |
| `Payload` | `Varying` | Its content depends on `Type` |

The `Payload` field is dependant on the packet type. Here's a list of the supported packet types:

## Basket (type 0x00)

**Payload**:

| Field | Type | Description |
| --- | --- | --- |
| `Unused` | `uint8_t` | |

## Accelerometer (type 0x01)

**Payload**:

| Field | Type | Description |
| --- | --- | --- |
| `AccelerometerX` | `float` | |
| `AccelerometerY` | `float` | |
| `AccelerometerZ` | `float` | |
| `GyroX` | `float` | |
| `GyroY` | `float` | |
| `GyroZ` | `float` | |
| `Temperature` | `float` | |

## CustomButton (type 0x02)

**Payload**:

| Field | Type | Description |
| --- | --- | --- |
| `CustomButtonIndex` | `uint32_t` | |

## PeopleDetected (type 0x03)

**Payload**:

| Field | Type | Description |
| --- | --- | --- |
| `Unused` | `uint8_t` | |
