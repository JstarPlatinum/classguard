idf_component_register(
    SRCS
        "main.c"
        "../test/sensor_v1_data_test/env_i2c_v1_data_test.c"
    INCLUDE_DIRS
        "."
        "../test/sensor_v1_data_test"
    REQUIRES drivers common
)
