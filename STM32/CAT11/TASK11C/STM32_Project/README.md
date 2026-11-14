# TASK11C – Node-RED as a Second HTTP Client

## EXP
Use Node-RED as a second HTTP client to request the STM32–Wemos HTTP server and display the returned response.

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
TASK11C

## Circuit Diagram
See [CAT11 overall circuit diagram](../Circuit_Diagram_CAT11.png). The same hardware arrangement is used across CAT11 tasks. The Wemos power is separate USB power.

## Core Logic / Pseudocode / Code Base
The STM32 firmware and Node-RED flow are provided separately:
- STM32 firmware configures the ESP8266 TCP server and serves the HTTP response.
- `NodeRED/CAT11C_NodeRED_Flow.json` contains an Inject → HTTP Request (GET) → Debug flow.
- Configure the HTTP Request node URL as `http://<ESP8266_IP>/`, replacing the placeholder with the Wemos IP address shown by the module.
- Deploy the flow, click Inject, and inspect the Debug sidebar for the response.

**Project files:** The complete STM32CubeMX/Keil project is preserved in `STM32_Project/`, including the `.ioc` file, `Core/`, `Drivers/`, and `MDK-ARM/` project dependencies.

## Node-RED Files
See [`NodeRED/README.md`](NodeRED/README.md) for import, IP configuration and run instructions.

## INFERENCE
Node-RED is configured as a second HTTP client. The supplied flow file is included for import into Node-RED. A demonstration was reported by the user; no screenshot evidence was included in the uploaded archives.
