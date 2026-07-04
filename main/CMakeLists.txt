idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_timer)

# Note: you must have a partition named "lua" in your partition table
littlefs_create_partition_image(lua ../lua FLASH_IN_PROJECT)
