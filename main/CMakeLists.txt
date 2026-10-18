idf_component_register(SRCS "main.c" "gpio.c" "espnow.c"
                         "ir/ir.c" "ir/decoder.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer esp_driver_gpio esp_driver_ledc esp_driver_rmt esp_adc esp_wifi esp_event nvs_flash)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LUA_32BITS=1)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
  set(HOST_LUA_32 "${CMAKE_BINARY_DIR}/lua5.5")
  set(LUA_COMPILED_DIR "${CMAKE_BINARY_DIR}/lua_compiled")
  set(ATMOS_LUA_DIR "${CMAKE_SOURCE_DIR}/ext/lua-atmos/atmos")
  set(F_STREAMS_LUA_DIR "${CMAKE_SOURCE_DIR}/ext/atmos-f-streams/streams")

  # Rule to compile host lua_32 tool
  add_custom_command(
        OUTPUT "${HOST_LUA_32}"
        COMMAND ${CMAKE_COMMAND} -E env "PATH=/usr/bin:/bin:$ENV{PATH}"
                gcc -B/usr/bin/ -O2 -DLUA_USE_LINUX -DLUA_32BITS -DMAKE_LUA -Wl,-E
                "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__lua/lua/onelua.c"
                -lm -ldl -o "${HOST_LUA_32}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__lua/lua/onelua.c"
        COMMENT "Building 32-bit host Lua interpreter for cross-compiling bytecode"
    )

  # Target to compile Lua scripts to 32-bit bytecode
  add_custom_target(compile_lua_bytecode
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}:$ENV{PATH}"
                python "${CMAKE_CURRENT_SOURCE_DIR}/../compile_lua.py"
                "${HOST_LUA_32}"
                "${LUA_COMPILED_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../lua"
                "atmos=${ATMOS_LUA_DIR}"
                "streams=${F_STREAMS_LUA_DIR}"
                "esp32=${CMAKE_CURRENT_SOURCE_DIR}/../lua/esp32"
        DEPENDS "${HOST_LUA_32}"
        COMMENT "Compiling Lua source files to 32-bit bytecode"
    )

  # Note: you must have a partition named "lua" in your partition table
  littlefs_create_partition_image(lua "${LUA_COMPILED_DIR}" FLASH_IN_PROJECT DEPENDS compile_lua_bytecode)
endif()
