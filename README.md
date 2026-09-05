# Multicode Utility

## Overview

`multicode` is a dependency-free C++17 encoding coordinator and command-line
utility. It translates ASCII, UTF-8, UTF-16, and UTF-32 through one canonical
Unicode-scalar pipeline.

The architectural contract is:

```text
source code units → validated Unicode scalars → target code units
```

This prevents direct pairwise converters from multiplying across the system.
Every encoding implements one path into and one path out of the shared scalar
representation.

## Structure

```text
multicode/
├── Makefile
├── multicode.cpp
├── multicode.hpp
├── multicode.md
└── unicode.hpp
```

| File | Responsibility |
| --- | --- |
| `unicode.hpp` | Unicode scalar limits, surrogate rules, ASCII range, and planes |
| `multicode.hpp` | Encoding, result, error, decode, encode, and transcode interfaces |
| `multicode.cpp` | Codec implementations and invokable command interface |
| `Makefile` | Compilation, testing, installation, and cleanup |
| `multicode.md` | Operational and developer reference |

## Supported encodings

| Encoding | Unit width | Units per scalar |
| --- | ---: | ---: |
| ASCII | 7 significant bits | 1 |
| UTF-8 | 8 bits | 1–4 |
| UTF-16 | 16 bits | 1–2 |
| UTF-32 | 32 bits | 1 |

Aliases accepted by the command interface include `utf8`/`utf-8`,
`utf16`/`utf-16`, and `utf32`/`utf-32`.

## Requirements

- A C++17-compatible compiler
- GNU Make
- A POSIX-compatible environment for installation

On Debian-based systems:

```bash
sudo apt update
sudo apt install g++ make
```

## Build and validate

```bash
make
make check
```

Additional targets:

```bash
make debug
make rebuild
make clean
```

The integrated checks cover UTF-8 and UTF-16 encoding, UTF-16 decoding,
UTF-8-to-UTF-32 transcoding, supplementary Unicode planes, and surrogate-pair
generation.

## Installation

```bash
sudo make install
multicode --version
```

The default executable path is `/usr/local/bin/multicode`.

Custom prefix:

```bash
sudo make install PREFIX=/opt/multicode
```

Staged package installation:

```bash
make install DESTDIR=/tmp/multicode-package PREFIX=/usr
```

Uninstall:

```bash
sudo make uninstall
```

## Inspect text

```bash
multicode inspect "A€😀"
```

Each scalar is reported with:

- Unicode code-point notation
- ASCII compatibility
- Unicode plane number
- UTF-8 code units
- UTF-16 code units
- UTF-32 code unit

Example record:

```text
U+1F600  ASCII=no  PLANE=1  UTF8=F0,9F,98,80  UTF16=D83D,DE00  UTF32=0001F600
```

## Encode UTF-8 text

Convert terminal UTF-8 text into target code units.

ASCII:

```bash
multicode encode ascii "ABC"
```

```text
41 42 43
```

UTF-8:

```bash
multicode encode utf8 "A€"
```

```text
41 E2 82 AC
```

UTF-16:

```bash
multicode encode utf16 "A😀"
```

```text
0041 D83D DE00
```

UTF-32:

```bash
multicode encode utf32 "A😀"
```

```text
00000041 0001F600
```

ASCII encoding rejects any scalar above `U+007F` rather than replacing or
silently truncating it.

## Decode code units

Code-unit arguments are interpreted as hexadecimal. The `0x` prefix is
optional.

Decode ASCII:

```bash
multicode decode ascii 41 42 43
```

Decode UTF-8:

```bash
multicode decode utf8 41 E2 82 AC
```

Decode UTF-16:

```bash
multicode decode utf16 0041 D83D DE00
```

Decode UTF-32:

```bash
multicode decode utf32 00000041 0001F600
```

Decoded output is emitted as UTF-8 text for the terminal and pipeline.

## Transcode code units

Transform source units directly into target units:

```bash
multicode transcode utf8 utf16 41 E2 82 AC
```

```text
0041 20AC
```

Supplementary-plane example:

```bash
multicode transcode utf16 utf8 D83D DE00
```

```text
F0 9F 98 80
```

Other valid combinations include ASCII to UTF-32, UTF-32 to UTF-8, and UTF-16
to ASCII when every scalar is representable in ASCII.

## Exit status

| Status | Meaning |
| ---: | --- |
| `0` | Operation completed successfully |
| `2` | Invalid command, encoding, unit, scalar, or sequence |

Results use standard output and diagnostics use standard error.

## Library API

```cpp
#include "multicode.hpp"
```

### Decode

```cpp
std::vector<MULTICODE::CODE_UNIT> units {
    0x41, 0xE2, 0x82, 0xAC
};

MULTICODE::RESULT result = MULTICODE::MULTICODE::Decode(
    MULTICODE::ENCODING::UTF8,
    units
);
```

Successful decoding places Unicode scalar values in:

```cpp
result.Scalars
```

### Encode

```cpp
std::u32string scalars { U'A', U'€' };

MULTICODE::RESULT result = MULTICODE::MULTICODE::Encode(
    MULTICODE::ENCODING::UTF16,
    scalars
);
```

Encoded units are available in:

```cpp
result.Units
```

### Transcode

```cpp
MULTICODE::RESULT result = MULTICODE::MULTICODE::Transcode(
    MULTICODE::ENCODING::UTF8,
    MULTICODE::ENCODING::UTF32,
    units
);
```

Transcoding validates the source fully before creating target units.

### UTF-8 text adapters

```cpp
std::u32string scalars;
MULTICODE::ERROR error;

bool decoded = MULTICODE::MULTICODE::Decode_UTF8_Text(
    "Text",
    scalars,
    error
);
```

```cpp
std::string text;

bool encoded = MULTICODE::MULTICODE::Encode_UTF8_Text(
    scalars,
    text,
    error
);
```

These adapters bridge ordinary UTF-8 `std::string` values and the canonical
`std::u32string` scalar representation.

## Result and error model

`MULTICODE::RESULT` contains:

```cpp
result.Scalars
result.Units
result.Error
```

An operation succeeds when the result evaluates to `true`.

```cpp
if (!result) {
    auto code = result.Error.Code;
    auto position = result.Error.Position;
}
```

Errors distinguish:

- Unknown encodings
- Invalid code units
- Invalid UTF-8 lead or continuation units
- Truncated sequences
- Overlong UTF-8 sequences
- Isolated UTF-16 surrogates
- Out-of-range Unicode values
- Scalars unavailable in a target encoding

`Position` identifies the failing source-unit index.

## Unicode model

```cpp
#include "unicode.hpp"
```

Core operations:

```cpp
MULTICODE::UNICODE::Is_Scalar(value)
MULTICODE::UNICODE::Is_Surrogate(value)
MULTICODE::UNICODE::Is_ASCII(value)
MULTICODE::UNICODE::Plane(value)
```

A Unicode scalar is within `U+0000` through `U+10FFFF`, excluding the surrogate
range `U+D800` through `U+DFFF`.

The plane is derived from the high sixteen bits of the scalar. Plane `0` is the
Basic Multilingual Plane; plane `1` is the Supplementary Multilingual Plane.

## Byte-order boundary

Multicode works with logical code units. It does not silently assign byte order
to UTF-16 or UTF-32 units. File and network serialization should add an explicit
little-endian or big-endian layer, including a declared BOM policy where
required.

This separation keeps these concerns distinct:

```text
Unicode semantics
Code-unit encoding
Byte serialization
Transport or storage
```

## Integration direction

Multicode can serve as the canonical coding boundary for later tokenizer,
parser, syntax, symbol-table, or cross-language transformation layers. A caller
decodes external data once, processes Unicode scalars internally, and encodes
only at an explicit output boundary.

## Version

```text
1.0.0
```
