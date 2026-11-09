# Experiment: CAT2

| Field                              | Details                                                                                                                                |
| :--------------------------------- | :------------------------------------------------------------------------------------------------------------------------------------- |
| **EXP**                            | CAT2                                                                                                                                   |
| **DATE**                           | 20/07/2026 **chk note**                                                                                                                            |
| **TASK NO**                        | 02 **chk observation**                                                                                                                 |
| **EXP REQUIREMENT**                | Compile a minimal bare-metal C program, program the device, and control an LED by directly accessing the memory-mapped GPIO registers. |
| **HARDWARE SETUP / PLATFORM USED** | 8051 Development Board, AT89S52 Microcontroller, USBasp Programmer, Keil uVision4.                                                     |

### Circuit Diagram



*Ensure that circuit.png shows the AT89S52, crystal oscillator, power rails, and LED connected to Port 1, Pin 0.*

### Hardware Bring-Up Sheet -- chk observation note once

| Parameter          | Entry                       |
| :----------------- | :-------------------------- |
| MCU                | AT89S52                     |
| CPU Core           | 8051 (8-bit)                |
| Operating Voltage  | 5V DC                       |
| Debug Interface    | ISP (In-System Programming) |
| LED GPIO Port      | Port 1                      |
| LED GPIO Pin       | P1.0                        |
| LED Active State   | Active LOW                  |
| LED Frequency (Hz) | \~1 Hz                      |

### Core Logic / Pseudocode / Code Base

**Logic:**

The LED is connected to Port 1, Pin 0 (`P1^0`). A custom `delay_ms()` library function uses nested `for` loops to generate a software delay.

Since hardware timers and HAL libraries are not permitted for this task, the software loop acts as the delay mechanism. To achieve a 1 Hz blinking frequency, the LED is toggled and followed by a 500 ms delay.

**PseudoCode:**
1. START
2. LED ← P1^0
3. delay_ms(ms):
       FOR i ← 0 TO ms-1
           FOR j ← 0 TO 119
               NULL
4. WHILE (1)
       LED ← ~LED
       delay_ms(500)
5. REPEAT
6. STOP

### Why 500 ms? – Mathematical Calculation

**Given:** Required blinking frequency = 1 Hz.

Frequency is the number of complete cycles per second.

$$
f = \frac{1}{T}
$$

Therefore, the time period is:

$$
T = \frac{1}{f} = \frac{1}{1} = 1 \text{ second}
$$

One complete cycle consists of LED ON and LED OFF durations.

Assuming equal ON and OFF times:

$$
T = T\_{ON} + T\_{OFF}
$$

$$
1000 = 500 + 500 \text{ ms}
$$

**Therefore:**

- LED ON duration = 500 ms
- LED OFF duration = 500 ms

**Result:** The LED completes one ON-OFF cycle every second, achieving the required blinking frequency of approximately **1 Hz**.
