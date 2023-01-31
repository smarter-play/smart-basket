# Protocol

Every packet consists of the following fields:
- `Type` (uint8)
- `Basket ID` (uint32)
- `Payload` (varying, depending on Type)

The `Type` field specifies how the `Payload` data will be formatted. Below a list of the supported packet types.

## Packet type: Basket (0x00)

`Payload`: - (no payload)

## Packet type: Accelerometer (0x01)

`Payload`:
- `Accelerometer X` (float)
- `Accelerometer Y` (float)
- `Accelerometer Z` (float)
- `Gyroscope X` (float)
- `Gyroscope Y` (float)
- `Gyroscope Z` (float)
- `Temperature` (float)

## Packet type: Custom button (0x02)

`Paylaod`:
- `Custom button index` (uint)


