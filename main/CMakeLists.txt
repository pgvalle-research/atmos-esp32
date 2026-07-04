idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ".")

# Note: you must have a partition named "assets" in your partition table
littlefs_create_partition_image(assets ../assets FLASH_IN_PROJECT)
