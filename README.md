# Smart Garden ESP32 + Blynk

An Internet of Things (IoT) based automatic plant watering system built on the
ESP32 microcontroller and the Blynk cloud platform. Unlike conventional timer or
fixed-threshold irrigation, this system uses a **multifactorial adaptive control
algorithm** that adjusts its watering thresholds in real time based on soil
moisture, air temperature, soil temperature, and air humidity. It was designed
and tested on the *Spathiphyllum* spp. (Peace Lily), a tropical plant that is
highly sensitive to both under and over watering.

This repository contains the firmware that accompanies the research paper
*"Design and Implementation of an Internet of Things (IoT)-Based Smart Garden
System Using ESP32 and the Blynk Platform for Plant Watering Automation."*

---

## Table of Contents

- [Features](#features)
- [How It Works](#how-it-works)
  - [Control Thresholds](#control-thresholds)
  - [Decision Priorities](#decision-priorities)
  - [Blynk Virtual Pins](#blynk-virtual-pins)
- [Hardware](#hardware)
  - [Bill of Materials](#bill-of-materials)
  - [Pin Connections](#pin-connections)
- [Software Setup](#software-setup)
  - [Required Libraries](#required-libraries)
  - [Installation](#installation)
- [Usage](#usage)
- [Repository Structure](#repository-structure)
- [Citation](#citation)
- [Authors](#authors)
- [License](#license)

---

## Features

- **Adaptive thresholds.** The "soil is dry" trigger shifts from 40% to 50%
  during hot or dry conditions so the plant is watered earlier and never loses
  turgor during heat peaks.
- **Cold lockout fail-safe.** The pump is force-disabled when the air drops
  below 15 C or the soil drops below 18 C, preventing watering into cold,
  oxygen-starved soil that invites root rot.
- **Overwatering guard.** Watering is force-stopped once soil moisture reaches
  the 70% upper limit, keeping the root zone inside the optimal 40 to 70% range.
- **Hysteresis control.** A dead band between the ON threshold and the 60%
  OFF point prevents the relay from rapidly chattering on and off.
- **Non-blocking cloud telemetry.** Sensor data is pushed to Blynk every 2
  seconds using `BlynkTimer`, so network latency never stalls the safety logic.
- **Local + remote display.** Live readings appear on a 16x2 I2C LCD and on the
  Blynk mobile and web dashboard at the same time.
- **Auto and manual modes.** Switch between fully automatic control and manual
  pump control from the dashboard.

---

## How It Works

The main routine `jalankanSistemUtama()` is called every 2 seconds. It reads all
sensors, pushes the values to Blynk, and (when in automatic mode) decides whether
the pump should run.

> **Pump wiring convention:** the relay is **active-LOW**. In code, `LOW` turns
> the pump **ON** and `HIGH` turns it **OFF**.

### Control Thresholds

All thresholds are defined as variables near the top of the sketch and can be
tuned for other plants.

| Parameter            | Value | Meaning                                            |
|----------------------|-------|----------------------------------------------------|
| `SP_KERING`          | 40 %  | Soil dry, pump turns ON (normal condition)         |
| `SP_KERING_STRES`    | 50 %  | ON threshold during heat or dry-air stress         |
| `SP_BASAH`           | 60 %  | Soil moist, pump turns OFF                          |
| `SP_BANJIR`          | 70 %  | Upper limit, pump forced OFF                        |
| `SUHU_LING_DINGIN`   | 15 C  | Air below this, pump blocked                        |
| `SUHU_LING_PANAS`    | 32 C  | Air above this counts as heat stress                |
| `SUHU_TANAH_DINGIN`  | 18 C  | Soil below this, pump blocked                        |
| `SUHU_TANAH_PANAS`   | 30 C  | Soil at or above this counts as heat stress         |
| `HUMI_KERING`        | 40 %  | Air humidity below this counts as dry stress        |

Soil moisture is read from an analog sensor and mapped from raw counts to a
0 to 100% scale using two calibration points: `AirValue = 2620` (sensor in dry
air) and `WaterValue = 1180` (sensor in water). Re-run this calibration for your
own sensor.

### Decision Priorities

When automatic mode is active, the logic is evaluated in this order:

1. **Protection (highest priority).** If the air or soil is too cold
   (`dinginEkstrem`), or the soil is flooded at 70% or above (`tanahBanjir`),
   the pump is forced OFF. A cold lockout shows `BLK` on the LCD; a flood shows
   `OFF`.
2. **Adaptive watering with hysteresis.** Otherwise, the active ON threshold is
   chosen: 50% if a heat or dry-air stress condition is detected, 40% otherwise.
   - If soil moisture is below the active threshold and the pump is off, it
     turns ON.
   - If soil moisture reaches 60% and the pump is on, it turns OFF.
   - Between those points the last state is held (the dead band).

### Blynk Virtual Pins

| Pin  | Direction | Function                                             |
|------|-----------|------------------------------------------------------|
| `V0` | Display   | Soil temperature (DS18B20), in C                     |
| `V1` | Display   | Soil moisture, in %                                  |
| `V2` | Display   | Air humidity (DHT11), in %                           |
| `V5` | Display   | Air / ambient temperature (DHT11), in C              |
| `V3` | Control   | Mode toggle: ON = Automatic, OFF = Manual            |
| `V4` | Control   | Manual pump switch (only works when in Manual mode)  |

---

## Hardware

### Bill of Materials

| Component                         | Notes                                          |
|-----------------------------------|------------------------------------------------|
| ESP32 DEVKIT board                | Dual-core MCU with built-in WiFi               |
| DHT11 sensor                      | Air temperature and relative humidity          |
| DS18B20 waterproof probe          | Soil (root-zone) temperature, 1-Wire           |
| Soil moisture sensor (analog)     | Volumetric water content                       |
| 1-channel relay module            | Switches the pump (active-LOW)                 |
| Submersible water pump            | Driven through the relay                       |
| 16x2 I2C LCD (address 0x27)       | Local readout                                  |
| 4.7k ohm resistor                 | Pull-up for the DS18B20 data line              |
| Water reservoir, tubing, wiring   | As needed                                      |

### Pin Connections

| Signal                  | ESP32 Pin            |
|-------------------------|----------------------|
| DHT11 data              | GPIO 5               |
| DS18B20 data (1-Wire)   | GPIO 4               |
| Relay / pump control    | GPIO 14              |
| Soil moisture (analog)  | A6 (analog input)    |
| LCD SDA                 | GPIO 21 (I2C default)|
| LCD SCL                 | GPIO 22 (I2C default)|

> The DS18B20 data line needs a 4.7k ohm pull-up resistor to 3.3V. Power all
> sensors from 3.3V unless your module specifically requires 5V logic.

---

## Software Setup

### Required Libraries

Install these through the Arduino IDE Library Manager:

- **Blynk** (`BlynkSimpleEsp32`)
- **LiquidCrystal_I2C**
- **DHT sensor library** (Adafruit)
- **DallasTemperature**
- **OneWire**

You will also need the **ESP32 board package** added to the Arduino IDE via the
Boards Manager.

### Installation

1. Clone or download this repository.
2. Open the `smart_garden_esp32/` folder, then open `smart_garden_esp32.ino`
   in the Arduino IDE.
3. In that same folder, copy `Secrets.h.example` to a new file named
   `Secrets.h`, and fill in your own WiFi and Blynk credentials:
   ```cpp
   #define BLYNK_TEMPLATE_ID    "TMPLxxxxxxxxx"
   #define BLYNK_TEMPLATE_NAME  "Smart Garden"
   #define BLYNK_AUTH_TOKEN     "YourBlynkAuthTokenHere"
   #define WIFI_SSID            "YourWiFiName"
   #define WIFI_PASS            "YourWiFiPassword"
   ```
   `Secrets.h` is listed in `.gitignore`, so it will not be committed.
4. Select **Tools > Board > ESP32 Dev Module** and the correct COM port.
5. Calibrate the soil moisture sensor if needed (update `AirValue` and
   `WaterValue`).
6. Upload the sketch. Open the Serial Monitor at **115200 baud** to watch the
   logs.

---

## Usage

On boot the LCD shows the moisture, soil temperature, air humidity, and pump
state. Use the Blynk dashboard to operate the system:

- **Automatic mode (V3 ON).** The controller manages watering on its own using
  the adaptive logic above. The manual switch is ignored.
- **Manual mode (V3 OFF).** The automatic logic pauses and you control the pump
  directly with the V4 switch.

The pump state field on the LCD reads `ON`, `OFF`, or `BLK` (blocked by the cold
lockout).

---

## Repository Structure

```
smart-garden-esp32-blynk/
├── README.md
├── LICENSE
├── .gitignore
└── smart_garden_esp32/
    ├── smart_garden_esp32.ino   # main firmware
    └── Secrets.h.example        # credentials template (copy to Secrets.h)
```

---

## Citation

If you use this work, please cite the accompanying paper:

```bibtex
@misc{sitompul_smartgarden,
  title  = {Design and Implementation of an Internet of Things (IoT)-Based
            Smart Garden System Using ESP32 and the Blynk Platform for Plant
            Watering Automation},
  author = {Sitompul, Daniel and Adrian, Adnan and Shafira, Khanza Almas and
            Hidayat, Muhammad Fadlan},
  year   = {2025},
  note   = {Computer Science Department, School of Computer Science,
            Bina Nusantara University, Jakarta, Indonesia}
}
```

---

## Authors

- **Daniel Sitompul**
- **Adnan Adrian**
- **Khanza Almas Shafira**
- **Muhammad Fadlan Hidayat**

Computer Science Department, School of Computer Science, Bina Nusantara
University, Jakarta, Indonesia.

---

## License

This project is released under the MIT License. See the [LICENSE](LICENSE) file
for details.
