#include "jpeg_info.hpp"

// Minimal JPEG dimension parser (SOF0/1/2/3/5/6/7/9/10/11/12/13/14/15)
bool parse_jpeg_info(const std::vector<uint8_t> &data, uint16_t &width, uint16_t &height,
                     bool &is_yuv420) {
    if (data.size() < 10 || data[0] != 0xFF || data[1] != 0xD8) {
        return false;  // not a JPEG SOI
    }

    size_t i = 2;
    while (i + 3 < data.size()) {
        if (data[i] != 0xFF) {
            ++i;
            continue;
        }
        uint8_t marker = data[i + 1];
        // Skip padding FFs.
        if (marker == 0xFF) {
            ++i;
            continue;
        }

        // EOI or SOS ends the searchable header section.
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }

        uint16_t seg_len = (static_cast<uint16_t>(data[i + 2]) << 8) | data[i + 3];
        if (seg_len < 2 || i + 2 + seg_len > data.size()) {
            break;
        }

        bool is_sof = (marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
                      (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF);
        if (is_sof && seg_len >= 7) {
            height = (static_cast<uint16_t>(data[i + 5]) << 8) | data[i + 6];
            width = (static_cast<uint16_t>(data[i + 7]) << 8) | data[i + 8];
            is_yuv420 = false;
            // Subsampling factors live in component tables that follow; expect 3 components.
            if (seg_len >= 2 + 6 + 3 * 3) {
                uint8_t comps = data[i + 9];
                if (comps == 3) {
                    uint8_t hv1 = data[i + 11];
                    uint8_t hv2 = data[i + 14];
                    uint8_t hv3 = data[i + 17];
                    uint8_t h1 = hv1 >> 4, v1 = hv1 & 0x0F;
                    uint8_t h2 = hv2 >> 4, v2 = hv2 & 0x0F;
                    uint8_t h3 = hv3 >> 4, v3 = hv3 & 0x0F;
                    if (h1 == 2 && v1 == 2 && h2 == 1 && v2 == 1 && h3 == 1 && v3 == 1) {
                        is_yuv420 = true;
                    }
                }
            }
            return true;
        }

        i += 2 + seg_len;
    }
    return false;
}

