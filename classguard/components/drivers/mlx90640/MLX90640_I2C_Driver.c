#include "MLX90640_I2C_Driver.h"

#include "app_config.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "i2c_bus.h"
#include "pin_map.h"

#define MLX90640_I2C_CHUNK_WORDS 16U

void MLX90640_I2CInit(void)
{
    (void)cg_i2c_bus_init_mlx();
}

int MLX90640_I2CGeneralReset(void)
{
    return MLX90640_NO_ERROR;
}

int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data)
{
    if (data == NULL || nMemAddressRead == 0) {
        return -MLX90640_I2C_NACK_ERROR;
    }

    size_t offset = 0;
    while (offset < nMemAddressRead) {
        size_t chunk_words = nMemAddressRead - offset;
        if (chunk_words > MLX90640_I2C_CHUNK_WORDS) {
            chunk_words = MLX90640_I2C_CHUNK_WORDS;
        }

        uint8_t bytes[MLX90640_I2C_CHUNK_WORDS * 2U];
        esp_err_t ret = cg_i2c_read_reg16(CG_I2C_MLX_PORT,
                                          slaveAddr,
                                          (uint16_t)(startAddress + offset),
                                          bytes,
                                          chunk_words * 2U,
                                          pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
        if (ret != ESP_OK) {
            return -MLX90640_I2C_NACK_ERROR;
        }

        for (size_t i = 0; i < chunk_words; ++i) {
            data[offset + i] = ((uint16_t)bytes[i * 2U] << 8) | bytes[i * 2U + 1U];
        }
        offset += chunk_words;
    }

    return MLX90640_NO_ERROR;
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    const uint8_t bytes[2] = {
        (uint8_t)(data >> 8),
        (uint8_t)(data & 0xFF),
    };
    esp_err_t ret = cg_i2c_write_reg16(CG_I2C_MLX_PORT,
                                       slaveAddr,
                                       writeAddress,
                                       bytes,
                                       sizeof(bytes),
                                       pdMS_TO_TICKS(CG_I2C_TIMEOUT_MS));
    return (ret == ESP_OK) ? MLX90640_NO_ERROR : -MLX90640_I2C_WRITE_ERROR;
}

void MLX90640_I2CFreqSet(int freq)
{
    (void)freq;
}
