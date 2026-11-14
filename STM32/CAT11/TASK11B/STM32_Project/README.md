# TASK11B – Observe HTTP Request and Send Static Response

## EXP
Receive an HTTP request from a client and return a static HTTP response through the ESP8266 modem.

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
TASK11B

## Circuit Diagram
See [Circuit_Diagram_CAT11](Circuit_Diagram_CAT11.png). The same hardware arrangement is used across CAT11 tasks. The Wemos power is separate USB power.

## Core Logic / Pseudocode / Code Base
1. Initialize the STM32 peripherals and configure the ESP8266 Wi-Fi connection and TCP server.
2. Receive ESP8266 data on USART1.
3. Detect the `+IPD` notification and extract the connection/link ID.
4. Accumulate the incoming request until the HTTP header terminator `\\r\\n\\r\\n` is detected.
5. Display the received request through USART2.
6. For a GET request, build a static HTTP `200 OK` response with `Content-Type`, `Content-Length`, and an HTML body.
7. Send `AT+CIPSEND=<link_id>,<response_length>`, wait for the `>` prompt, transmit the response bytes, then close the connection.

**Project files:** The complete STM32CubeMX/Keil project is preserved in `STM32_Project/`, including the `.ioc` file, `Core/`, `Drivers/`, and `MDK-ARM/` project dependencies.

The parser is a demonstration implementation based on the supplied source; do not interpret it as a general-purpose HTTP server stack.

## INFERENCE
The firmware handles the demonstrated HTTP request path and sends a static HTML response. The response is not live sensor data; the supplied implementation serves fixed page content.
