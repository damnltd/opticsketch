#pragma once

#include <string>

namespace opticsketch {

// Write RGB image to PNG file.
// dataRGB: width * height * 3 bytes, row-major, bottom-up (OpenGL glReadPixels order).
// Returns true on success.
bool savePngToFile(const std::string& path, int width, int height, const unsigned char* dataRGB);

} // namespace opticsketch
