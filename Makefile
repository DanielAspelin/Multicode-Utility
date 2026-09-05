CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

TARGET := multicode
SOURCE := multicode.cpp
HEADERS := multicode.hpp unicode.hpp
OBJECT := multicode.o
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

.PHONY: all debug check test install uninstall clean rebuild

all: $(TARGET)

$(TARGET): $(OBJECT)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJECT): $(SOURCE) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJECT:.o=.d)

debug: CXXFLAGS := -std=c++17 -O0 -g3 -Wall -Wextra -Wpedantic
debug: clean $(TARGET)

check: $(TARGET)
	./$(TARGET) encode utf8 'A€' | grep -qx '41 E2 82 AC'
	./$(TARGET) encode utf16 'A€' | grep -qx '0041 20AC'
	./$(TARGET) decode utf16 0041 20AC | grep -qx 'A€'
	./$(TARGET) transcode utf8 utf32 41 E2 82 AC | grep -qx '00000041 000020AC'
	./$(TARGET) inspect '😀' | grep -qx 'U+1F600  ASCII=no  PLANE=1  UTF8=F0,9F,98,80  UTF16=D83D,DE00  UTF32=0001F600'

test: check

install: $(TARGET)
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"

clean:
	rm -f $(TARGET) $(OBJECT) $(OBJECT:.o=.d)

rebuild: clean all
