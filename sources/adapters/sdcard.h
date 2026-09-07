#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Mounts the SD card (SPI mode) as a FAT filesystem at /sdcard. Uses the
// pinout confirmed from Elecrow's own V1.1 SD-card example: MOSI=6, MISO=4,
// SCK=5, CS=0.
namespace sdcard {

bool init();

// Diagnostic helper: lists every file at the SD card root and, for any
// .bmp file, parses just its header (no full pixel decode) to report
// width, height, bit depth and compression.
void listAndValidateImages();

// Returns the filenames (relative to the SD root) of every .bmp file that
// is exactly width x height, 24bpp, uncompressed - i.e. ready to load
// directly with loadBmp().
std::vector<std::string> listValidBmp(int width, int height);

// Loads one such file into destRGB565Buf (must already be allocated,
// width*height uint16_t), converting each 24-bit BGR row to RGB565 as it
// reads. Returns false on any I/O or format problem.
bool loadBmp(const std::string& filename, uint16_t* destRGB565Buf, int width, int height);

}  // namespace sdcard
