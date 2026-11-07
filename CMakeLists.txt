cmake_minimum_required(VERSION 3.16)

# Suppress upstream ESP-IDF v6.1 internal private include directory warnings between esp_wifi and wpa_supplicant
function(message)
    list(GET ARGV 0 type)
    if(type STREQUAL "WARNING")
        list(GET ARGV 1 text)
        if(text MATCHES "belongs to component (esp_wifi|wpa_supplicant)")
            return()
        endif()
    endif()
    _message(${ARGV})
endfunction()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lua_atmos_esp32)
