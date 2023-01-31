# smart-basket



# The schematic

The smart-basket's circuit is divided into several parts:

### Power supply

The power supply must be able to provide 9V and it must be able to support the circuit functionalities. We haven't measured the minimum required power,
for our prototype we sticked to a power supply able to provide up to 3A.

### The MCU

The smart-basket logic and connectivity is achieved by an ESP8266, mounted on a NodeMCU board. The MCU is directly powered from the external 9V power supply throughout the Vin pin. The built-in voltage regulator mounted on the NodeMCU board will provide 3.3V output.





