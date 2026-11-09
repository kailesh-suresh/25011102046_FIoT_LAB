# Experiment: CAT2

| Field                              | Details                                                                                                                  |
| :--------------------------------- | :----------------------------------------------------------------------------------------------------------------------- |
| **EXP**                            | CAT2                                                                                                                     |
| **DATE**                           | 20/07/2026                                                                                                               |
|                       |                                                                                                                      |
| **EXP REQUIREMENT**                | Compile a minimal bare-metal C program, program the device, and control an LED by directly accessing the GPIO registers. |
| **HARDWARE SETUP / PLATFORM USED** | 8051 Development Board (AT89S52 Microcontroller), USBasp Programmer, Keil uVision4.   *[ A USBasp programmer is a USB-based hardware device used to upload compiled programs (firmware) from your computer to a microcontroller.]*                                   |

**What does it mean by SFR-based I/O access ?**

The LED toggling operation passes through the following layers:

```text
1. SOFTWARE LAYER
   C Statement: LED = ~LED;
          |
          v
2. CPU LAYER
   8051 executes the translated instruction
   (e.g., CPL P1.0)
          |
          v
3. REGISTER LAYER
   Port 1 Special Function Register (SFR)
   Address: 0x90
   Bit 0 (P1.0) toggles its state
          |
          v
4. HARDWARE LAYER
   Physical GPIO Pin P1.0 changes voltage
   HIGH (5V) <-> LOW (0V)
          |
          v
5. OUTPUT
   LED turns ON / OFF
```

**Key Concept:** The C instruction modifies the P1.0 bit in the 8051's Special Function Register (SFR), causing the corresponding physical pin to change its output state.

**Note:** Since the LED is Active LOW, writing `0` turns it ON, while writing `1` turns it OFF.

### Circuit Diagram
Circuit showing the AT89S52, crystal oscillator, power rails, and LED connected to Port 1, Pin 0.
---

### Core Logic / Pseudocode / Code Base

**Logic and Code Base:**

The LED is connected to Port 1, Pin 0 (`P1^0`). A custom `delay_ms()` library function uses nested `for` loops to generate a software delay.

Since hardware timers and HAL libraries are not permitted for this task, the software loop acts as the delay mechanism. To achieve a 1 Hz blinking frequency, the LED is toggled and followed by a 500 ms delay.

**Pseudocode:**

```text
1. START

2. LED ← P1^0

3. FUNCTION delay_ms(ms):
       FOR i ← 0 TO ms-1
           FOR j ← 0 TO 119
               NULL

4. WHILE (1):
       LED ← ~LED
       delay_ms(500)

5. REPEAT
```
---

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

 The LED completes one ON-OFF cycle every second, achieving the required blinking frequency of approximately **1 Hz**.

---

