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

* **Hardware Drivers:**
  * **GPIO:** Digital input/output and edge-triggered interrupts (`gpio.watch`).
  * **PWM / Fade:** Hardware LEDC-based pulse-width modulation and smooth duty cycle fading (`gpio.fade_pwm`).
  * **ADC:** High-speed analog reads (`gpio.read_adc`) with adjustable voltage attenuation.
  * **ESP-NOW:** Low-latency connectionless peer-to-peer wireless communication (`espnow.send`).
* **Cross-Device Testing:** Includes a companion Arduino ESP8266 sketch under `esp8266_pong/` for instant bidirectional wireless ping-pong testing.
* **Resource Optimization:** Selective library loading (unloads unneeded standard libraries like `io` and `utf8`) and custom low-latency buffer queues to preserve internal SRAM.

---

## Project Structure

```text
├── main/                   # ESP-IDF C application source code
│   ├── main.c              # NVS, Filesystem, and Lua VM startup
│   ├── gpio.c              # C bindings for GPIO, PWM, and ADC
│   ├── espnow.c            # C bindings for ESP-NOW WiFi radio driver
│   └── CMakeLists.txt      # Component build script & dependency paths
├── lua/                    # Lua root directory (compiles into LittleFS image)
│   ├── init.lua            # Main application script & tasks
│   └── esp32/              # ESP32 Atmos wrapper packages
│       ├── init.lua        # Module exports
│       ├── clock.lua       # Primary scheduler clock environment
│       ├── gpio.lua        # Secondary GPIO environment
│       └── espnow.lua      # Secondary ESP-NOW environment
├── esp8266_pong/           # Companion C++ Arduino sketch for cross-platform testing
├── doc/
│   └── api.md              # Complete Atmos C/Lua API specification
├── partitions.csv          # Custom partition table (App + LittleFS storage)
└── sdkconfig.defaults      # Pre-tuned memory and compiler configurations
```

---

## Build, Flash, and Monitor

### Prerequisites

* cmake
* python3
* ESP-IDF v6.x installed and exported.

### 1. Clone this repo

```bash
git clone https://github.com/pgvalle/lua-atmos-esp32.git
```

### 2. Clone the Dependency Ports

```bash
# Clone the Atmos runtime fork (Checkout the v0.7-lua5.5 branch)
git clone -b v0.7-lua5.5 https://github.com/pgvalle/atmos.git /whatever/path/you/choose

# Clone the f-streams fork (Checkout the v0.2-lua5.5 branch)
git clone -b v0.2-lua5.5 https://github.com/pgvalle/f-streams.git /whatever/path/you/choose
```

### 3. Configure Paths in CMake

Open **[main/CMakeLists.txt](main/CMakeLists.txt#L9-L11)** and set the variables to the absolute paths where you cloned the repositories:

```cmake
set(ATMOS_LUA_DIR "/path/to/your/cloned/atmos/atmos")
set(F_STREAMS_LUA_DIR "/path/to/your/cloned/f-streams/streams")
```

### 4. Build and Flash the ESP32

1. Connect the ESP32 board to your PC.
2. Run the build, flash, and serial monitor sequence:
   ```bash
   idf.py -p /dev/ttyUSB0 build flash monitor
   ```
  *(Replace `/dev/ttyUSB0` with the actual device port of your ESP32).*

---

## License
MIT License. See [LICENSE](LICENSE) for details.
