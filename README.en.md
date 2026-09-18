<div align="center">

[🇹🇷 Türkçe](README.md) &nbsp;·&nbsp; **🇬🇧 English**

# orientation-cube

Real-time orientation measurement and 3D cube visualisation with an ESP32 and a BMI160

[![License](https://img.shields.io/badge/code-AGPL--3.0--only-3DA639?style=flat-square)](LICENSE)
[![Docs](https://img.shields.io/badge/docs-CC%20BY--SA%204.0-EF9421?style=flat-square)](NOTICE)
[![MCU](https://img.shields.io/badge/ESP32-WROOM--32D-E7352C?style=flat-square&logo=espressif&logoColor=white)](#)
[![Tests](https://img.shields.io/badge/unit%20tests-29-2DD4BF?style=flat-square)](test)

</div>

<p align="center">
  <img src="varlik/kart.jpg" alt="ESP32, BMI160, 1.44 inch TFT, 18650 cell and power modules on a perfboard" width="560">
</p>

Rotate the board in your hand and the cube on the screen turns **the same way,
by the same angle**. Gyroscope and accelerometer data from the BMI160 are fused
with a complementary filter. The cube is rotated on the ESP32 itself and drawn
in perspective on a 128×128 display. No Wi-Fi, no Bluetooth, no host computer:
everything runs on the board.

This was the end-of-term project for an Electrical & Electronic Measurements
course (spring 2026). After submission the code was restructured, and a
scaling bug in the submitted version was found. Details below.

> The course report and code comments are in Turkish.

---

## What's in it

- **Sensor fusion.** A complementary filter (α = 0.96). The gyroscope is
  precise short-term but drifts when integrated. The accelerometer doesn't
  drift but is sensitive to vibration. The filter combines the two.
- **Zero-rate calibration at boot.** While the board is held still, 300
  samples are averaged into a gyro offset. A progress bar shows on screen.
- **3D on the MCU.** Three-axis rotation matrices, perspective projection
  and back-face culling. Visible faces are filled, each in its own colour.
- **Both cores used.** Core 0 reads and filters the sensor at a fixed 100 Hz.
  Core 1 only draws. On a single core, slow SPI drawing would disrupt sampling.
- **Tested without the board.** The filter and the 3D maths are
  hardware-free modules. The same code runs on the ESP32 and is tested on the
  desktop with `g++`.

## Bug found after submission: gyro scale

The submitted code divided raw gyro readings by `131.2`, the sensitivity for
the ±250 °/s range. The `DFRobot_BMI160` library, however, configures the
gyro for **±2000 °/s**, where the sensitivity is **16.4 LSB/(°/s)**. Angular
rates were therefore measured **8× too small**.

The bug was being masked, without anyone noticing, by an empirically tuned
"sensitivity multiplier". The gyro was multiplied by 16, and yaw by 8. That
made yaw accidentally correct, while pitch and roll turned 2× too far. The
constant is now fixed, the multiplier is gone, and a unit test pins the value.

The full reasoning and the other fixes (a watchdog reset, an unguarded
perspective divisor, filter state being read back from a cross-core shared
variable) are in [`docs/DEGISIKLIKLER.md`](docs/DEGISIKLIKLER.md) (Turkish).

> ⚠ The scale fix is verified against the library source and by calculation.
> **It has not been run on the board yet.**

## Hardware

| Part | Model | Interface |
|---|---|---|
| Dev board | ESP32-WROOM-32D, DevKit V1 (30 pin) | — |
| Display | 1.44" TFT, ST7735, 128×128 | SPI (VSPI) |
| IMU | BMI160, 3-axis gyro + 3-axis accelerometer | I²C, `0x68` |
| Power | 18650 Li-ion + MT3608 boost converter | — |

| Signal | ESP32 GPIO |
|---|---|
| TFT CS · RST · DC · MOSI · SCK | 5 · 4 · 2 · 23 · 18 |
| BMI160 SDA · SCL | 21 · 22 |
| BMI160 SDO | GND (fixes address to `0x68`) |

The display and the IMU run on **3.3 V**. Do not connect the display to 5 V.

<p align="center">
  <img src="varlik/prototip.jpg" alt="First prototype on a breadboard" width="320"><br>
  <sub>First prototype on a breadboard</sub>
</p>

## Build and flash

Arduino IDE or `arduino-cli` with the **esp32** core and the libraries
`Adafruit GFX`, `Adafruit ST7735 and ST7789`, `DFRobot_BMI160`.

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:FlashMode=dio firmware/orientation_cube
arduino-cli upload  --fqbn esp32:esp32:esp32:FlashMode=dio -p <PORT> firmware/orientation_cube
```

- **Flash Mode: DIO.** This board gives `flash read err` with QIO.
- If the upload hangs at `Connecting...`, hold **BOOT** and release it once
  `Writing` appears.
- Keep the board still for ~1.5 s while the calibration screen is shown.

## Tests

```bash
cd test && make
```

No dependencies, `-Wall -Wextra -Wpedantic -Werror`. The tests record the
reasoning as well as the behaviour. For example, they show that without
calibration the angle does **not** drift without bound: the complementary
filter settles the bias error into a fixed offset of about 0.9°. With a pure
gyro, the same error grows without limit.

## Known limitations

- **Yaw drifts.** Gravity doesn't change with rotation about Z, so the
  accelerometer can't correct yaw. A full fix needs a magnetometer, and the
  BMI160 has none.
- **Flicker.** Each frame clears the whole screen and redraws it. Double
  buffering (128×128×2 = 32 KB) would fix this but isn't implemented yet.
- **Euler angles.** Subject to gimbal lock. Quaternions would be the next step.

## Layout

```
firmware/orientation_cube/
  orientation_cube.ino   hardware layer: setup, calibration screen, drawing, Core 0/1
  src/yonelim.*          complementary filter + bias calibration (hardware-free)
  src/kup3b.*            3D rotation, perspective, back-face culling (hardware-free)
test/                    desktop unit tests
docs/PROJE_RAPORU.md     the submitted course report (Turkish)
docs/DEGISIKLIKLER.md    what changed after submission, and why (Turkish)
```

## License

Code is **AGPL-3.0-only** ([LICENSE](LICENSE)). Documentation and images are
**CC BY-SA 4.0** ([NOTICE](NOTICE)).
