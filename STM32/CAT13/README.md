# CAT13 – TASK13: BME280 Sensor Data Monitoring using ESP8266 Wi-Fi

## 1. Objective

To interface the BME280 environmental sensor with the STM32F401RB microcontroller, retrieve temperature, pressure and humidity readings, and transmit the sensed data wirelessly using the ESP8266 Wi-Fi module. The readings can then be viewed through a web browser by accessing the device's IP address.

## 2. Hardware Components

- STM32F401RB development board
- BME280 environmental sensor
- ESP8266 Wi-Fi module
- USB power/programming connection
- Connecting wires
- Wi-Fi network
- Computer or smartphone with a web browser

## 3. Working Principle

1. The BME280 sensor measures environmental parameters such as temperature, atmospheric pressure and humidity.
2. The STM32F401RB communicates with the BME280 and retrieves the sensor readings.
3. The microcontroller processes the received data.
4. The ESP8266 Wi-Fi module is used to establish wireless network connectivity and transfer the sensor data.
5. The device makes the sensed information accessible over the network.
6. When a user enters the device's IP address in a web browser, the browser displays the available sensor readings.

## 4. System Architecture

![CAT13 System Architecture](CAT13.png)

**Data flow:**  
BME280 Sensor → STM32F401RB → ESP8266 → Wi-Fi Network → Web Browser

## 5. Expected Output

The web page displays the environmental readings obtained from the BME280 sensor:

- Temperature
- Atmospheric Pressure
- Humidity

The readings can be accessed by entering the IP address of the device in a browser connected to the appropriate network.

## 6. Project Structure

```text
TASK13/
├── Core/
├── Drivers/
├── MDK-ARM/
├── .mxproject
├── README.md
├── TASK13.ioc
└── CAT13.png
```

## 7. Testing and Verification

- Software testing: Completed.
- Hardware testing: Completed.
- Sensor interfacing and data acquisition: Tested.
- ESP8266 Wi-Fi communication: Tested.
- Browser-based access using the device IP address: Tested.

## 8. Result

The BME280 sensor readings are acquired using the STM32F401RB and transmitted wirelessly through the ESP8266 Wi-Fi module. The sensed environmental parameters can be viewed through a web browser by accessing the device's IP address.

**Thus, sensor-based environmental monitoring with wireless data transmission and browser-based visualization was successfully implemented.**