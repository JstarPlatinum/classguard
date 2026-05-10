#include "mlx90640_data_test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "app_config.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mlx90640.h"
#include "occupancy_detector.h"
#include "pin_map.h"

#define THERMAL_MAGIC_0 'C'
#define THERMAL_MAGIC_1 'G'
#define THERMAL_MAGIC_2 'T'
#define THERMAL_MAGIC_3 'H'
#define THERMAL_PROTOCOL_VERSION 1U
#define THERMAL_FRAME_TYPE_FLOAT32 1U
#define THERMAL_HEADER_LENGTH 28U
#define THERMAL_PAYLOAD_LENGTH (CG_MLX90640_PIXEL_COUNT * sizeof(float))
#define MLX90640_RETRY_DELAY_MS 2000U
#define MLX90640_TEST_TASK_STACK_SIZE 16384U
#define MLX90640_TEST_TASK_PRIORITY 5U
#define THERMAL_UART_WRITE_CHUNK 128U
#define THERMAL_UART_DRAIN_TIMEOUT_MS 3000U

typedef enum {
    THERMAL_SEND_OK = 0,
    THERMAL_SEND_HEADER_FAILED,
    THERMAL_SEND_PAYLOAD_FAILED,
    THERMAL_SEND_DRAIN_TIMEOUT,
} thermal_send_status_t;

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
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

static void put_le16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static esp_err_t thermal_uart_init(void)
{
    fflush(stdout);
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

static bool thermal_uart_write_all(const uint8_t *data, size_t length, size_t *written_total)
{
    size_t offset = 0;
    while (offset < length) {
        size_t chunk = length - offset;
        if (chunk > THERMAL_UART_WRITE_CHUNK) {
            chunk = THERMAL_UART_WRITE_CHUNK;
        }

        int written = uart_write_bytes(CG_UART_THERMAL_TEST_PORT, (const char *)(data + offset), chunk);
        if (written <= 0) {
            if (written_total != NULL) {
                *written_total = offset;
            }
            return false;
        }

        offset += (size_t)written;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (written_total != NULL) {
        *written_total = offset;
    }
    return true;
}

static thermal_send_status_t thermal_uart_send_frame(const thermal_frame_t *frame, uint32_t sequence, size_t *written_total)
{
    uint8_t header[THERMAL_HEADER_LENGTH];
    const uint8_t *payload = (const uint8_t *)frame->pixels;
    const uint32_t payload_crc = crc32_update(0, payload, THERMAL_PAYLOAD_LENGTH);

    header[0] = THERMAL_MAGIC_0;
    header[1] = THERMAL_MAGIC_1;
    header[2] = THERMAL_MAGIC_2;
    header[3] = THERMAL_MAGIC_3;
    header[4] = THERMAL_PROTOCOL_VERSION;
    header[5] = THERMAL_FRAME_TYPE_FLOAT32;
    put_le16(&header[6], THERMAL_HEADER_LENGTH);
    put_le32(&header[8], sequence);
    put_le32(&header[12], frame->timestamp_ms);
    put_le16(&header[16], CG_MLX90640_WIDTH);
    put_le16(&header[18], CG_MLX90640_HEIGHT);
    put_le32(&header[20], THERMAL_PAYLOAD_LENGTH);
    put_le32(&header[24], payload_crc);

    size_t header_written = 0;
    if (!thermal_uart_write_all(header, sizeof(header), &header_written)) {
        if (written_total != NULL) {
            *written_total = header_written;
        }
        return THERMAL_SEND_HEADER_FAILED;
    }

    size_t payload_written = 0;
    if (!thermal_uart_write_all(payload, THERMAL_PAYLOAD_LENGTH, &payload_written)) {
        if (written_total != NULL) {
            *written_total = header_written + payload_written;
        }
        return THERMAL_SEND_PAYLOAD_FAILED;
    }

    if (written_total != NULL) {
        *written_total = header_written + payload_written;
    }

    return (uart_wait_tx_done(CG_UART_THERMAL_TEST_PORT, pdMS_TO_TICKS(THERMAL_UART_DRAIN_TIMEOUT_MS)) == ESP_OK)
               ? THERMAL_SEND_OK
               : THERMAL_SEND_DRAIN_TIMEOUT;
}

static const char *thermal_send_status_name(thermal_send_status_t status)
{
    switch (status) {
    case THERMAL_SEND_OK:
        return "ok";
    case THERMAL_SEND_HEADER_FAILED:
        return "header_failed";
    case THERMAL_SEND_PAYLOAD_FAILED:
        return "payload_failed";
    case THERMAL_SEND_DRAIN_TIMEOUT:
        return "drain_timeout";
    default:
        return "unknown";
    }
}

static const char *occupancy_state_name(cg_occupancy_state_t state)
{
    switch (state) {
    case CG_OCCUPANCY_UNOCCUPIED:
        return "unoccupied";
    case CG_OCCUPANCY_POSSIBLE_OCCUPIED:
        return "possible";
    case CG_OCCUPANCY_OCCUPIED:
        return "occupied";
    default:
        return "unknown";
    }
}

void mlx90640_data_test_run(void)
{
    esp_log_level_set("*", ESP_LOG_NONE);
    printf("MLX90640 %s test task started at console baud 115200; switching UART0 to %u baud.\r\n",
           CG_MLX90640_TEST_VERSION,
           CG_THERMAL_UART_BAUD_RATE);

    esp_err_t ret = thermal_uart_init();
    if (ret != ESP_OK) {
        printf("MLX90640 test UART init failed: %s\r\n", esp_err_to_name(ret));
        return;
    }
    printf("MLX90640 %s test UART ready: port=%d tx=%d rx=%d baud=%u\r\n",
           CG_MLX90640_TEST_VERSION,
           CG_UART_THERMAL_TEST_PORT,
           CG_PIN_THERMAL_TEST_TX,
           CG_PIN_THERMAL_TEST_RX,
           CG_THERMAL_UART_BAUD_RATE);

    static cg_mlx90640_t mlx;
    static cg_occupancy_detector_t occupancy_detector;
    while ((ret = cg_mlx90640_init(&mlx, CG_I2C_MLX_PORT)) != ESP_OK) {
        printf("MLX90640 init failed: %s, retry in %u ms\r\n", esp_err_to_name(ret), MLX90640_RETRY_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(MLX90640_RETRY_DELAY_MS));
    }
    cg_occupancy_detector_init(&occupancy_detector);
    printf("MLX90640 init ok: i2c_port=%d addr=0x%02X period=%u ms\r\n",
           CG_I2C_MLX_PORT,
           CG_ADDR_MLX90640,
           CG_MLX90640_DEFAULT_PERIOD_MS);

    uint32_t sequence = 0;
    for (;;) {
        static thermal_frame_t frame;
        int64_t loop_start_us = esp_timer_get_time();
        memset(&frame, 0, sizeof(frame));
        ret = cg_mlx90640_read_thermal_frame(&mlx, &frame);
        if (ret == ESP_OK && frame.valid) {
            occupancy_frame_t occupancy = {0};
            esp_err_t occ_ret = cg_occupancy_detector_update(&occupancy_detector, &frame, &occupancy);
            uint32_t sent_sequence = sequence++;
            size_t written_total = 0;
            thermal_send_status_t send_status = thermal_uart_send_frame(&frame, sent_sequence, &written_total);
            printf("MLX90640 frame %s: seq=%lu ts=%lu bytes=%u/%u min=%.2f max=%.2f avg=%.2f hotspot=%u\r\n",
                   thermal_send_status_name(send_status),
                   (unsigned long)sent_sequence,
                   (unsigned long)frame.timestamp_ms,
                   (unsigned int)written_total,
                   (unsigned int)(THERMAL_HEADER_LENGTH + THERMAL_PAYLOAD_LENGTH),
                   frame.min_temp_c,
                   frame.max_temp_c,
                   frame.avg_temp_c,
                   frame.hotspot_index);
            if (occ_ret == ESP_OK && occupancy.valid) {
                printf("MLX90640 occupancy: state=%s occupied=%d ratio=%.4f score=%.4f threshold=%.2f max_delta=%.2f pixels=%u region=%u\r\n",
                       occupancy_state_name(occupancy.state),
                       occupancy.occupied ? 1 : 0,
                       occupancy.occupancy_ratio,
                       occupancy.occupancy_score,
                       occupancy.threshold,
                       occupancy.max_delta,
                       occupancy.valid_pixels,
                       occupancy.max_region_area);
            } else if (occ_ret == ESP_OK) {
                printf("MLX90640 occupancy initializing: %u/%u background frames\r\n",
                       occupancy_detector.init_frames,
                       CG_OCCUPANCY_BACKGROUND_INIT_FRAMES);
            } else {
                printf("MLX90640 occupancy failed: %s\r\n", esp_err_to_name(occ_ret));
            }
        } else if (ret == ESP_OK) {
            printf("MLX90640 frame invalid: ts=%lu min=%.2f max=%.2f avg=%.2f hotspot=%u\r\n",
                   (unsigned long)frame.timestamp_ms,
                   frame.min_temp_c,
                   frame.max_temp_c,
                   frame.avg_temp_c,
                   frame.hotspot_index);
        } else {
            printf("MLX90640 read failed: %s\r\n", esp_err_to_name(ret));
        }

        int64_t elapsed_ms = (esp_timer_get_time() - loop_start_us) / 1000LL;
        if (elapsed_ms < CG_MLX90640_DEFAULT_PERIOD_MS) {
            vTaskDelay(pdMS_TO_TICKS(CG_MLX90640_DEFAULT_PERIOD_MS - elapsed_ms));
        }
    }
}

static void mlx90640_data_test_task(void *arg)
{
    (void)arg;
    mlx90640_data_test_run();
    vTaskDelete(NULL);
}

void mlx90640_data_test_start(void)
{
    BaseType_t ok = xTaskCreatePinnedToCore(mlx90640_data_test_task,
                                            "mlx90640_test",
                                            MLX90640_TEST_TASK_STACK_SIZE,
                                            NULL,
                                            MLX90640_TEST_TASK_PRIORITY,
                                            NULL,
                                            tskNO_AFFINITY);
    if (ok != pdPASS) {
        printf("MLX90640 test task create failed\r\n");
    }
}
