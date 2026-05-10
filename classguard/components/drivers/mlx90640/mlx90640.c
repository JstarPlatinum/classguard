#include "mlx90640.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include "app_config.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "i2c_bus.h"
#include "pin_map.h"

static const char *TAG = "cg_mlx90640";

#define MLX90640_REFRESH_RATE_2HZ 0x02U
#define MLX90640_FRAME_SUBPAGES 2U
#define MLX90640_EMISSIVITY_DEFAULT 0.95f

static bool mlx90640_temperature_is_valid(float value)
{
    return isfinite(value) && value >= CG_MLX90640_VALID_MIN_TEMP_C && value <= CG_MLX90640_VALID_MAX_TEMP_C;
}

static float mlx90640_neighbor_average_or_zero(const float *pixels, int x, int y)
{
    float sum = 0.0f;
    uint16_t count = 0;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= (int)CG_MLX90640_WIDTH || ny < 0 || ny >= (int)CG_MLX90640_HEIGHT) {
                continue;
            }

            float value = pixels[ny * CG_MLX90640_WIDTH + nx];
            if (mlx90640_temperature_is_valid(value)) {
                sum += value;
                ++count;
            }
        }
    }

    return (count > 0) ? (sum / (float)count) : 0.0f;
}

static void mlx90640_repair_invalid_pixels(thermal_frame_t *frame)
{
    float original[CG_MLX90640_PIXEL_COUNT];
    memcpy(original, frame->pixels, sizeof(original));

    for (int y = 0; y < (int)CG_MLX90640_HEIGHT; ++y) {
        for (int x = 0; x < (int)CG_MLX90640_WIDTH; ++x) {
            size_t index = (size_t)y * CG_MLX90640_WIDTH + (size_t)x;
            if (!mlx90640_temperature_is_valid(original[index])) {
                frame->pixels[index] = mlx90640_neighbor_average_or_zero(original, x, y);
            }
        }
    }
}

static void mlx90640_finalize_frame(thermal_frame_t *frame)
{
    mlx90640_repair_invalid_pixels(frame);

    float min_temp = FLT_MAX;
    float max_temp = -FLT_MAX;
    float sum_temp = 0.0f;
    uint16_t hotspot_index = 0;

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        float value = frame->pixels[i];
        if (value < min_temp) {
            min_temp = value;
        }
        if (value > max_temp) {
            max_temp = value;
            hotspot_index = i;
        }
        sum_temp += value;
    }

    frame->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    frame->min_temp_c = min_temp;
    frame->max_temp_c = max_temp;
    frame->avg_temp_c = sum_temp / (float)CG_MLX90640_PIXEL_COUNT;
    frame->hotspot_index = hotspot_index;
    frame->valid = true;
}

esp_err_t cg_mlx90640_init(cg_mlx90640_t *dev, i2c_port_t port)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->port = port;
    dev->address = CG_ADDR_MLX90640;
    dev->state = CG_DRIVER_STATE_UNINITIALIZED;
    dev->params_valid = false;

    ESP_RETURN_ON_ERROR(cg_i2c_bus_init_mlx(), TAG, "mlx i2c init failed");
    ESP_RETURN_ON_ERROR(cg_mlx90640_probe(dev), TAG, "mlx probe failed");

    static uint16_t eeprom[CG_MLX90640_EEPROM_WORDS];
    ESP_RETURN_ON_ERROR(cg_mlx90640_read_eeprom(dev, eeprom, CG_MLX90640_EEPROM_WORDS), TAG, "mlx eeprom read failed");

    int api_ret = MLX90640_ExtractParameters(eeprom, &dev->params);
    if (api_ret != MLX90640_NO_ERROR) {
        return ESP_FAIL;
    }
    dev->params_valid = true;

    api_ret = MLX90640_SetRefreshRate(dev->address, MLX90640_REFRESH_RATE_2HZ);
    if (api_ret != MLX90640_NO_ERROR) {
        return ESP_FAIL;
    }

    dev->state = CG_DRIVER_STATE_READY;
    return ESP_OK;
}

esp_err_t cg_mlx90640_probe(cg_mlx90640_t *dev)
{
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cg_i2c_bus_probe(dev->port, dev->address, pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
}

esp_err_t cg_mlx90640_read_register(cg_mlx90640_t *dev, uint16_t reg, uint16_t *value)
{
    if (dev == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bytes[2];
    ESP_RETURN_ON_ERROR(cg_i2c_read_reg16(dev->port, dev->address, reg, bytes, sizeof(bytes), pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS)), TAG, "mlx read register failed");
    *value = ((uint16_t)bytes[0] << 8) | bytes[1];
    return ESP_OK;
}

esp_err_t cg_mlx90640_read_words(cg_mlx90640_t *dev, uint16_t start_reg, uint16_t *words, size_t word_count)
{
    if (dev == NULL || words == NULL || word_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t offset = 0;
    while (offset < word_count) {
        size_t chunk_words = word_count - offset;
        if (chunk_words > 16) {
            chunk_words = 16;
        }

        uint8_t bytes[32];
        ESP_RETURN_ON_ERROR(cg_i2c_read_reg16(dev->port, dev->address, start_reg + offset, bytes, chunk_words * 2, pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS)), TAG, "mlx read words failed");
        for (size_t i = 0; i < chunk_words; ++i) {
            words[offset + i] = ((uint16_t)bytes[i * 2] << 8) | bytes[i * 2 + 1];
        }
        offset += chunk_words;
    }

    return ESP_OK;
}

esp_err_t cg_mlx90640_read_eeprom(cg_mlx90640_t *dev, uint16_t *eeprom_words, size_t word_count)
{
    if (word_count < CG_MLX90640_EEPROM_WORDS) {
        return ESP_ERR_INVALID_SIZE;
    }
    return cg_mlx90640_read_words(dev, CG_MLX90640_EEPROM_START, eeprom_words, CG_MLX90640_EEPROM_WORDS);
}

esp_err_t cg_mlx90640_read_frame_raw(cg_mlx90640_t *dev, uint16_t *frame_words, size_t word_count)
{
    if (word_count < CG_MLX90640_FRAME_WORDS) {
        return ESP_ERR_INVALID_SIZE;
    }
    return cg_mlx90640_read_words(dev, CG_MLX90640_FRAME_START, frame_words, CG_MLX90640_FRAME_WORDS);
}

esp_err_t cg_mlx90640_calculate_temperatures(const uint16_t *eeprom_words, const uint16_t *frame_words, thermal_frame_t *frame)
{
    if (eeprom_words == NULL || frame_words == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    static paramsMLX90640 params;
    int api_ret = MLX90640_ExtractParameters((uint16_t *)eeprom_words, &params);
    if (api_ret != MLX90640_NO_ERROR) {
        frame->valid = false;
        return ESP_FAIL;
    }

    memset(frame, 0, sizeof(*frame));
    float ta = MLX90640_GetTa((uint16_t *)frame_words, &params);
    MLX90640_CalculateTo((uint16_t *)frame_words, &params, MLX90640_EMISSIVITY_DEFAULT, ta - 8.0f, frame->pixels);
    MLX90640_BadPixelsCorrection(params.brokenPixels, frame->pixels, 1, &params);
    MLX90640_BadPixelsCorrection(params.outlierPixels, frame->pixels, 1, &params);

    mlx90640_finalize_frame(frame);
    return ESP_OK;
}

esp_err_t cg_mlx90640_read_thermal_frame(cg_mlx90640_t *dev, thermal_frame_t *frame)
{
    if (dev == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!dev->params_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    static uint16_t frame_data[CG_MLX90640_FRAME_WORDS];
    static float pixels[CG_MLX90640_PIXEL_COUNT];
    memset(pixels, 0, sizeof(pixels));

    for (size_t subpage = 0; subpage < MLX90640_FRAME_SUBPAGES; ++subpage) {
        int api_ret = MLX90640_GetFrameData(dev->address, frame_data);
        if (api_ret < MLX90640_NO_ERROR || api_ret > 1) {
            frame->valid = false;
            return ESP_FAIL;
        }

        float ta = MLX90640_GetTa(frame_data, &dev->params);
        MLX90640_CalculateTo(frame_data, &dev->params, MLX90640_EMISSIVITY_DEFAULT, ta - 8.0f, pixels);
    }

    MLX90640_BadPixelsCorrection(dev->params.brokenPixels, pixels, 1, &dev->params);
    MLX90640_BadPixelsCorrection(dev->params.outlierPixels, pixels, 1, &dev->params);

    for (uint16_t i = 0; i < CG_MLX90640_PIXEL_COUNT; ++i) {
        frame->pixels[i] = pixels[i];
    }

    mlx90640_finalize_frame(frame);
    return ESP_OK;
}
