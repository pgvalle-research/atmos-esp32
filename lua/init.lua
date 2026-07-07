local esp32 = require "esp32"
local clock, gpio, espnow = esp32.clock, esp32.gpio, esp32.espnow

-- =========================================================================
-- CONFIGURATION: Set the MAC Address of your ESP8266 NodeMCU here
-- =========================================================================
local esp8266_mac = "4c:eb:d6:1f:f3:3d" -- Actual ESP8266 NodeMCU MAC from flash logs
local LED_PIN = 23                      -- Pin to blink on receiving PONG

loop(function()
    -- Initialize Pin
    gpio.mode(LED_PIN, "output")
    gpio.write(LED_PIN, false)

    -- Initialize ESP-NOW
    espnow.init()
    local my_mac = espnow.get_mac()
    print("======================================================")
    print("ESP32 ATMOS RUNNING")
    print("My MAC Address: " .. my_mac)
    print("Target ESP8266 MAC: " .. esp8266_mac)
    print("======================================================")

    -- Register the peer
    espnow.add_peer(esp8266_mac)

    -- Task 1: Periodic Sender (Sends PING to ESP8266 every 2 seconds)
    spawn(task(function()
        while true do
            print(string.format("[%0.3f] Sending PING...", clock.now / _s_))
            local acked = espnow.send_reliable(esp8266_mac, "PING")
            if acked then
                print("   -> PING delivered successfully (ACK received)!")
            else
                print("   -> PING delivery failed (No ACK from ESP8266)")
            end
            await(2 * _s_)
        end
    end))

    -- Task 2: Receiver (Listens for PONG from ESP8266)
    spawn(task(function()
        while true do
            -- Suspend until a packet arrives specifically from our ESP8266
            local ev = await({ tag = "espnow_recv", mac = esp8266_mac })
            print(string.format("[%0.3f] Received from %s: '%s'", ev.time / _s_, ev.mac, ev.data))

            if ev.data == "PONG" then
                -- Blink the LED
                gpio.write(LED_PIN, true)
                await(100 * _ms_)
                gpio.write(LED_PIN, false)
            end
        end
    end))

    -- Keep main thread alive
    await(10000000 * _s_)
end)
