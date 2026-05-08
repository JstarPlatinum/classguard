idf_component_register(
    SRCS
        "main.c"
        "sht35_serial_test.c"
    INCLUDE_DIRS "."
    REQUIRES drivers common
)
