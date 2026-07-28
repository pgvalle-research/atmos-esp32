# ESP32 Atmos Library API Documentation

This document describes the Atmos hardware wrapper libraries for the ESP32. All libraries are packaged under the `esp32` namespace.

You can import all modules at once:
```lua
local esp32 = require "esp32"
local clock, gpio, espnow, ir = esp32.clock, esp32.gpio, esp32.espnow, esp32.ir
```

---

## Table of Contents
* **[1. esp32.clock](#1-esp32clock)**
  * [`clock.now`](#clocknow)
* **[2. esp32.gpio](#2-esp32gpio)**
  * [`gpio.mode`](#gpiomode)
  * [`gpio.read`](#gpioread)
  * [`gpio.write`](#gpiowrite)
  * [`gpio.watch`](#gpiowatch)
  * [`gpio.unwatch`](#gpiounwatch)
  * [`gpio.read_adc`](#gpioread_adc)
  * [`gpio.write_pwm`](#gpiowrite_pwm)
  * [`gpio.fade_pwm`](#gpiofade_pwm)
* **[3. esp32.espnow](#3-esp32espnow)**
  * [`espnow.init`](#espnowinit)
  * [`espnow.get_mac`](#espnowget_mac)
  * [`espnow.add_peer`](#espnowadd_peer)
  * [`espnow.send`](#espnowsend)
* **[4. esp32.ir](#4-esp32ir)**
  * [`ir.start`](#irstart)
  * [`ir.stop`](#irstop)
  * [`ir.stats`](#irstats)
* **[5. Atmos Hardware Events](#5-atmos-hardware-events)**
  * [`gpio` Event](#gpio-event)
  * [`gpio_fade_pwm` Event](#gpio_fade_pwm-event)
  * [`espnow_recv` Event](#espnow_recv-event)
  * [`espnow_send` Event](#espnow_send-event)
  * [`ir` Event](#ir-event)
* **[6. Cross-Device Testing (ESP32 <-> ESP8266)](#6-cross-device-testing-esp32---esp8266)**
  * [Companion Sketch](#companion-sketch)
  * [Channel & Power Saving Considerations](#channel--power-saving-considerations)

---

## 1. `esp32.clock`
The clock module manages logical time and high-precision timing.

### `clock.now`
* **Syntax:** `clock.now` (field)
* **Returns:** `number` (Microseconds since microcontroller boot).
* **Example:**
  ```lua
  local start_time = clock.now
  -- do work
  local elapsed = clock.now - start_time
  print("Elapsed time in us: " .. elapsed)
  ```

---

## 2. `esp32.gpio`
The GPIO module handles all physical pin interactions including digital I/O, edge interrupts, ADC oneshot reads, and hardware-controlled PWM.

### `gpio.mode`
Configures a physical pin's routing and buffers.
* **Syntax:** `gpio.mode(pin, mode, [extra1, extra2, extra3])`
* **Arguments:**
  * `pin`: `integer` (GPIO pin number).
  * `mode`: `string`. Supported values:
    * `"input"`: Digital input.
    * `"input_pullup"`: Digital input with internal pull-up resistor.
    * `"input_pulldown"`: Digital input with internal pull-down resistor.
    * `"output"`: Digital push-pull output.
    * `"open_drain"`: Digital open-drain output.
    * `"pwm"`: Configure the pin for hardware PWM.
  * **PWM Mode Additional Arguments:**
    * `channel`: `integer` (LEDC channel `0` to `7`). Timers `0` to `3` are assigned automatically via `channel % 4`.
    * `frequency`: `integer` (Frequency in Hz).
    * `resolution`: `integer` (Duty resolution in bits, `1` to `20`).
* **Example:**
  ```lua
  -- Configure pin 22 as a normal output
  gpio.mode(22, "output")

  -- Configure pin 23 as a PWM output on Channel 0, at 5kHz with 13-bit resolution (0-8191 duty)
  gpio.mode(23, "pwm", 0, 5000, 13)
  ```

### `gpio.read`
Reads the digital state of a pin.
* **Syntax:** `gpio.read(pin)`
* **Returns:** `integer` (`0` for LOW, `1` for HIGH).
* **Example:**
  ```lua
  local state = gpio.read(4)
  if state == 1 then
      print("Pin is HIGH")
  end
  ```

### `gpio.write`
Sets the digital state of an output pin.
* **Syntax:** `gpio.write(pin, value)`
* **Arguments:**
  * `value`: `boolean` or `integer` (`true`/`1` sets HIGH, `false`/`0` sets LOW).
* **Example:**
  ```lua
  gpio.write(22, 1) -- High
  gpio.write(22, false) -- Low
  ```

### `gpio.watch`
Enables hardware interrupts on a pin. Edge transitions are queued and emitted as **[`gpio` events](#gpio-event)**.
* **Syntax:** `gpio.watch(pin, edge)`
* **Arguments:**
  * `edge`: `string` (`"rising"`, `"falling"`, or `"both"`/`"change"`).
* **Cross Reference:** Configures the hardware to trigger **[`gpio` events](#gpio-event)**.
* **Example:**
  ```lua
  gpio.mode(4, "input_pullup")
  gpio.watch(4, "falling")

  spawn(task(function()
      while true do
          local ev = await({ tag = "gpio", pin = 4, edge = "falling" })
          print("Button pressed at: " .. ev.time)
      end
  end))
  ```

### `gpio.unwatch`
Disables interrupts on the specified pin and detaches its ISR handler.
* **Syntax:** `gpio.unwatch(pin)`

### `gpio.read_adc`
Reads the raw analog voltage on a pin.
* **Syntax:** `gpio.read_adc(pin, [attenuation])`
* **Arguments:**
  * `attenuation`: `string` or `integer`. Configures internal voltage dividers to support different input ranges:
    * `"0db"` or `0`: $0\text{V}$ to $1.1\text{V}$ range.
    * `"2.5db"` or `2`: $0\text{V}$ to $1.25\text{V}$ range.
    * `"6db"` or `6`: $0\text{V}$ to $1.75\text{V}$ range.
    * `"12db"` / `"11db"` or `12` / `11`: $0\text{V}$ to $3.3\text{V}$ range (Default).
* **Returns:** `integer` (Raw reading from `0` to `4095` for 12-bit resolution).
* > [!WARNING]
  > **Wi-Fi vs. ADC2 Hardware Conflict Gotcha (ESP32 Limitation)**
  >
  > The ESP32's internal Wi-Fi hardware controller shares physical circuitry with the **ADC2** pins. When Wi-Fi (or ESP-NOW) is active, reading from any ADC2 pin will fail or return corrupt noise.
  >
  > * **DO NOT USE ADC2 PINS:** GPIO 0, 2, 4, 12, 13, 14, 15, 25, 26, 27.
  > * **ALWAYS USE ADC1 PINS:** **GPIO 32, 33, 34, 35, 36, 37, 38, 39**. (For example, `gpio.read_adc(34)` is perfectly safe and unaffected by ESP-NOW).
* **Example:**
  ```lua
  local raw = gpio.read_adc(34, "12db")
  local volts = (raw / 4095) * 3.3
  print("Sensor Voltage: " .. volts)
  ```

### `gpio.write_pwm`
Sets the static duty cycle on a configured PWM channel.
* **Syntax:** `gpio.write_pwm(channel, duty)`
* **Arguments:**
  * `duty`: `integer` (Value from `0` to $2^{\text{resolution}} - 1$).

### `gpio.fade_pwm`
Triggers a non-blocking hardware duty cycle fade over a duration of time. Once completed, a **[`gpio_fade_pwm` event](#gpio_fade_pwm-event)** is emitted.
* **Syntax:** `gpio.fade_pwm(channel, target_duty, duration_ms)`
* **Cross Reference:** Triggers a **[`gpio_fade_pwm` event](#gpio_fade_pwm-event)** upon completion.
* **Example:**
  ```lua
  -- Fade LED on Channel 0 to 100% (8191) over 1000ms, then fade back to 0% over 500ms
  spawn(task(function()
      while true do
          gpio.fade_pwm(0, 8191, 1000)
          await({ tag = "gpio_fade_pwm", channel = 0 })

          gpio.fade_pwm(0, 0, 500)
          await({ tag = "gpio_fade_pwm", channel = 0 })
      end
  end))
  ```

---

## 3. `esp32.espnow`
The ESP-NOW module enables low-latency connectionless packet communication over the Wi-Fi radio.

### `espnow.init`
Initializes the Wi-Fi Station stack and boots the ESP-NOW communication service.
* **Syntax:** `espnow.init()`
* **Under the hood configurations:**
  * **Power Saving:** Automatically disables Wi-Fi Power Saving (`WIFI_PS_NONE`). This keeps the radio receiver awake at all times to prevent dropped packets.
  * **Channel Lock:** Locks the physical Wi-Fi channel to **Channel 1** to ensure matching transmission frequencies with peer devices.
* **Cross Reference:** Allows incoming packets to trigger **[`espnow_recv` events](#espnow_recv-event)**.

### `espnow.get_mac`
Gets the local MAC address of the board.
* **Syntax:** `espnow.get_mac()`
* **Returns:** `string` (The formatted MAC address, e.g., `"24:0a:c4:05:06:07"`).

### `espnow.add_peer`
Registers a target peer device. This must be called before sending unicast messages to a peer.
* **Syntax:** `espnow.add_peer(mac_str)`
* **Arguments:**
  * `mac_str`: `string` (Standard MAC format, e.g. `"24:0a:c4:05:06:07"`).

### `espnow.send`
Sends an asynchronous message to the specified MAC address (unicast or broadcast). Does not wait for confirmation.
* **Syntax:** `espnow.send(mac_str, payload)`
* **Arguments:**
  * `mac_str`: `string` (Target peer MAC or `"ff:ff:ff:ff:ff:ff"` for broadcast).
  * `payload`: `string` (Message string up to 250 bytes).
* **Cross Reference:** Triggers an **[`espnow_send` event](#espnow_send-event)** upon packet delivery outcome (immediate for broadcast).
* **Reliability Check Observation:**
  This function is non-blocking and fires the packet asynchronously. If you need link-layer delivery confirmation (ensuring the receiver ACKed the packet), you should explicitly `await` the corresponding **[`espnow_send` event](#espnow_send-event)** matching the target MAC:
  ```lua
  espnow.send(peer_mac, "Hello")
  
  -- Suspend explicitly until transmission resolves
  local ev = await({ tag = "espnow_send", mac = peer_mac })
  if ev.status == "success" then
      print("Delivered successfully!")
  else
  end
  ```
* > [!NOTE]
  > **Transmission Status Semantics (Unicast vs. Broadcast)**
  >
  > * **Unicast (Specific MAC):** A subsequent `espnow_send` event returning `status = "success"` guarantees that the target device **successfully received** the packet and returned a hardware Link-layer ACK.
  > * **Broadcast (`"ff:ff:ff:ff:ff:ff"`):** A subsequent `espnow_send` event returning `status = "success"` **only** confirms the packet was successfully **transmitted into the air** by the local antenna. It does **not** indicate whether any remote device actually heard it.

---

## 4. `esp32.ir`

The IR module receives demodulated remote-control signals through the ESP32 RMT
peripheral. It currently decodes Sony SIRC and Samsung frames. The module emits
neutral frame data and does not assign application-specific meanings to commands.

### `ir.start`

Starts the single IR receiver.

* **Syntax:** `ir.start(pin, [options])`
* **Arguments:**
  * `pin`: GPIO connected to the IR receiver output.
  * `options.glitch_ns`: Pulses shorter than this are discarded. Default: `80000`.
  * `options.idle_ns`: A level lasting this long ends the frame. Default: `15000000`.

```lua
local ir = require("esp32").ir
ir.start(15)
```

### `ir.stop`

Stops the receiver and releases its RMT channel, task, and queues.

* **Syntax:** `ir.stop()`

### `ir.stats`

Returns diagnostic counters:

```lua
{
    captured = integer,
    decoded = integer,
    invalid = integer,
    dropped = integer
}
```

`invalid` counts captured waveforms that were not valid supported frames.
`dropped` counts frames lost because an internal queue was full.

---

## 5. Atmos Hardware Events

Secondary environments process physical interrupts and emit them as Atmos events. You can capture them in tasks using the `await()` pattern.

### `gpio` Event
* **Created/Configured by:** [`gpio.watch`](#gpiowatch)
* **Structure:**
  ```lua
  {
      tag = "gpio",
      pin = integer,       -- GPIO pin number
      value = integer,     -- Pin state: 0 (LOW) or 1 (HIGH)
      edge = string,       -- "rising" or "falling"
      time = integer       -- Microseconds since boot of transition
  }
  ```

### `gpio_fade_pwm` Event
* **Created/Triggered by:** [`gpio.fade_pwm`](#gpiofade_pwm)
* **Structure:**
  ```lua
  {
      tag = "gpio_fade_pwm",
      channel = integer,   -- PWM channel number (0 to 7)
      time = integer       -- Microseconds since boot when fade completed
  }
  ```

### `espnow_recv` Event
* **Allowed/Enabled by:** [`espnow.init`](#espnowinit)
* **Structure:**
  ```lua
  {
      tag = "espnow_recv",
      mac = string,        -- Sender's MAC address (e.g. "24:0a:c4:05:06:07")
      data = string,       -- Received payload data (string)
      time = integer       -- Microseconds since boot when received
  }
  ```

### `espnow_send` Event
* **Created/Triggered by:** [`espnow.send`](#espnowsend)
* **Structure:**
  ```lua
  {
      tag = "espnow_send",
      mac = string,        -- Target peer's MAC address
      status = string,     -- "success" or "fail"
      time = integer       -- Microseconds since boot when result resolved
  }
  ```
* > [!NOTE]
  > **Transmission Status Semantics (Unicast vs. Broadcast)**
  >
  > * **Unicast (Specific MAC):** A `status = "success"` outcome guarantees that the target device **successfully received** the packet and returned a hardware Link-layer ACK.
  > * **Broadcast (`"ff:ff:ff:ff:ff:ff"`):** A `status = "success"` outcome **only** confirms the packet was successfully **transmitted into the air** by the local antenna. It does **not** indicate whether any remote device actually heard it.

---

### `ir` Event

Emitted once for each valid decoded IR frame:

```lua
{
    tag = "ir",
    protocol = "sony", -- or "samsung"
    address = integer,
    command = integer,
    raw = "0xAABBCCDD",
    bits = integer,
    repeat = boolean,
    time = integer
}
```

`raw` is a hexadecimal string because this firmware configures Lua with
32-bit integers. The event describes only the received frame; button mappings
and state machines belong to application code.

```lua
spawn(task(function()
    while true do
        local ev = await({ tag = "ir" })
        print(ev.protocol, ev.address, ev.command, ev.repeat)
    end
end))
```

### Compatibility and current limitations

The event is intentionally comparable to Arduino-IRremote's
`IrReceiver.decodedIRData`: it reports a decoded protocol, address, command,
raw value, bit count, and repeat status. It is not a complete replacement for
Arduino-IRremote yet:

* Only Sony SIRC (12, 15, and 20 bit) and Samsung32 are decoded.
* Samsung's ordinary 8-bit command layout is supported. A valid Samsung
  16-bit command is currently reduced to its first 8 bits.
* A duplicated Samsung 8-bit address is returned as the complete 16-bit wire
  value. Arduino-IRremote normally collapses equal address bytes to one byte.
* The SamsungLG short repeat format used by current Arduino-IRremote is not
  decoded yet. Full repeated Samsung frames are decoded normally.
* Repeat detection for complete frames compares equal raw frames received
  within 120 ms. Arduino-IRremote instead uses protocol-specific preceding-gap
  limits by default.
* The default 15 ms RMT idle threshold can merge repeated Sony20 frames whose
  inter-frame gap is shorter than 15 ms. Single frames and typical Sony12
  repetitions are unaffected.
* Samsung inverse-byte parity is not currently reported or rejected.
* Unknown protocols and malformed frames do not emit events; they increment
  `ir.stats().invalid`.
* Only one receiver can be active. Decoded events use a fixed queue of 16;
  queue overflow increments `ir.stats().dropped`.
* `raw` is a hexadecimal string, rather than an integer, to preserve the value
  in the firmware's 32-bit Lua configuration.

These differences do not affect the current BMJ logic: BMJ reads only the Sony
or Samsung protocol and ordinary 8-bit command field. It does not use address,
raw data (except for logging), bit count, parity, or repeat metadata.

---

## 6. Cross-Device Testing (ESP32 <-> ESP8266)

The project repository includes a compatible ESP8266 Arduino C++ firmware folder to facilitate testing two-way communication between Atmos devices and standard Arduino platforms.

### Companion Sketch
Located at [esp8266_pong/esp8266_pong.ino](file:///home/pedro/Projects/idf-extra-components/lua/examples/lua_example/esp8266_pong/esp8266_pong.ino), the sketch implements a responsive ping-pong node:
* It listens on Wi-Fi channel 1.
* On receiving `"PING"`, it schedules a reply of `"PONG"` to the configured ESP32 MAC address.

### Channel & Power Saving Considerations
For ESP-NOW to function reliably:
1. **Match Wi-Fi Channels:** Ensure both devices are explicitly set to the same Wi-Fi channel (Channel 1 is standard).
2. **Disable Power Saving:** The receiver device must have Wi-Fi Power Management turned off (`esp_wifi_set_ps(WIFI_PS_NONE)` in ESP-IDF). Otherwise, the radio receiver goes to sleep periodically and will drop incoming frames.
3. **Defer Transmission in Callbacks:** On ESP8266 Arduino, calling `esp_now_send` directly from inside the `onDataRecv` callback context is unstable. The companion sketch avoids this by setting a volatile flag and executing the transmission within the main `loop()`.
