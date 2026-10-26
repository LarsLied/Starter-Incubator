# Hardware

## Pin map (defaults, change in `idf.py menuconfig` → Sourdough Incubator → Pins)

| Signal | GPIO | Notes |
|---|---|---|
| HX711 DOUT | 4 | |
| HX711 SCK | 5 | |
| DHT22 DATA | 6 | 3-pin module has 5.1 kΩ pull-up on board |
| JSN-SR04T TRIG | 7 | |
| JSN-SR04T ECHO | 15 | Voltage divider if sensor powered at 5 V |
| GC9A01 SCLK / MOSI / CS / DC / RST | 12 / 11 / 10 / 9 / 8 | Backlight tied to VCC |
| Peltier PWM / DIR | 16 / 17 | Placeholder, power stage TBD |

Avoid strapping pins (0, 3, 45, 46), USB (19, 20) and flash/PSRAM (26–37).

## HX711 + load cell

| Load cell wire | HX711 |
|---|---|
| Red | E+ |
| Black | E− |
| White | A− |
| Green | A+ |

HX711 VCC/VDD to 3.3 V. With the RATE pin low it runs at 10 samples/s.
If the weight goes *down* when you load the cell, swap A+ and A−.

### Calibration

1. Boot with the scale empty (tare on boot is on by default).
2. Put a known mass `M` grams on it and read `raw=` from the log line.
3. Calculate `(raw − offset) / M`. The offset is printed as `tare offset = …` at boot.
4. Enter the result ×100 in menuconfig → Calibration → *HX711 scale factor*.

Readings drift with temperature – re-tare when needed (`sensor_manager_tare()`).

## DHT22 (AM2302)

3-pin module with an on-board 5.1 kΩ pull-up between + and out:

| Module pin | ESP32-S3 |
|---|---|
| + | 3V3 |
| out | GPIO 6 |
| − | GND |

- The data pin is configured as open drain. The internal pull-up is off by default
  (`CONFIG_DHT22_INTERNAL_PULLUP`); enable it only for a bare sensor without a pull-up.
- Do not read it more often than every 2 s; the driver returns the cached value if you do.
- An occasional failed read (timeout/checksum) is normal and is logged as a warning.

## JSN-SR04T

- Powering it at 3.3 V lets you connect ECHO directly. At 5 V, put a divider on ECHO
  (e.g. 1 kΩ / 2 kΩ).
- **Blind zone is 20 cm**: readings below 200 mm are rejected. Mount the sensor at least
  ~20 cm above the highest expected dough surface.
- The beam is wide (~50°). Jar walls can give false echoes; a narrow, tall jar is worse.
- The distance is compensated with the air temperature from the DHT22:
  `v = 331.3 + 0.606·T` m/s.

## Peltier (not designed yet)

A reversible Peltier needs an **H-bridge**, not a single MOSFET. The software
abstraction is `peltier_set_power(-100..+100 %)` (− = cooling, + = heating).
