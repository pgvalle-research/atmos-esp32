idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(HOST_LUA_32 "${CMAKE_BINARY_DIR}/lua_32")
    set(LUA_COMPILED_DIR "${CMAKE_BINARY_DIR}/lua_compiled")

    # Rule to compile host lua_32 tool
    add_custom_command(
        OUTPUT "${HOST_LUA_32}"
        COMMAND gcc -O2 -DLUA_USE_LINUX -DLUA_32BITS -DMAKE_LUA -Wl,-E 
                "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__lua/lua/onelua.c" 
                -lm -ldl -o "${HOST_LUA_32}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/espressif__lua/lua/onelua.c"
        COMMENT "Building 32-bit host Lua interpreter for cross-compiling bytecode"
    )

    # Target to compile Lua scripts to 32-bit bytecode
    add_custom_target(compile_lua_bytecode
        COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/../compile_lua.py"
                "${HOST_LUA_32}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../lua"
                "${LUA_COMPILED_DIR}"
        DEPENDS "${HOST_LUA_32}"
        COMMENT "Compiling Lua source files to 32-bit bytecode"
    )

    # Note: you must have a partition named "lua" in your partition table
    littlefs_create_partition_image(lua "${LUA_COMPILED_DIR}" FLASH_IN_PROJECT DEPENDS compile_lua_bytecode)
endif()
