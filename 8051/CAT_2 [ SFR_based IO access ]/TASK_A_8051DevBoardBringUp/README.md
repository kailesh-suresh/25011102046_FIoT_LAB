
# Experiment: CAT2

| Field                              | Details                                                                                                                  |
| :--------------------------------- | :----------------------------------------------------------------------------------------------------------------------- |
| **EXP**                            | CAT 2_A                                                                                                                     |
| **DATE**                           | 20/07/2026                                                                                                               |
|                       |                                                                                                                      |
| **EXP REQUIREMENT**                | Hardware Board Bring-Up |
| **HARDWARE SETUP / PLATFORM USED** | 8051 Development Board (AT89S52 Microcontroller) ,Keil uVision4.   *[ A USBasp programmer is a USB-based hardware device used to upload compiled programs (firmware) from your computer to a microcontroller.]*                                   |

### 1. Hardware Identification

The hardware components and their functions were identified as follows:

| Component | Identification / Function |
|:---|:---|
| Microcontroller | AT89S52 (8051, 8-bit) |
| Development Board | Amotech Labs' 8051 Development Board |
| Programmer | USBasp (USB-based ISP programmer) |
| Power Source | Laptop USB connection through USBasp, using target-power support |
| User LED | Onboard LED connected to P1.0 |
| GPIO Port | Port 1 |
| GPIO Pin | P1.0 |
| LED Active State | Active LOW |
| Operating Voltage | 5V DC |

### 2. Power Rail Verification

The board's power supply was checked using a Digital Multimeter (DMM).

**Measurement procedure:**

1. Set the multimeter to DC voltage mode.
2. Connect the black probe to GND.
3. Connect the red probe to the +5V power rail.
4. Observe the voltage reading.

| Parameter | Expected Reading |
|:---|:---:|
| VCC (+5V) to GND | Approximately 5V DC |
| GND to GND | 0V |

**Result:** The power rails were checked to ensure that the board receives the required operating voltage.


### 3. Identification of LED GPIO Pin

The onboard LED connection was identified using the Amotech Labs 8051 Development Board schematic.

| Parameter | Identified Value |
|:---|:---|
| GPIO Port | Port 1 |
| GPIO Pin | P1.0 |
| SFR Register | P1 |
| SFR Address | 0x90 |
| LED Configuration | Active LOW |

**Key Concept:** The P1.0 pin is associated with Bit 0 of the Port 1 Special Function Register (SFR). Since the onboard LED is Active LOW, writing `0` turns the LED ON, while writing `1` turns it OFF.

### 4. Host Computer–Microcontroller Communication

Communication between the host computer and the target microcontroller was established through the USBasp programmer.

**Communication Flow:**

```text
1. HOST COMPUTER
       |
       v
2. USB CONNECTION
       |
       v
3. USBasp PROGRAMMER
       |
       v
4. ISP PROGRAMMING INTERFACE
       |
       v
5. AT89S52 MICROCONTROLLER
       |
       v
6. PROGRAM STORED IN FLASH MEMORY
```


 The host computer–target microcontroller programming interface was established, enabling the compiled firmware to be transferred to the AT89S52.

---

### 5. Final Observation

The AT89S52 microcontroller, USBasp programmer, power supply, onboard LED, and corresponding GPIO pin were identified. The board's power rails were checked, and the host-to-target programming connection was established.