# Lua Atmos on ESP32

## API Documentation
For the complete API signature reference, list of hardware functions, event payloads, and code examples, see:
👉 **[ESP32 Atmos API Reference](doc/api.md)**

---

## Project Background & Repositories

This project uses a modified port of the **Atmos** reactive programming runtime:
* **Original Project:** Created by Francisco Sant'Anna ([lua-atmos/atmos](https://github.com/lua-atmos/atmos)).
* **Original Streams Library:** Created by Francisco Sant'Anna ([fsantanna/f-streams](https://github.com/fsantanna/f-streams)).
* **ESP32 Port Core Repository:** Pedro Valle's Atmos port for Lua 5.5 ([pgvalle/atmos](https://github.com/pgvalle/atmos)).
* **ESP32 Port Streams Repository:** Pedro Valle's Streams port for Lua 5.5 ([pgvalle/f-streams](https://github.com/pgvalle/f-streams)).

---

## Initial Setup & Dependency Paths
Before building the project, you must clone the Lua 5.5 compatible ports and configure their paths in the build configuration:

---

## Key Features

* **Hardware Drivers & Modules:**
  * **GPIO:** Digital input/output and edge-triggered interrupts (`gpio.watch`).
  * **PWM / Fade:** Hardware LEDC-based pulse-width modulation and smooth duty cycle fading (`gpio.fade_pwm`), emitting `gpio.pwm_fade` events.
  * **ADC:** High-speed analog reads (`gpio.read_adc`) with adjustable voltage attenuation.
  * **ESP-NOW:** Low-latency connectionless peer-to-peer wireless communication (`espnow.send`), emitting `espnow.recv` and `espnow.send` events.
  * **IR Receiver:** Hardware RMT-based infrared decoding supporting NEC, Samsung, Sony SIRC, and LG protocols, emitting `ir` events.
  * **Preferences:** Key-value non-volatile storage (NVS) backed persistence for integers, booleans, and strings.
* **Resource Optimization:** Incremental garbage collection tuning, selective library loading, and custom event queues to preserve internal SRAM.

---

## Project Structure

```text
├── atm/                    # Atmos application code (compiled into LittleFS image)
│   └── init.atm            # Main Atmos application entrypoint
├── main/                   # ESP-IDF C application source code
│   ├── main.c              # NVS, Filesystem, and Lua VM startup
│   ├── gpio.c              # C bindings for GPIO, PWM, and ADC (_esp.gpio)
│   ├── espnow.c            # C bindings for ESP-NOW WiFi radio driver (_esp.espnow)
│   ├── ir/                 # C bindings & RMT decoder for IR remote (_esp.ir)
│   ├── preferences.c       # C bindings for NVS key-value storage (_esp.preferences)
│   └── CMakeLists.txt      # Component build script & dependency paths
├── lua/                    # Lua root directory
│   └── esp32/              # ESP32 Atmos wrapper packages
│       ├── init.lua        # Module exports
│       ├── clock.lua       # Primary scheduler clock environment
│       ├── gpio.lua        # Secondary GPIO environment
│       ├── espnow.lua      # Secondary ESP-NOW environment
│       ├── ir.lua          # Secondary IR environment
│       ├── preferences.lua # NVS storage API wrapper
│       └── debug.lua       # Memory and FPS monitoring tasks
├── ext/                    # Git submodules
│   ├── atmos/              # Atmos compiler repository
│   ├── atmos-f-streams/    # Streams library repository
│   └── lua-atmos/          # Atmos Lua runtime repository
├── compile_lua.py          # Host tool cross-compiling Atmos/Lua to 32-bit bytecode
├── doc/
│   └── api.md              # Complete Atmos C/Lua API specification
├── partitions.csv          # Custom partition table (2MB App + 1.92MB LittleFS)
└── sdkconfig.defaults      # Pre-tuned memory and compiler configurations
```

---

## Build, Flash, and Monitor

### Prerequisites

* cmake
* python3
* ESP-IDF v6.x installed and exported.

### 1. Clone this repo with submodules

```bash
git clone --recurse-submodules https://github.com/pgvalle-research/atmos-esp32.git
```

If you already cloned without submodules, initialize them:
```bash
git submodule update --init --recursive
```

### 2. Build and Flash the ESP32

1. Connect the ESP32 board to your PC.
2. Run the build, flash, and serial monitor sequence:
   ```bash
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```
  *(Replace `/dev/ttyUSB0` with the actual device port of your ESP32).*

---

## License
MIT License. See [LICENSE](LICENSE) for details.
