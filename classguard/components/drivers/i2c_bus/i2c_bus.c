#include "i2c_bus.h"

#include <stdbool.h>
#include "app_config.h"
#include "esp_check.h"
#include "pin_map.h"

static const char *TAG = "cg_i2c_bus";
static bool s_i2c_initialized[I2C_NUM_MAX];

esp_err_t cg_i2c_bus_init(const cg_i2c_bus_config_t *config)
{
    if (config == NULL || config->port < 0 || config->port >= I2C_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_i2c_initialized[config->port]) {
        return ESP_OK;
    }

    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_pin,
        .scl_io_num = config->scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->frequency_hz,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(config->port, &i2c_config), TAG, "i2c_param_config failed");
    esp_err_t ret = i2c_driver_install(config->port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret == ESP_ERR_INVALID_STATE) {
        s_i2c_initialized[config->port] = true;
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "i2c_driver_install failed");
    s_i2c_initialized[config->port] = true;
    return ESP_OK;
}

esp_err_t cg_i2c_bus_init_mlx(void)
{
    const cg_i2c_bus_config_t config = {
        .port = CG_I2C_MLX_PORT,
        .sda_pin = CG_PIN_MLX_SDA,
        .scl_pin = CG_PIN_MLX_SCL,
        .frequency_hz = CG_I2C_MLX_FREQ_HZ,
    };
    return cg_i2c_bus_init(&config);
}

esp_err_t cg_i2c_bus_init_env(void)
{
    const cg_i2c_bus_config_t config = {
        .port = CG_I2C_ENV_PORT,
        .sda_pin = CG_PIN_ENV_SDA,
        .scl_pin = CG_PIN_ENV_SCL,
        .frequency_hz = CG_I2C_ENV_FREQ_HZ,
    };
    return cg_i2c_bus_init(&config);
}

esp_err_t cg_i2c_bus_probe(i2c_port_t port, uint8_t address, TickType_t timeout_ticks)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, timeout_ticks);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t cg_i2c_write(i2c_port_t port, uint8_t address, const uint8_t *data, size_t length, TickType_t timeout_ticks)
{
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, length, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, timeout_ticks);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t cg_i2c_read(i2c_port_t port, uint8_t address, uint8_t *data, size_t length, TickType_t timeout_ticks)
{
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    if (length > 1) {
        i2c_master_read(cmd, data, length - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + length - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, timeout_ticks);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t cg_i2c_write_read(i2c_port_t port, uint8_t address, const uint8_t *write_data, size_t write_length, uint8_t *read_data, size_t read_length, TickType_t timeout_ticks)
{
    if (write_data == NULL || write_length == 0 || read_data == NULL || read_length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_data, write_length, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, true);
    if (read_length > 1) {
        i2c_master_read(cmd, read_data, read_length - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, read_data + read_length - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, timeout_ticks);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t cg_i2c_read_reg16(i2c_port_t port, uint8_t address, uint16_t reg, uint8_t *data, size_t length, TickType_t timeout_ticks)
{
    uint8_t reg_bytes[2] = {(uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF)};
    return cg_i2c_write_read(port, address, reg_bytes, sizeof(reg_bytes), data, length, timeout_ticks);
}

esp_err_t cg_i2c_write_reg16(i2c_port_t port, uint8_t address, uint16_t reg, const uint8_t *data, size_t length, TickType_t timeout_ticks)
{
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[34];
    if (length + 2 > sizeof(buffer)) {
        return ESP_ERR_INVALID_SIZE;
    }

    buffer[0] = (uint8_t)(reg >> 8);
    buffer[1] = (uint8_t)(reg & 0xFF);
    for (size_t i = 0; i < length; ++i) {
        buffer[i + 2] = data[i];
    }

    return cg_i2c_write(port, address, buffer, length + 2, timeout_ticks);
}
