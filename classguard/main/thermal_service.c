#include "thermal_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "app_config.h"
#include "app_data.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mlx90640.h"
#include "occupancy_detector.h"
#include "pin_map.h"

#define THERMAL_TASK_STACK_SIZE 24576U
#define THERMAL_TASK_PRIORITY 5U
#define THERMAL_INIT_RETRY_DELAY_MS 2000U
#define THERMAL_UART_WRITE_CHUNK 128U
#define THERMAL_UART_DRAIN_TIMEOUT_MS 1000U
#define THERMAL_MAGIC_0 'C'
#define THERMAL_MAGIC_1 'G'
#define THERMAL_MAGIC_2 'T'
#define THERMAL_MAGIC_3 'H'
#define THERMAL_PROTOCOL_VERSION 1U
#define THERMAL_FRAME_TYPE_FLOAT32 1U
#define THERMAL_HEADER_LENGTH 28U
#define THERMAL_PAYLOAD_LENGTH (CG_MLX90640_PIXEL_COUNT * sizeof(float))

static uint32_t thermal_crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    crc = ~crc;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)(-(int32_t)(crc & 1U)));
        }
    }
    return ~crc;
}

static void thermal_put_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static void thermal_put_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static esp_err_t thermal_uart_init(void)
{
    esp_log_level_set("*", ESP_LOG_NONE);
    (void)uart_wait_tx_done(CG_UART_THERMAL_TEST_PORT, pdMS_TO_TICKS(200));

    const uart_config_t uart_config = {
        .baud_rate = CG_THERMAL_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(CG_UART_THERMAL_TEST_PORT, &uart_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = uart_set_pin(CG_UART_THERMAL_TEST_PORT,
                       CG_PIN_THERMAL_TEST_TX,
                       CG_PIN_THERMAL_TEST_RX,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = uart_driver_install(CG_UART_THERMAL_TEST_PORT,
                              CG_THERMAL_UART_RX_BUFFER_SIZE,
                              CG_THERMAL_UART_TX_BUFFER_SIZE,
                              0,
                              NULL,
                              0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    return uart_set_baudrate(CG_UART_THERMAL_TEST_PORT, CG_THERMAL_UART_BAUD_RATE);
}

static bool thermal_uart_write_all(const uint8_t *data, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        size_t chunk = length - offset;
        if (chunk > THERMAL_UART_WRITE_CHUNK) {
            chunk = THERMAL_UART_WRITE_CHUNK;
        }

        int written = uart_write_bytes(CG_UART_THERMAL_TEST_PORT, (const char *)(data + offset), chunk);
        if (written <= 0) {
            return false;
        }

        offset += (size_t)written;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

static bool thermal_uart_send_frame(const thermal_frame_t *frame, uint32_t sequence)
{
    uint8_t header[THERMAL_HEADER_LENGTH];
    const uint8_t *payload = (const uint8_t *)frame->pixels;
    const uint32_t payload_crc = thermal_crc32_update(0, payload, THERMAL_PAYLOAD_LENGTH);

    header[0] = THERMAL_MAGIC_0;
    header[1] = THERMAL_MAGIC_1;
    header[2] = THERMAL_MAGIC_2;
    header[3] = THERMAL_MAGIC_3;
    header[4] = THERMAL_PROTOCOL_VERSION;
    header[5] = THERMAL_FRAME_TYPE_FLOAT32;
    thermal_put_le16(&header[6], THERMAL_HEADER_LENGTH);
    thermal_put_le32(&header[8], sequence);
    thermal_put_le32(&header[12], frame->timestamp_ms);
    thermal_put_le16(&header[16], CG_MLX90640_WIDTH);
    thermal_put_le16(&header[18], CG_MLX90640_HEIGHT);
    thermal_put_le32(&header[20], THERMAL_PAYLOAD_LENGTH);
    thermal_put_le32(&header[24], payload_crc);

    if (!thermal_uart_write_all(header, sizeof(header))) {
        return false;
    }
    if (!thermal_uart_write_all(payload, THERMAL_PAYLOAD_LENGTH)) {
        return false;
    }

    return uart_wait_tx_done(CG_UART_THERMAL_TEST_PORT, pdMS_TO_TICKS(THERMAL_UART_DRAIN_TIMEOUT_MS)) == ESP_OK;
}

static void thermal_occupancy_task(void *arg)
{
    (void)arg;

    static cg_mlx90640_t mlx;
    static cg_occupancy_detector_t detector;
    bool ready = false;
    bool uart_ready = false;
    uint32_t thermal_sequence = 0;

    cg_occupancy_detector_init(&detector);

    for (;;) {
        if (!uart_ready) {
            uart_ready = thermal_uart_init() == ESP_OK;
        }

        if (!ready) {
            esp_err_t ret = cg_mlx90640_init(&mlx, CG_I2C_MLX_PORT);
            if (ret != ESP_OK) {
                cg_app_data_set_sensor_error(CG_SENSOR_STATUS_MLX90640, "mlx90640_init", ret);
                vTaskDelay(pdMS_TO_TICKS(THERMAL_INIT_RETRY_DELAY_MS));
                continue;
            }

            cg_occupancy_detector_init(&detector);
            cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_MLX90640);
            ready = true;
        }

        static thermal_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        esp_err_t ret = cg_mlx90640_read_thermal_frame(&mlx, &frame);
        if (ret != ESP_OK || !frame.valid) {
            cg_app_data_set_sensor_error(CG_SENSOR_STATUS_MLX90640, "mlx90640_read", ret);
            ready = false;
            vTaskDelay(pdMS_TO_TICKS(THERMAL_INIT_RETRY_DELAY_MS));
            continue;
        }

        cg_app_data_update_thermal(&frame);
        if (uart_ready) {
            if (!thermal_uart_send_frame(&frame, thermal_sequence++)) {
                uart_ready = false;
            }
        }

        static occupancy_frame_t occupancy;
        memset(&occupancy, 0, sizeof(occupancy));
        ret = cg_occupancy_detector_update(&detector, &frame, &occupancy);
        if (ret == ESP_OK) {
            if (occupancy.valid) {
                cg_app_data_update_occupancy(&occupancy);
            }
            cg_app_data_clear_sensor_error(CG_SENSOR_STATUS_MLX90640);
        } else {
            cg_app_data_set_sensor_error(CG_SENSOR_STATUS_MLX90640, "occupancy_update", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(CG_MLX90640_DEFAULT_PERIOD_MS));
    }
}

esp_err_t cg_thermal_service_start(void)
{
    BaseType_t ok = xTaskCreate(thermal_occupancy_task,
                                "thermal_occupancy",
                                THERMAL_TASK_STACK_SIZE,
                                NULL,
                                THERMAL_TASK_PRIORITY,
                                NULL);

    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
