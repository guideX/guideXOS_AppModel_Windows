#include "text_validation.hpp"

#include <stdexcept>

namespace guidexos::appmodel::detail {
namespace {

bool IsContinuationByte(unsigned char value) noexcept {
    return value >= 0x80U && value <= 0xBFU;
}

} // namespace

void ValidateUtf8(const std::string& value) {
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char first = static_cast<unsigned char>(value[index]);
        if (first <= 0x7FU) {
            ++index;
            continue;
        }

        if (first >= 0xC2U && first <= 0xDFU) {
            if (index + 1 >= value.size() ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 1]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 2;
            continue;
        }

        if (first == 0xE0U) {
            if (index + 2 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0xA0U ||
                static_cast<unsigned char>(value[index + 1]) > 0xBFU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 3;
            continue;
        }

        if ((first >= 0xE1U && first <= 0xECU) ||
            (first >= 0xEEU && first <= 0xEFU)) {
            if (index + 2 >= value.size() ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 1])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 3;
            continue;
        }

        if (first == 0xEDU) {
            if (index + 2 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0x80U ||
                static_cast<unsigned char>(value[index + 1]) > 0x9FU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 3;
            continue;
        }

        if (first == 0xF0U) {
            if (index + 3 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0x90U ||
                static_cast<unsigned char>(value[index + 1]) > 0xBFU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 3]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 4;
            continue;
        }

        if (first >= 0xF1U && first <= 0xF3U) {
            if (index + 3 >= value.size() ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 1])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 3]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 4;
            continue;
        }

        if (first == 0xF4U) {
            if (index + 3 >= value.size() ||
                static_cast<unsigned char>(value[index + 1]) < 0x80U ||
                static_cast<unsigned char>(value[index + 1]) > 0x8FU ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 2])) ||
                !IsContinuationByte(static_cast<unsigned char>(value[index + 3]))) {
                throw std::invalid_argument("App Model text is not valid UTF-8");
            }
            index += 4;
            continue;
        }

        throw std::invalid_argument("App Model text is not valid UTF-8");
    }
}

} // namespace guidexos::appmodel::detail
