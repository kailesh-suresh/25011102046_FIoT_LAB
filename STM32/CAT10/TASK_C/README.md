# CAT10C — Connect Wemos ESP8266 to Wi-Fi and Retrieve IP Address

## EXP
CAT10C

## DATE
Not specified in the supplied project files. Enter the date of the experiment performed.

## NAME OF EXP
Connect Wemos ESP8266 to Wi-Fi and Retrieve IP Address

*Note: This title is based on the supplied source-code header and implementation. The questions PDF was not included, so confirm the exact experiment title against that document.*

## HARDWARE SETUP / PLATFORM
- **Microcontroller board:** STM32F103C8T6 (Blue Pill-class MCU; exact board name should be confirmed against the physical setup)
- **Wi-Fi module:** Wemos ESP8266
- **Development platform:** STM32CubeMX project configuration and Keil MDK-ARM project
- **Communication:** UART, 115200 baud, 8 data bits, no parity, 1 stop bit
- **USART1:** STM32 communicates with the ESP8266.
- **USART2:** STM32 forwards ESP8266 responses to a PC serial terminal.

### UART connections

| STM32 pin | Function | Connects to |
|---|---|---|
| PA9 | USART1_TX | ESP8266 RX |
| PA10 | USART1_RX | ESP8266 TX |
| PA2 | USART2_TX | USB-to-UART adapter RX / PC terminal |
| PA3 | USART2_RX | USB-to-UART adapter TX / PC terminal |
| GND | Common ground | ESP8266 and USB-to-UART adapter GND |

Use compatible 3.3 V logic levels. Power the ESP8266 from a supply suitable for its current requirements; do not assume the STM32 board's 3.3 V output can supply the module.

## TASK NO
CAT10C

## Circuit Diagram

The supplied ZIP does not contain a drawn circuit-diagram image. The connection representation below is based on the USART pin assignments in `10_C.ioc` and the UART instances initialized in `Core/Src/main.c`.

```text
             STM32F103C8T6
          +-------------------+
          |                   |
 PA9  ----| USART1_TX          |----> ESP8266 RX
 PA10 <---| USART1_RX          |<---- ESP8266 TX
          |                   |
 PA2  ----| USART2_TX          |----> USB-to-UART RX ----> PC
 PA3  <---| USART2_RX          |<---- USB-to-UART TX <---- PC
 GND  ----| GND               |------ Common GND
          +-------------------+

        ESP8266 power: suitable 3.3 V supply
```

Confirm the ESP8266 board's pin labels and power requirements before wiring.

## Core Logic / Pseudocode / Code Base

### Core logic
The firmware initializes the HAL, system clock, GPIO, USART1, and USART2. It then communicates with the ESP8266 using AT commands over USART1 and forwards received responses to the PC over USART2.

The sequence implemented in `Core/Src/main.c` is:

1. Send `AT` to test communication with the ESP8266.
2. Send `AT+CWMODE=1` to select station mode.
3. Send `AT+CWJAP` with the SSID and password defined in the source to attempt Wi-Fi connection.
4. Send `AT+CIFSR` to request the IP address.
5. Forward the received responses through USART2.
6. Enter a permanent delay loop after the command sequence.

### Pseudocode
```text
Initialize HAL
Configure system clock
Initialize GPIO
Initialize USART1 and USART2

Repeat:
    Send "AT" to ESP8266 through USART1
    Receive response and forward it through USART2

    Send "AT+CWMODE=1"
    Receive response and forward it through USART2

    Send Wi-Fi join command
    Wait for response and forward it through USART2

    Send "AT+CIFSR"
    Receive response and forward it through USART2

    Stop in an infinite delay loop
```

### Code base
- `Core/Src/main.c` — application logic, AT-command transmission, response reception, UART initialization, and system clock configuration.
- `Core/Inc/` — application header files.
- `Drivers/` — STM32 HAL and CMSIS dependencies.
- `10_C.ioc` — STM32CubeMX configuration.
- `.mxproject` — STM32CubeMX project metadata.
- `MDK-ARM/10_C.uvprojx` — Keil uVision project.

## INFERENCE
The experiment demonstrates UART-based communication between an STM32 microcontroller and an ESP8266 Wi-Fi module using AT commands. The firmware attempts to configure the ESP8266 in station mode, connect it to the configured wireless network, and request its IP address. The module's responses are forwarded to a PC serial terminal for observation.

Successful Wi-Fi association and IP-address retrieval depend on correct wiring, suitable power, compatible ESP8266 firmware, and valid Wi-Fi credentials. The supplied source code does not parse or independently validate the returned IP address; it forwards the module's response to the PC terminal.

### Important note before publishing
The supplied `Core/Src/main.c` contains a hard-coded Wi-Fi SSID and password. Replace them with non-sensitive local values or remove them before publishing the repository publicly. The original source file is retained unchanged in this package.

### Documentation details to confirm
- Enter the actual experiment date.
- Confirm the exact `NAME OF EXP` wording against the questions PDF.
- Add a proper circuit-diagram image if one is required by the lab submission format; the supplied ZIP did not include one.
