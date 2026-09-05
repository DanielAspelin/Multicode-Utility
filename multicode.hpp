#ifndef MULTICODE_HPP
#define MULTICODE_HPP

#include "unicode.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace MULTICODE {

enum class ENCODING {
    ASCII,
    UTF8,
    UTF16,
    UTF32
};

class ERROR {
public:
    enum class CODE {
        None,
        Unknown_Encoding,
        Invalid_Unit,
        Invalid_Lead_Unit,
        Invalid_Continuation_Unit,
        Truncated_Sequence,
        Overlong_Sequence,
        Isolated_Surrogate,
        Out_Of_Range,
        Unrepresentable
    };

    CODE Code { CODE::None };
    std::size_t Position { 0 };

    constexpr explicit operator bool() const noexcept
    {
        return Code != CODE::None;
    }
};

class RESULT {
public:
    std::u32string Scalars;
    std::vector<CODE_UNIT> Units;
    ERROR Error;

    constexpr explicit operator bool() const noexcept
    {
        return !static_cast<bool>(Error);
    }
};

class MULTICODE {
public:
    static bool Parse_Encoding(std::string_view name, ENCODING& output) noexcept;
    static std::string_view Encoding_Name(ENCODING encoding) noexcept;
    static std::string_view Error_Name(ERROR::CODE code) noexcept;

    static RESULT Decode(ENCODING source, const std::vector<CODE_UNIT>& units);
    static RESULT Encode(ENCODING target, std::u32string_view scalars);
    static RESULT Transcode(
        ENCODING source,
        ENCODING target,
        const std::vector<CODE_UNIT>& units
    );

    static bool Decode_UTF8_Text(
        std::string_view input,
        std::u32string& output,
        ERROR& error
    );

    static bool Encode_UTF8_Text(
        std::u32string_view input,
        std::string& output,
        ERROR& error
    );
};

} // namespace MULTICODE

#endif // MULTICODE_HPP
