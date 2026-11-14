# TASK11A – Configure ESP8266 as TCP Server

## EXP
Configure the ESP8266 Wi-Fi modem as a TCP server using AT commands.

## DATE
Not specified

## NAME OF EXP
HTTP Server - STM32–Wemos Wi-Fi Modem Using AT Commands

## HARDWARE SETUP / PLATFORM
- MCU: Blue Pill STM32F103C8T6
- Wi-Fi module: Wemos D1 mini (ESP8266)
- Debug interface: USB-to-TTL adapter connected to USART2 and a PC serial terminal (e.g. PuTTY)
- USART1: PA9 (STM32 TX) → Wemos RX; PA10 (STM32 RX) ← Wemos TX
- USART2: PA2 (STM32 TX) → USB-to-TTL RX; PA3 (STM32 RX) ← USB-to-TTL TX
- UART configuration: 115200 baud, 8 data bits, no parity, 1 stop bit (8N1), no flow control
- Wemos D1 mini is powered separately through USB. Connect common GND between the STM32, Wemos and USB-to-TTL adapter.
- Use 3.3 V UART logic. Do not apply a 5 V UART signal to the ESP8266 RX pin.

## TASK NO
TASK11A

## Circuit Diagram
See [Circuit_Diagram_CAT11](Circuit_Diagram_CAT11.png). The same hardware arrangement is used across CAT11 tasks. The Wemos power is separate USB power.

## Core Logic / Pseudocode / Code Base
1. Initialize STM32 HAL, clocks, GPIO and USART1/USART2.
2. Send `AT` to check modem communication.
3. Set station mode with `AT+CWMODE=1`.
4. Join the configured Wi-Fi network using `AT+CWJAP`.
5. Query the module network information using `AT+CIFSR`.
6. Enable multiple connections using `AT+CIPMUX=1`.
7. Start the TCP server on port 80 using `AT+CIPSERVER=1,80`.
8. Forward received ESP8266 UART data to the USART2 serial monitor.

The Wi-Fi SSID and password are placeholders in the packaged source. Set them locally before running.

**Project files:** The complete STM32CubeMX/Keil project is preserved in `STM32_Project/`, including the `.ioc` file, `Core/`, `Drivers/`, and `MDK-ARM/` project dependencies.

The supplied source implements TCP server setup and serial forwarding; this task does not itself implement the HTTP request parser/HTML response.

## INFERENCE
The firmware configures the ESP8266 in station mode and starts a multi-connection TCP server on port 80. The serial monitor provides visibility into the modem responses.
