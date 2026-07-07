# ESP32 Atmos Library API Documentation

This document describes the Atmos hardware wrapper libraries for the ESP32. All libraries are packaged under the `esp32` namespace.

You can import all modules at once:
```lua
local esp32 = require "esp32"
local clock, gpio, espnow = esp32.clock, esp32.gpio, esp32.espnow
```

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
Triggers a non-blocking hardware duty cycle fade over a duration of time. Once completed, a **[`pwm` event](#pwm-event)** is emitted.
* **Syntax:** `gpio.fade_pwm(channel, target_duty, duration_ms)`
* **Cross Reference:** Triggers a **[`pwm` event](#pwm-event)** upon completion.
* **Example:**
  ```lua
  -- Fade LED on Channel 0 to 100% (8191) over 1000ms, then fade back to 0% over 500ms
  spawn(task(function()
      while true do
          gpio.fade_pwm(0, 8191, 1000)
          await({ tag = "pwm", channel = 0 })

          gpio.fade_pwm(0, 0, 500)
          await({ tag = "pwm", channel = 0 })
      end
  end))
  ```

---

## 3. `esp32.espnow`
The ESP-NOW module enables low-latency connectionless packet communication over the Wi-Fi radio.

### `espnow.init`
Initializes the Wi-Fi Station stack and boots the ESP-NOW communication service.
* **Syntax:** `espnow.init()`
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

### `espnow.send_reliable`
Sends a message and suspends the calling coroutine until the target peer acknowledges (ACKs) receipt.
* **Syntax:** `espnow.send_reliable(mac_str, payload)`
* **Returns:** `boolean`, `string`
  1. `success`: `true` if delivered successfully, `false` if delivery failed.
  2. `status`: `"success"` or `"fail"`.
* **Cross Reference:** Awaits the **[`espnow_send` event](#espnow_send-event)** matching the target MAC.
* **Example:**
  ```lua
  local success, status = espnow.send_reliable("24:0a:c4:05:06:07", "Ping")
  if success then
      print("Ping delivered!")
  else
      print("Delivery failed: " .. status)
  end
  ```

---

## 4. Atmos Hardware Events

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

### `pwm` Event
* **Created/Triggered by:** [`gpio.fade_pwm`](#gpiofade_pwm)
* **Structure:**
  ```lua
  {
      tag = "pwm",
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
* **Created/Triggered by:** [`espnow.send`](#espnowsend) and [`espnow.send_reliable`](#espnowsend_reliable)
* **Structure:**
  ```lua
  {
      tag = "espnow_send",
      mac = string,        -- Target peer's MAC address
      status = string,     -- Transmission result: "success" or "fail"
      time = integer       -- Microseconds since boot when result resolved
  }
  ```
