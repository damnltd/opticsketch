#include "export/export_png.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <cstring>

namespace opticsketch {

// Flip OpenGL bottom-up rows to top-down
static std::vector<unsigned char> flipRows(int width, int height, const unsigned char* dataRGB) {
    std::vector<unsigned char> flipped(static_cast<size_t>(width) * height * 3);
    const size_t rowBytes = static_cast<size_t>(width) * 3;
    for (int y = 0; y < height; ++y) {
        std::memcpy(&flipped[static_cast<size_t>(y) * rowBytes],
                     &dataRGB[static_cast<size_t>(height - 1 - y) * rowBytes],
                     rowBytes);
    }
    return flipped;
}

bool savePngToFile(const std::string& path, int width, int height, const unsigned char* dataRGB) {
    if (!dataRGB || width <= 0 || height <= 0) return false;
    auto flipped = flipRows(width, height, dataRGB);
    return stbi_write_png(path.c_str(), width, height, 3, flipped.data(), width * 3) != 0;
}

bool saveJpgToFile(const std::string& path, int width, int height, const unsigned char* dataRGB, int quality) {
    if (!dataRGB || width <= 0 || height <= 0) return false;
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    auto flipped = flipRows(width, height, dataRGB);
    return stbi_write_jpg(path.c_str(), width, height, 3, flipped.data(), quality) != 0;
}

// Callback for stbi_write_jpg_to_func: appends bytes to a std::vector
static void jpgWriteCallback(void* context, void* data, int size) {
    auto* buf = static_cast<std::vector<unsigned char>*>(context);
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

bool savePdfToFile(const std::string& path, int width, int height, const unsigned char* dataRGB) {
    if (!dataRGB || width <= 0 || height <= 0) return false;

    // Flip rows first
    auto flipped = flipRows(width, height, dataRGB);

    // Encode the image as JPEG in memory
    std::vector<unsigned char> jpegData;
    jpegData.reserve(static_cast<size_t>(width) * height); // rough estimate
    int ok = stbi_write_jpg_to_func(jpgWriteCallback, &jpegData, width, height, 3, flipped.data(), 90);
    if (!ok || jpegData.empty()) return false;

    // Build a minimal single-page PDF with the JPEG image
    // Page size matches image aspect ratio, 72 DPI base
    float pageW = static_cast<float>(width);
    float pageH = static_cast<float>(height);

    // Collect PDF objects with their byte offsets
    std::ostringstream pdf;
    std::vector<size_t> offsets; // byte offset of each object (1-indexed, offsets[0] unused)

    // Header
    pdf << "%PDF-1.4\n";
    // Binary comment to mark as binary PDF
    pdf << "%\xe2\xe3\xcf\xd3\n";

    // Object 1: Catalog
    offsets.push_back(0); // placeholder for index 0
    offsets.push_back(static_cast<size_t>(pdf.tellp()));
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    // Object 2: Pages
    offsets.push_back(static_cast<size_t>(pdf.tellp()));
    pdf << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";

    // Object 3: Page
    offsets.push_back(static_cast<size_t>(pdf.tellp()));
    pdf << "3 0 obj\n<< /Type /Page /Parent 2 0 R"
        << " /MediaBox [0 0 " << pageW << " " << pageH << "]"
        << " /Contents 4 0 R"
        << " /Resources << /XObject << /Img 5 0 R >> >> >>\nendobj\n";

    // Object 4: Content stream (draws the image full-page)
    std::ostringstream contentStream;
    contentStream << "q\n" << pageW << " 0 0 " << pageH << " 0 0 cm\n/Img Do\nQ\n";
    std::string content = contentStream.str();

    offsets.push_back(static_cast<size_t>(pdf.tellp()));
    pdf << "4 0 obj\n<< /Length " << content.size() << " >>\nstream\n"
        << content << "endstream\nendobj\n";

    // Object 5: Image XObject (JPEG)
    offsets.push_back(static_cast<size_t>(pdf.tellp()));
    pdf << "5 0 obj\n<< /Type /XObject /Subtype /Image"
        << " /Width " << width
        << " /Height " << height
        << " /ColorSpace /DeviceRGB"
        << " /BitsPerComponent 8"
        << " /Filter /DCTDecode"
        << " /Length " << jpegData.size()
        << " >>\nstream\n";

    // We need to write the PDF header + jpeg binary + trailer
    // So we build the header part, then binary, then trailer
    std::string headerPart = pdf.str();
    pdf.str("");
    pdf.clear();

    pdf << "\nendstream\nendobj\n";

    // Cross-reference table
    size_t jpegOffset = headerPart.size();
    // Recalculate offset for object 5 accounting for the fact we split the stream
    // Object 5 offset is already correct in offsets[5] since it was measured before the split

    std::string afterJpeg = pdf.str();

    // Now we need to fix: the xref offsets for objects after the JPEG need adjustment
    // Objects 1-4 are in headerPart, object 5 starts in headerPart but its stream includes jpegData
    // All offsets in offsets[] are relative to headerPart start, which is correct

    // Build xref
    size_t xrefOffset = headerPart.size() + jpegData.size() + afterJpeg.size();
    int numObjects = 6; // 0 through 5

    std::ostringstream xref;
    xref << "xref\n0 " << numObjects << "\n";
    // Object 0: free
    xref << "0000000000 65535 f \n";
    for (int i = 1; i < numObjects; ++i) {
        char buf[21];
        snprintf(buf, sizeof(buf), "%010zu 00000 n \n", offsets[i]);
        xref << buf;
    }

    // Trailer
    xref << "trailer\n<< /Size " << numObjects << " /Root 1 0 R >>\n";
    xref << "startxref\n" << xrefOffset << "\n%%EOF\n";

    std::string xrefStr = xref.str();

    // Write everything to file
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    out.write(headerPart.data(), static_cast<std::streamsize>(headerPart.size()));
    out.write(reinterpret_cast<const char*>(jpegData.data()), static_cast<std::streamsize>(jpegData.size()));
    out.write(afterJpeg.data(), static_cast<std::streamsize>(afterJpeg.size()));
    out.write(xrefStr.data(), static_cast<std::streamsize>(xrefStr.size()));

    return out.good();
}

} // namespace opticsketch
