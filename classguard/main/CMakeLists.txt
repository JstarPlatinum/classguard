idf_component_register(
    SRCS
        "main.c"
        "../test/sensor_serial_tests/sht35_serial_test.c"
        "../test/sensor_serial_tests/scd41_serial_test.c"
        "../test/sensor_serial_tests/env_i2c_serial_test.c"
    INCLUDE_DIRS
        "."
        "../test/sensor_serial_tests"
    REQUIRES drivers common
)
