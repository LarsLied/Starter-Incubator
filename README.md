# Sourdough Incubator – Project Description for Copilot

## Project goal

An ESP32-S3 based **sourdough starter incubator**. The device monitors a sourdough starter in a closed chamber and should eventually:

1. Detect when the starter has peaked and is ready for baking (based on rise height measured over time).
2. Estimate the remaining time until the starter is ready.
3. Control the chamber temperature (heating **and** cooling) with a Peltier element.
4. Show status on a small round display.
5. Connect to home Wi-Fi and push data to a cloud backend.

**Current phase (Phase 1):** Create the project skeleton and a clean, layered folder structure, and implement **reading of all sensors with output to the serial terminal only**. Everything beyond that should exist only as clearly marked stubs/placeholders. The developer will extend the application logic himself, so prioritise clean interfaces, readability and comments over features.

---

## Toolchain and conventions

- **Framework:** ESP-IDF v5.x (latest stable), plain C. **No Arduino.**
- **IDE:** VS Code with the official ESP-IDF extension.
- **Target:** `esp32s3` (ESP32-S3 DevKit).
- Use the modern ESP-IDF driver APIs (`driver/gpio.h`, `driver/gptimer.h`, `driver/rmt_*.h`, `driver/spi_master.h`, `esp_timer.h`), not deprecated legacy drivers.
- Logging with `ESP_LOGx` and a `TAG` per module. Return `esp_err_t` from all driver/HAL functions and check errors with `ESP_ERROR_CHECK` / `ESP_RETURN_ON_ERROR`.
- All GPIO pins and tunables are configurable through **Kconfig** (`idf.py menuconfig`), not hard-coded.
- FreeRTOS tasks for periodic work; no busy loops in `app_main`.
- Code style: snake_case, module prefix on public functions (e.g. `hx711_read_raw()`), one header per module with doc comments describing units.
- Use SI-ish units consistently: grams, °C, %RH, millimetres, milliseconds.

---

## Hardware

| Function | Part | Interface | Notes |
|---|---|---|---|
| MCU | ESP32-S3 DevKit | – | 3.3 V logic |
| Weight | 1 kg load cell + **HX711** amplifier | 2-wire (DOUT, SCK), bit-banged | Load cell wires: red E+, black E−, white A−, green A+. Needs tare + calibration factor. |
| Temperature / humidity | **DHT22** (AM2302) | Single-wire proprietary protocol | Minimum 2 s between reads. Needs pull-up (~10 kΩ) on data line if not on a module. Timing-critical – use RMT or careful microsecond timing. |
| Dough height | **JSN-SR04T** waterproof ultrasonic sensor | TRIG / ECHO (pulse width) | Range 20–800 cm, **20 cm blind zone**, ~1 mm resolution. 10 µs trigger pulse; echo high time = round-trip time. Power at 3.3 V, or use a voltage divider on ECHO if powered at 5 V. |
| Display | 1.28" round TFT, **GC9A01**, 240×240 | 4-wire SPI | Backlight is tied to VCC (no BL pin). **Phase 1: driver stub only.** |
| Thermal actuator | **Peltier element** (TBD) | PWM + direction (H-bridge, TBD) | Heating and cooling by reversing current. Power stage is not designed yet. **Phase 1: placeholder only.** |

### Default pin map (override in menuconfig)

Avoid ESP32-S3 strapping pins (0, 3, 45, 46), USB pins (19, 20) and flash/PSRAM pins (26–37).

| Signal | Default GPIO |
|---|---|
| HX711 DOUT | 4 |
| HX711 SCK | 5 |
| DHT22 DATA | 6 |
| JSN-SR04T TRIG | 7 |
| JSN-SR04T ECHO | 15 |
| GC9A01 SCLK / MOSI / CS / DC / RST | 12 / 11 / 10 / 9 / 8 |
| Peltier PWM / DIR (placeholder) | 16 / 17 |

---

## Architecture

Strict layering. Higher layers only depend on the layer directly below through headers.

```
app (logic)          fermentation monitoring, estimation, control loop, UI
   │
services             sensor_manager, thermal_control, wifi, cloud
   │
hal (drivers)        hx711, dht22, jsn_sr04t, gc9a01, peltier
   │
ESP-IDF
```

- **drivers/HAL:** Talk to hardware only. No application logic, no Wi-Fi, no display calls.
- **services:** Combine drivers into useful data (e.g. `sensor_manager` produces one `sensor_snapshot_t` with all readings + timestamp). Apply calibration and unit conversion here.
- **app:** Business logic. Consumes `sensor_snapshot_t`, never calls drivers directly.

### Folder structure

```
sourdough-incubator/
├── CMakeLists.txt
├── sdkconfig.defaults            # target esp32s3, log level, etc.
├── README.md
├── .github/
│   └── copilot-instructions.md   # this file
├── docs/
│   └── hardware.md               # wiring, pin map, part notes
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild         # pins, intervals, calibration defaults
│   └── main.c                    # init + start tasks only
└── components/
    ├── hx711/                    # include/hx711.h, hx711.c, CMakeLists.txt
    ├── dht22/
    ├── jsn_sr04t/
    ├── gc9a01/                   # STUB in phase 1
    ├── peltier/                  # STUB in phase 1 (set_power(-100..100 %))
    ├── sensor_manager/           # periodic sampling -> sensor_snapshot_t
    ├── thermal_control/          # STUB: future PID heat/cool controller
    ├── fermentation/             # STUB: rise tracking, peak detection, time-to-ready estimate
    ├── wifi_manager/             # STUB: STA connect to home Wi-Fi
    └── cloud_client/             # STUB: publish snapshots (MQTT/HTTP TBD)
```

Each component has its own `CMakeLists.txt` with `idf_component_register(...)` and explicit `REQUIRES`.

---

## Phase 1 – what to implement now

1. **Project skeleton** that builds with `idf.py set-target esp32s3 && idf.py build`.
2. **HX711 driver**
   - `hx711_init`, `hx711_is_ready`, `hx711_read_raw` (24-bit, channel A gain 128), `hx711_read_average(n)`.
   - `hx711_tare()` and `hx711_set_scale(float counts_per_gram)`; `hx711_get_grams()`.
   - Disable interrupts only for the short clocking sequence (critical section).
3. **DHT22 driver**
   - `dht22_init`, `dht22_read(float *temp_c, float *rh)`.
   - Checksum validation, return error on timeout/bad checksum, enforce ≥2 s between reads (return cached value otherwise).
4. **JSN-SR04T driver**
   - `jsn_sr04t_init`, `jsn_sr04t_measure_mm(uint32_t *distance_mm)`.
   - 10 µs trigger, measure echo width with GPIO interrupt + `esp_timer_get_time()` (or RMT), timeout ~60 ms.
   - Optional speed-of-sound compensation using air temperature: `v = 331.3 + 0.606 * T` m/s.
   - Median of N samples to reject outliers.
5. **sensor_manager**
   - A FreeRTOS task sampling all sensors every `CONFIG_SAMPLE_INTERVAL_MS` (default 5000 ms).
   - Fills a `sensor_snapshot_t { int64_t timestamp_us; float weight_g; float temp_c; float humidity_pct; uint32_t distance_mm; per-sensor valid flags; }`.
   - A sensor failing must not stop the others; mark it invalid and log a warning.
6. **main.c** – init NVS, init sensor_manager, print each snapshot to the terminal as one readable line, e.g.
   `I (12345) app: T=26.4C RH=71.2% W=412.3g D=231mm`
7. **Stubs** for `gc9a01`, `peltier`, `thermal_control`, `fermentation`, `wifi_manager`, `cloud_client`: header with the intended public API and doc comments, `.c` file with functions that log `"not implemented"` and return `ESP_ERR_NOT_SUPPORTED`. Mark them with `// TODO(phase2)`.

**Do not** implement Wi-Fi, cloud, display graphics, temperature control or the estimation algorithm yet.

---

## Future phases (for context only – do not implement)

- **Calibration:** store HX711 tare/scale and the empty-chamber distance (sensor-to-jar-bottom) in NVS; simple serial/console command to calibrate.
- **Dough height:** `height_mm = empty_distance_mm - measured_distance_mm`; rise % relative to height at feeding time.
- **Fermentation logic:** detect feeding events (sudden weight increase), track rise curve, detect peak (rise rate → 0 and starting to fall), estimate time-to-peak from rise rate and temperature.
- **Weight usage:** show starter mass and help with feeding ratios (e.g. 1:1:1 starter:flour:water).
- **Thermal control:** PID controller driving the Peltier in both directions (heat/cool) towards a setpoint, with safety limits and sensor-failure fallback (power off).
- **Display:** round UI showing temperature, humidity, rise %, and estimated time to ready (LVGL is a candidate).
- **Connectivity:** Wi-Fi STA with provisioning, publish snapshots to a cloud backend (MQTT or HTTPS, TBD), possibly push notifications when ready.

---

## Known hardware considerations

- The **JSN-SR04T has a 20 cm blind zone and a wide beam (~50°)**. It must be mounted at least ~20 cm above the highest expected dough surface, and reflections from jar walls may affect readings. Keep the distance driver behind a generic interface (`distance_sensor`-style API) so it can be swapped for a ToF sensor (e.g. VL53L0X/VL53L1X) later.
- HX711 readings drift with temperature; plan for re-taring.
- DHT22 is slow and occasionally fails a read – always handle errors gracefully.
- A reversible Peltier needs an **H-bridge**, not a single MOSFET. Keep `peltier_set_power(int8_t percent)` (−100 = full cooling, +100 = full heating) as the abstraction so the power stage can change.
