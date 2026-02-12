#pragma once

#include <string>

namespace opticsketch {

// Write RGB image to PNG file.
// dataRGB: width * height * 3 bytes, row-major, bottom-up (OpenGL glReadPixels order).
// Returns true on success.
bool savePngToFile(const std::string& path, int width, int height, const unsigned char* dataRGB);

// Write RGB image to JPEG file.
// dataRGB: width * height * 3 bytes, row-major, bottom-up (OpenGL glReadPixels order).
// quality: 1-100 (higher = better quality, larger file).
// Returns true on success.
bool saveJpgToFile(const std::string& path, int width, int height, const unsigned char* dataRGB, int quality = 90);

// Write RGB image to a single-page PDF file with the image filling the page.
// dataRGB: width * height * 3 bytes, row-major, bottom-up (OpenGL glReadPixels order).
// Returns true on success.
bool savePdfToFile(const std::string& path, int width, int height, const unsigned char* dataRGB);

} // namespace opticsketch
