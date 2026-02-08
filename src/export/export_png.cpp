#include "export/export_png.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vector>

namespace opticsketch {

bool savePngToFile(const std::string& path, int width, int height, const unsigned char* dataRGB) {
    if (!dataRGB || width <= 0 || height <= 0) return false;
    const int stride = width * 3;
    // OpenGL gives bottom-up; PNG expects top-down. Flip rows.
    std::vector<unsigned char> flipped(static_cast<size_t>(width) * height * 3);
    for (int y = height - 1; y >= 0; --y)
        for (int x = 0; x < width; ++x)
            for (int c = 0; c < 3; ++c)
                flipped[(static_cast<size_t>(height - 1 - y) * width + x) * 3 + c] = dataRGB[(static_cast<size_t>(y) * width + x) * 3 + c];
    return stbi_write_png(path.c_str(), width, height, 3, flipped.data(), width * 3) != 0;
}

} // namespace opticsketch
