#include "multicode.hpp"
#include "unicode.hpp"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace MULTICODE {

bool MULTICODE::Parse_Encoding(std::string_view name, ENCODING& output) noexcept
{
    if (name == "ascii" || name == "ASCII") output = ENCODING::ASCII;
    else if (name == "utf8" || name == "utf-8" || name == "UTF-8") output = ENCODING::UTF8;
    else if (name == "utf16" || name == "utf-16" || name == "UTF-16") output = ENCODING::UTF16;
    else if (name == "utf32" || name == "utf-32" || name == "UTF-32") output = ENCODING::UTF32;
    else return false;
    return true;
}

std::string_view MULTICODE::Encoding_Name(ENCODING encoding) noexcept
{
    switch (encoding) {
        case ENCODING::ASCII: return "ASCII";
        case ENCODING::UTF8:  return "UTF-8";
        case ENCODING::UTF16: return "UTF-16";
        case ENCODING::UTF32: return "UTF-32";
    }
    return "unknown";
}

std::string_view MULTICODE::Error_Name(ERROR::CODE code) noexcept
{
    switch (code) {
        case ERROR::CODE::None:                      return "none";
        case ERROR::CODE::Unknown_Encoding:          return "unknown encoding";
        case ERROR::CODE::Invalid_Unit:              return "invalid code unit";
        case ERROR::CODE::Invalid_Lead_Unit:         return "invalid UTF-8 lead unit";
        case ERROR::CODE::Invalid_Continuation_Unit: return "invalid UTF-8 continuation unit";
        case ERROR::CODE::Truncated_Sequence:        return "truncated sequence";
        case ERROR::CODE::Overlong_Sequence:         return "overlong UTF-8 sequence";
        case ERROR::CODE::Isolated_Surrogate:        return "isolated UTF-16 surrogate";
        case ERROR::CODE::Out_Of_Range:              return "value outside Unicode range";
        case ERROR::CODE::Unrepresentable:           return "value not representable in target encoding";
    }
    return "unknown error";
}

RESULT MULTICODE::Decode(ENCODING source, const std::vector<CODE_UNIT>& units)
{
    RESULT result;

    if (source == ENCODING::ASCII) {
        for (std::size_t index = 0; index < units.size(); ++index) {
            if (units[index] > 0x7F) {
                result.Error = { ERROR::CODE::Invalid_Unit, index };
                return result;
            }
            result.Scalars.push_back(static_cast<CODE_POINT>(units[index]));
        }
        return result;
    }

    if (source == ENCODING::UTF8) {
        for (std::size_t index = 0; index < units.size();) {
            if (units[index] > 0xFF) {
                result.Error = { ERROR::CODE::Invalid_Unit, index };
                return result;
            }

            const CODE_UNIT lead = units[index];
            CODE_POINT scalar = 0;
            std::size_t length = 0;
            if (lead <= 0x7F) {
                scalar = static_cast<CODE_POINT>(lead); length = 1;
            } else if (lead >= 0xC2 && lead <= 0xDF) {
                scalar = static_cast<CODE_POINT>(lead & 0x1F); length = 2;
            } else if (lead >= 0xE0 && lead <= 0xEF) {
                scalar = static_cast<CODE_POINT>(lead & 0x0F); length = 3;
            } else if (lead >= 0xF0 && lead <= 0xF4) {
                scalar = static_cast<CODE_POINT>(lead & 0x07); length = 4;
            } else {
                result.Error = { ERROR::CODE::Invalid_Lead_Unit, index };
                return result;
            }

            if (index + length > units.size()) {
                result.Error = { ERROR::CODE::Truncated_Sequence, index };
                return result;
            }
            for (std::size_t offset = 1; offset < length; ++offset) {
                if (units[index + offset] > 0xFF ||
                    (units[index + offset] & 0xC0) != 0x80) {
                    result.Error = {
                        ERROR::CODE::Invalid_Continuation_Unit, index + offset
                    };
                    return result;
                }
                scalar = static_cast<CODE_POINT>(
                    (scalar << 6) | (units[index + offset] & 0x3F)
                );
            }

            if ((length == 2 && scalar < 0x80) ||
                (length == 3 && scalar < 0x800) ||
                (length == 4 && scalar < 0x10000)) {
                result.Error = { ERROR::CODE::Overlong_Sequence, index };
                return result;
            }
            if (UNICODE::Is_Surrogate(scalar)) {
                result.Error = { ERROR::CODE::Isolated_Surrogate, index };
                return result;
            }
            if (!UNICODE::Is_Scalar(scalar)) {
                result.Error = { ERROR::CODE::Out_Of_Range, index };
                return result;
            }

            result.Scalars.push_back(scalar);
            index += length;
        }
        return result;
    }

    if (source == ENCODING::UTF16) {
        for (std::size_t index = 0; index < units.size(); ++index) {
            const CODE_UNIT first = units[index];
            if (first > 0xFFFF) {
                result.Error = { ERROR::CODE::Invalid_Unit, index };
                return result;
            }
            if (first >= 0xD800 && first <= 0xDBFF) {
                if (index + 1 >= units.size()) {
                    result.Error = { ERROR::CODE::Truncated_Sequence, index };
                    return result;
                }
                const CODE_UNIT second = units[index + 1];
                if (second < 0xDC00 || second > 0xDFFF) {
                    result.Error = { ERROR::CODE::Isolated_Surrogate, index + 1 };
                    return result;
                }
                result.Scalars.push_back(static_cast<CODE_POINT>(
                    0x10000 + (((first - 0xD800) << 10) | (second - 0xDC00))
                ));
                ++index;
            } else if (first >= 0xDC00 && first <= 0xDFFF) {
                result.Error = { ERROR::CODE::Isolated_Surrogate, index };
                return result;
            } else {
                result.Scalars.push_back(static_cast<CODE_POINT>(first));
            }
        }
        return result;
    }

    for (std::size_t index = 0; index < units.size(); ++index) {
        const auto scalar = static_cast<CODE_POINT>(units[index]);
        if (!UNICODE::Is_Scalar(scalar)) {
            result.Error = {
                UNICODE::Is_Surrogate(scalar)
                    ? ERROR::CODE::Isolated_Surrogate
                    : ERROR::CODE::Out_Of_Range,
                index
            };
            return result;
        }
        result.Scalars.push_back(scalar);
    }
    return result;
}

RESULT MULTICODE::Encode(ENCODING target, std::u32string_view scalars)
{
    RESULT result;
    result.Scalars.assign(scalars.begin(), scalars.end());

    for (std::size_t index = 0; index < scalars.size(); ++index) {
        CODE_POINT scalar = scalars[index];
        if (!UNICODE::Is_Scalar(scalar)) {
            result.Error = { ERROR::CODE::Out_Of_Range, index };
            return result;
        }

        if (target == ENCODING::ASCII) {
            if (!UNICODE::Is_ASCII(scalar)) {
                result.Error = { ERROR::CODE::Unrepresentable, index };
                return result;
            }
            result.Units.push_back(static_cast<CODE_UNIT>(scalar));
        } else if (target == ENCODING::UTF8) {
            if (scalar <= 0x7F) {
                result.Units.push_back(scalar);
            } else if (scalar <= 0x7FF) {
                result.Units.push_back(0xC0 | (scalar >> 6));
                result.Units.push_back(0x80 | (scalar & 0x3F));
            } else if (scalar <= 0xFFFF) {
                result.Units.push_back(0xE0 | (scalar >> 12));
                result.Units.push_back(0x80 | ((scalar >> 6) & 0x3F));
                result.Units.push_back(0x80 | (scalar & 0x3F));
            } else {
                result.Units.push_back(0xF0 | (scalar >> 18));
                result.Units.push_back(0x80 | ((scalar >> 12) & 0x3F));
                result.Units.push_back(0x80 | ((scalar >> 6) & 0x3F));
                result.Units.push_back(0x80 | (scalar & 0x3F));
            }
        } else if (target == ENCODING::UTF16) {
            if (scalar <= 0xFFFF) {
                result.Units.push_back(scalar);
            } else {
                scalar -= 0x10000;
                result.Units.push_back(0xD800 | (scalar >> 10));
                result.Units.push_back(0xDC00 | (scalar & 0x3FF));
            }
        } else {
            result.Units.push_back(scalar);
        }
    }
    return result;
}

RESULT MULTICODE::Transcode(
    ENCODING source,
    ENCODING target,
    const std::vector<CODE_UNIT>& units
)
{
    RESULT decoded = Decode(source, units);
    if (!decoded) return decoded;
    return Encode(target, decoded.Scalars);
}

bool MULTICODE::Decode_UTF8_Text(
    std::string_view input,
    std::u32string& output,
    ERROR& error
)
{
    std::vector<CODE_UNIT> units;
    units.reserve(input.size());
    for (unsigned char unit : input) units.push_back(unit);
    RESULT result = Decode(ENCODING::UTF8, units);
    error = result.Error;
    output = std::move(result.Scalars);
    return !error;
}

bool MULTICODE::Encode_UTF8_Text(
    std::u32string_view input,
    std::string& output,
    ERROR& error
)
{
    RESULT result = Encode(ENCODING::UTF8, input);
    error = result.Error;
    output.clear();
    if (error) return false;
    output.reserve(result.Units.size());
    for (CODE_UNIT unit : result.Units) output.push_back(static_cast<char>(unit));
    return true;
}

} // namespace MULTICODE

namespace {

constexpr std::string_view Version = "1.0.0";

void help()
{
    std::cout
        << "multicode " << Version << " - multi-encoding conversion utility\n\n"
        << "Usage:\n"
        << "  multicode inspect <UTF-8-text>\n"
        << "  multicode encode <ascii|utf8|utf16|utf32> <UTF-8-text>\n"
        << "  multicode decode <ascii|utf8|utf16|utf32> <hex-unit> [...]\n"
        << "  multicode transcode <source> <target> <hex-unit> [...]\n"
        << "  multicode --help\n"
        << "  multicode --version\n";
}

int width_for(MULTICODE::ENCODING encoding) noexcept
{
    switch (encoding) {
        case MULTICODE::ENCODING::ASCII: return 2;
        case MULTICODE::ENCODING::UTF8:  return 2;
        case MULTICODE::ENCODING::UTF16: return 4;
        case MULTICODE::ENCODING::UTF32: return 8;
    }
    return 1;
}

void print_units(
    const std::vector<MULTICODE::CODE_UNIT>& units,
    MULTICODE::ENCODING encoding,
    char separator = ' '
)
{
    const int width = width_for(encoding);
    for (std::size_t index = 0; index < units.size(); ++index) {
        if (index != 0) std::cout << separator;
        std::cout << std::uppercase << std::hex << std::setfill('0')
                  << std::setw(width) << units[index] << std::dec;
    }
}

void print_error(const MULTICODE::ERROR& error)
{
    std::cerr << "multicode: "
              << MULTICODE::MULTICODE::Error_Name(error.Code)
              << " at unit " << error.Position << '\n';
}

bool parse_encoding(std::string_view name, MULTICODE::ENCODING& encoding)
{
    if (MULTICODE::MULTICODE::Parse_Encoding(name, encoding)) return true;
    std::cerr << "multicode: unknown encoding: " << name << '\n';
    return false;
}

bool parse_unit(std::string_view text, MULTICODE::CODE_UNIT& output)
{
    if (text.size() >= 2 && (text.substr(0, 2) == "0x" ||
                             text.substr(0, 2) == "0X" ||
                             text.substr(0, 2) == "U+" ||
                             text.substr(0, 2) == "u+")) text.remove_prefix(2);
    if (text.empty()) return false;

    std::uint32_t value = 0;
    const auto parsed = std::from_chars(
        text.data(), text.data() + text.size(), value, 16
    );
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return false;
    }
    output = value;
    return true;
}

bool parse_units(
    int begin,
    int count,
    char* arguments[],
    std::vector<MULTICODE::CODE_UNIT>& units
)
{
    for (int index = begin; index < count; ++index) {
        MULTICODE::CODE_UNIT unit = 0;
        if (!parse_unit(arguments[index], unit)) {
            std::cerr << "multicode: invalid hexadecimal unit: "
                      << arguments[index] << '\n';
            return false;
        }
        units.push_back(unit);
    }
    return true;
}

int encode(MULTICODE::ENCODING target, std::string_view text)
{
    std::u32string scalars;
    MULTICODE::ERROR error;
    if (!MULTICODE::MULTICODE::Decode_UTF8_Text(text, scalars, error)) {
        print_error(error); return 2;
    }
    auto result = MULTICODE::MULTICODE::Encode(target, scalars);
    if (!result) { print_error(result.Error); return 2; }
    print_units(result.Units, target);
    std::cout << '\n';
    return 0;
}

int decode(
    MULTICODE::ENCODING source,
    const std::vector<MULTICODE::CODE_UNIT>& units
)
{
    auto result = MULTICODE::MULTICODE::Decode(source, units);
    if (!result) { print_error(result.Error); return 2; }
    std::string text;
    MULTICODE::ERROR error;
    if (!MULTICODE::MULTICODE::Encode_UTF8_Text(result.Scalars, text, error)) {
        print_error(error); return 2;
    }
    std::cout << text << '\n';
    return 0;
}

int inspect(std::string_view text)
{
    std::u32string scalars;
    MULTICODE::ERROR error;
    if (!MULTICODE::MULTICODE::Decode_UTF8_Text(text, scalars, error)) {
        print_error(error); return 2;
    }
    for (MULTICODE::CODE_POINT scalar : scalars) {
        const std::u32string one(1, scalar);
        auto utf8 = MULTICODE::MULTICODE::Encode(MULTICODE::ENCODING::UTF8, one);
        auto utf16 = MULTICODE::MULTICODE::Encode(MULTICODE::ENCODING::UTF16, one);
        auto utf32 = MULTICODE::MULTICODE::Encode(MULTICODE::ENCODING::UTF32, one);

        std::cout << "U+" << std::uppercase << std::hex << std::setfill('0')
                  << std::setw(4) << static_cast<std::uint32_t>(scalar) << std::dec
                  << "  ASCII=" << (MULTICODE::UNICODE::Is_ASCII(scalar) ? "yes" : "no")
                  << "  PLANE=" << static_cast<int>(MULTICODE::UNICODE::Plane(scalar))
                  << "  UTF8=";
        print_units(utf8.Units, MULTICODE::ENCODING::UTF8, ',');
        std::cout << "  UTF16=";
        print_units(utf16.Units, MULTICODE::ENCODING::UTF16, ',');
        std::cout << "  UTF32=";
        print_units(utf32.Units, MULTICODE::ENCODING::UTF32, ',');
        std::cout << '\n';
    }
    return 0;
}

} // namespace

int main(int argument_count, char* arguments[])
{
    if (argument_count == 1) { help(); return 0; }
    const std::string_view command = arguments[1];
    if (command == "--help" || command == "-h" || command == "help") {
        help(); return 0;
    }
    if (command == "--version" || command == "-v") {
        std::cout << "multicode " << Version << '\n'; return 0;
    }
    if (command == "inspect" && argument_count == 3) {
        return inspect(arguments[2]);
    }
    if (command == "encode" && argument_count == 4) {
        MULTICODE::ENCODING encoding;
        if (!parse_encoding(arguments[2], encoding)) return 2;
        return encode(encoding, arguments[3]);
    }
    if (command == "decode" && argument_count >= 4) {
        MULTICODE::ENCODING encoding;
        std::vector<MULTICODE::CODE_UNIT> units;
        if (!parse_encoding(arguments[2], encoding) ||
            !parse_units(3, argument_count, arguments, units)) return 2;
        return decode(encoding, units);
    }
    if (command == "transcode" && argument_count >= 5) {
        MULTICODE::ENCODING source;
        MULTICODE::ENCODING target;
        std::vector<MULTICODE::CODE_UNIT> units;
        if (!parse_encoding(arguments[2], source) ||
            !parse_encoding(arguments[3], target) ||
            !parse_units(4, argument_count, arguments, units)) return 2;
        auto result = MULTICODE::MULTICODE::Transcode(source, target, units);
        if (!result) { print_error(result.Error); return 2; }
        print_units(result.Units, target);
        std::cout << '\n';
        return 0;
    }

    std::cerr << "multicode: invalid command or argument count\n"
              << "Try 'multicode --help' for usage.\n";
    return 2;
}
