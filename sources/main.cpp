#include "adapters/lvgl/lvgl_port_v8.h"
#include "pipeline/supervisor.h"
#include "pipeline/renderer.h"

#include "i2c_bus.h"
#include "nvs_flash.h"
#include "esp_log.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

static const char* TAG = "main";

#define I2C_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO GPIO_NUM_15
#define I2C_MASTER_SCL_IO GPIO_NUM_16
#define I2C_MASTER_FREQ_HZ 400000

#define BACKLIGHT_ADDR_V1_1 0x30

extern "C" void app_main(void) {
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = I2C_MASTER_FREQ_HZ},
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL,
    };
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM, &i2c_config);
    vTaskDelay(pdMS_TO_TICKS(50));

    // A blind first write to the backlight controller right after
    // i2c_bus_create() is unreliable even though it reports ESP_OK - the
    // STC8H1K28 needs a moment after power-on before it's actually ready.
    // Scanning the bus first (like the factory main.cpp does) reliably
    // gives it that time; skipping this step is what caused the "backlight
    // write succeeds but screen stays black" issue during bring-up.
    uint8_t addr_buf[16];
    i2c_bus_scan(i2c_bus, addr_buf, sizeof(addr_buf));

    // V1.1 backlight controller (STC8H1K28, addr 0x30): 0x10 = max brightness.
    i2c_bus_device_handle_t backlight_dev = i2c_bus_device_create(i2c_bus, BACKLIGHT_ADDR_V1_1, 0);
    if (backlight_dev) {
        esp_err_t err = i2c_bus_write_byte(backlight_dev, NULL_I2C_MEM_ADDR, 0x10);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Unable to configure backlight: %s", esp_err_to_name(err));
        }
        i2c_bus_device_delete(&backlight_dev);
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    Board* board = new Board();
    assert(board);
    ESP_UTILS_CHECK_FALSE_EXIT(board->init(), "Board init failed");
    ESP_UTILS_CHECK_FALSE_EXIT(board->begin(), "Board begin failed");
    ESP_UTILS_CHECK_FALSE_EXIT(lvgl_port_init(board->getLCD(), board->getTouch()), "LVGL init failed");

    lv_disp_t* dispp = lv_disp_get_default();
    lv_theme_t* theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE),
                                               lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    renderer::init(800, 480);
    supervisor::begin();
    supervisor::requestNewQuery();  // kick off the MVP pipeline immediately

    Touch* touch = board->getTouch();
    bool wasTouched = false;

    while (1) {
        supervisor::update();

        if (touch) {
            TouchPoint point;
            bool touched = touch->readPoints(&point, 1, 0) > 0;
            if (touched && !wasTouched) {
                supervisor::requestNewQuery();  // trigger only on the rising edge
            }
            wasTouched = touched;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
