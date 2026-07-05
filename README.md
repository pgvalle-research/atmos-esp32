# Lua Atmos ESP32

This project runs a Lua 5.5 Atmos environment on ESP32 using Espressif's official ESP-IDF framework.

Lua sources are stored in a LittleFS partition named `lua` and mounted at `/lua`. The firmware loads `/lua/init.lua`, configures `package.path` for modules under `/lua`, and runs the Lua app inside a yieldable Lua coroutine.

## Features

- Lua 5.5 runtime through the `espressif/lua` component
- Atmos runtime files stored in LittleFS
- `/lua/init.lua` as the hardcoded Lua entrypoint
- ESP timer wakeup bridge using `wait_clock()`
- C/Lua yield-resume loop so FreeRTOS regains control between clock steps
- Deadline-based Atmos `loop_on` support in the bundled Atmos runtime

## Layout

```text
main/          ESP-IDF application host
lua/           Lua filesystem image contents
lua/init.lua   application entrypoint
partitions.csv custom partition table with the lua LittleFS partition
```

## Requirements

- ESP32 board with at least 4 MB flash
- ESP-IDF 6.x
- ESP-IDF component manager access for:
  - `espressif/lua`
  - `joltwallet/littlefs`

ESP-IDF documentation: https://docs.espressif.com/projects/esp-idf/

## Build And Flash

```bash
idf.py build flash monitor
```

If needed, specify the serial port:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Lua Entrypoint

The firmware loads:

```text
/lua/init.lua
```

Lua modules are resolved through:

```text
/lua/?.lua
/lua/?/init.lua
```

For example:

```lua
local clock = require "atmos.env.clock_esp"
```

loads:

```text
/lua/atmos/env/clock_esp.lua
```

## Timing Model

The ESP clock environment calls `wait_clock()`, which yields out of Lua to C. C waits for an ESP timer notification, then resumes the same Lua coroutine. This keeps the Lua state intact while letting FreeRTOS run between clock steps.

The current ESP clock environment uses `os.clock()` normalized to microseconds to match the desktop Atmos timing model more closely.

## License

MIT License. See [LICENSE](LICENSE).
