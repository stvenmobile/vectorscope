#include "sdcard.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

namespace sdcard {

namespace {

const char* TAG = "sdcard";
const char* MOUNT_POINT = "/sdcard";
sdmmc_card_t* g_card = nullptr;

#define SD_PIN_MOSI GPIO_NUM_6
#define SD_PIN_MISO GPIO_NUM_4
#define SD_PIN_SCK GPIO_NUM_5
#define SD_PIN_CS GPIO_NUM_0

struct BmpInfo {
    uint32_t dataOffset;
    int32_t width;
    int32_t height;  // positive = bottom-up (standard), negative = top-down
    uint16_t bpp;
    uint32_t compression;
};

bool readBmpInfo(FILE* f, BmpInfo& out) {
    uint8_t header[54];
    if (fread(header, 1, sizeof(header), f) < sizeof(header)) return false;
    if (header[0] != 'B' || header[1] != 'M') return false;

    out.dataOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    out.width = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    out.height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    out.bpp = header[28] | (header[29] << 8);
    out.compression = header[30] | (header[31] << 8) | (header[32] << 16) | (header[33] << 24);
    return true;
}

bool isBmpFilename(const char* name) {
    size_t len = strlen(name);
    return len > 4 && strcasecmp(name + len - 4, ".bmp") == 0;
}

}  // namespace

bool init() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SD_PIN_MOSI;
    bus_cfg.miso_io_num = SD_PIN_MISO;
    bus_cfg.sclk_io_num = SD_PIN_SCK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 4000;

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &g_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem - is the card formatted FAT32?");
        } else {
            ESP_LOGE(TAG, "Failed to init SD card: %s", esp_err_to_name(ret));
        }
        return false;
    }

    ESP_LOGI(TAG, "SD card mounted at %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, g_card);
    return true;
}

void listAndValidateImages() {
    DIR* dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open %s", MOUNT_POINT);
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        char path[300];
        snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, entry->d_name);

        if (!isBmpFilename(entry->d_name)) {
            ESP_LOGI(TAG, "  %s (not a .bmp, skipping)", entry->d_name);
            continue;
        }

        FILE* f = fopen(path, "rb");
        if (!f) {
            ESP_LOGE(TAG, "  %s: failed to open", entry->d_name);
            continue;
        }

        BmpInfo info;
        bool validHeader = readBmpInfo(f, info);
        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fclose(f);

        if (!validHeader) {
            ESP_LOGE(TAG, "  %s: not a valid BMP (bad header)", entry->d_name);
            continue;
        }

        bool ok = (info.dataOffset == 54) && (info.width == 800) &&
                  (info.height == 480 || info.height == -480) && (info.bpp == 24) && (info.compression == 0);

        ESP_LOGI(TAG, "  %s: %ldx%ld, %dbpp, compression=%lu, data@%lu, file size=%ld bytes -> %s", entry->d_name,
                 (long)info.width, (long)info.height, info.bpp, (unsigned long)info.compression,
                 (unsigned long)info.dataOffset, fileSize, ok ? "OK" : "PROBLEM");
    }

    closedir(dir);
}

std::vector<std::string> listValidBmp(int width, int height) {
    std::vector<std::string> names;

    DIR* dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open %s", MOUNT_POINT);
        return names;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (!isBmpFilename(entry->d_name)) continue;

        char path[300];
        snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, entry->d_name);
        FILE* f = fopen(path, "rb");
        if (!f) continue;

        BmpInfo info;
        bool validHeader = readBmpInfo(f, info);
        fclose(f);

        if (validHeader && info.dataOffset == 54 && info.width == width &&
            (info.height == height || info.height == -height) && info.bpp == 24 && info.compression == 0) {
            names.emplace_back(entry->d_name);
        }
    }

    closedir(dir);
    return names;
}

bool loadBmp(const std::string& filename, uint16_t* destRGB565Buf, int width, int height) {
    char path[300];
    snprintf(path, sizeof(path), "%s/%s", MOUNT_POINT, filename.c_str());

    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "loadBmp: failed to open %s", filename.c_str());
        return false;
    }

    BmpInfo info;
    if (!readBmpInfo(f, info) || info.width != width || (info.height != height && info.height != -height) ||
        info.bpp != 24 || info.compression != 0) {
        ESP_LOGE(TAG, "loadBmp: %s does not match expected %dx%d/24bpp/uncompressed format", filename.c_str(),
                 width, height);
        fclose(f);
        return false;
    }

    bool bottomUp = info.height > 0;
    int rowSize = ((width * 3 + 3) / 4) * 4;  // BMP rows are padded to a 4-byte boundary
    uint8_t* rowBuf = (uint8_t*)malloc(rowSize);
    if (!rowBuf) {
        ESP_LOGE(TAG, "loadBmp: failed to allocate row buffer");
        fclose(f);
        return false;
    }

    fseek(f, info.dataOffset, SEEK_SET);

    bool ok = true;
    for (int row = 0; row < height && ok; row++) {
        if (fread(rowBuf, 1, rowSize, f) != (size_t)rowSize) {
            ESP_LOGE(TAG, "loadBmp: %s: short read at row %d", filename.c_str(), row);
            ok = false;
            break;
        }
        int destRow = bottomUp ? (height - 1 - row) : row;
        uint16_t* destRowPtr = destRGB565Buf + destRow * width;
        for (int x = 0; x < width; x++) {
            uint8_t b = rowBuf[x * 3 + 0];
            uint8_t g = rowBuf[x * 3 + 1];
            uint8_t r = rowBuf[x * 3 + 2];
            destRowPtr[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
    }

    free(rowBuf);
    fclose(f);
    return ok;
}

}  // namespace sdcard
