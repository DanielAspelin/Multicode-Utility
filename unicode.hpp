#ifndef UNICODE_HPP
#define UNICODE_HPP

#include <cstdint>

namespace MULTICODE {

using CODE_POINT = char32_t;
using CODE_UNIT = std::uint32_t;

class UNICODE {
public:
    enum class PLANE : std::uint8_t {
        Basic_Multilingual = 0,
        Supplementary_Multilingual = 1,
        Supplementary_Ideographic = 2,
        Tertiary_Ideographic = 3,
        Unassigned_4 = 4,
        Unassigned_5 = 5,
        Unassigned_6 = 6,
        Unassigned_7 = 7,
        Unassigned_8 = 8,
        Unassigned_9 = 9,
        Unassigned_10 = 10,
        Unassigned_11 = 11,
        Unassigned_12 = 12,
        Unassigned_13 = 13,
        Supplementary_Special_Purpose = 14,
        Private_Use_A = 15,
        Private_Use_B = 16,
        Invalid = 255
    };

    inline static constexpr CODE_POINT Minimum = 0x000000;
    inline static constexpr CODE_POINT Maximum = 0x10FFFF;
    inline static constexpr CODE_POINT Surrogate_Begin = 0xD800;
    inline static constexpr CODE_POINT Surrogate_End = 0xDFFF;
    inline static constexpr CODE_POINT Replacement = 0xFFFD;
    inline static constexpr CODE_POINT ASCII_End = 0x7F;

    static constexpr bool Is_Surrogate(CODE_POINT value) noexcept
    {
        return value >= Surrogate_Begin && value <= Surrogate_End;
    }

    static constexpr bool Is_Scalar(CODE_POINT value) noexcept
    {
        return value <= Maximum && !Is_Surrogate(value);
    }

    static constexpr bool Is_ASCII(CODE_POINT value) noexcept
    {
        return value <= ASCII_End;
    }

    static constexpr PLANE Plane(CODE_POINT value) noexcept
    {
        return Is_Scalar(value)
            ? static_cast<PLANE>(static_cast<std::uint32_t>(value) >> 16)
            : PLANE::Invalid;
    }
};

} // namespace MULTICODE

#endif // UNICODE_HPP
