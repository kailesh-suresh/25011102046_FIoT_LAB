\# Experiment: CAT2



| Field | Details |

| :--- | :--- |

| \*\*EXP\*\* | CAT2 |

| \*\*DATE\*\* | 20/07/2026 \*\*CHK IN OBSERVATION\*\* |

| \*\*EXP REQUIREMENT\*\* | Compile a minimal bare-metal C program, program the device, and control an LED by directly accessing the memory-mapped GPIO registers. |

| \*\*HARDWARE SETUP / PLATFORM USED\*\* | 8051 Development Board, AT89S52 Microcontroller, USBasp Programmer, Keil uVision4. |

| \*\*TASK NO\*\* |  **02** |



\### Circuit Diagram

!\[Circuit Diagram](circuit.png)

\*(Ensure your circuit.png shows the AT89S52, the crystal oscillator, the power rails, and the LED connected to Port 1, Pin 0).\*



\### Hardware Bring-Up Sheet CHECK WITH VALUES IN OBSERVATION NOTE





| Parameter | Entry |

| :--- | :--- |

| MCU | AT89S52 |

| CPU Core | 8051 (8-bit) |

| Operating Voltage | 5V DC |

| Debug Interface | ISP (In-System Programming) |

| LED GPIO Port | Port 1 |

| LED GPIO Pin | P1.0 |

| LED Active State | Active LOW |

| LED Frequency (Hz) | \~1 Hz |



\### Core Logic / Pseudocode / Code Base



\*\*Logic:\*\* 

The LED is connected to Port 1, Pin 0 (`P1^0`). We use a custom `delay\_ms()` library function which uses nested `for` loops to generate a software delay. 

Because we are strictly forbidden from using HAL libraries or hardware timers for this task, this software loop acts as our delay mechanism. To achieve a 1 Hz blink rate, we toggle the LED pin and wait for 500ms.



\*\*\*WHY 500 SECONDS? - The Math behind\*\*\*





\*\*Given:\*\* Required blinking frequency = 1 Hz.



Frequency is the number of complete cycles per second.



$$  

f = \\frac{1}{T}  

$$



Therefore, the time period is:



$$  

T = \\frac{1}{f} = \\frac{1}{1} = 1 \\text{ second}  

$$



One complete cycle consists of LED ON and LED OFF durations.



Assuming equal ON and OFF times:



$$  

T = T\_{ON} + T\_{OFF}  

$$



$$  

1000 = 500 + 500 \\text{ ms}  

$$



\*\*Therefore:\*\*



\-   LED ON duration = 500 ms

&#x20;   

\-   LED OFF duration = 500 ms

&#x20;   



\*\*Result:\*\* The LED completes one ON-OFF cycle every second, achieving the required blinking frequency of approximately \*\*1 Hz\*\*.







