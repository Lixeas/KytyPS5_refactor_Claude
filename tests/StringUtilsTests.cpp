#include "common/stringUtils.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Check(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "StringUtilsTests: FAILED: %s\n", what);
		std::exit(1);
	}
}

void CheckUtf8(const char16_t* input, const char* expected, const char* what) {
	Check(Common::Utf16ToUtf8(input) == std::string(expected), what);
}

// Inputs are spelled as numeric char16_t arrays and expectations as hex escapes, so this file stays
// pure ASCII. Literal characters would make the test depend on its own source encoding, which is
// the one thing it must not do.
void TestUtf16ToUtf8ConvertsWellFormedInput() {
	const char16_t empty[]  = {0};
	const char16_t ascii[]  = {u'a', u'b', u'c', 0};
	const char16_t u_007f[] = {0x007f, 0};                 // last 1-byte codepoint
	const char16_t u_0080[] = {0x0080, 0};                 // first 2-byte codepoint
	const char16_t u_00e9[] = {0x00e9, 0};                 // 'e' acute
	const char16_t u_07ff[] = {0x07ff, 0};                 // last 2-byte codepoint
	const char16_t u_0800[] = {0x0800, 0};                 // first 3-byte codepoint
	const char16_t u_20ac[] = {0x20ac, 0};                 // euro sign
	const char16_t u_ffff[] = {0xffff, 0};                 // last 3-byte codepoint
	const char16_t u_10000[] = {0xd800, 0xdc00, 0};        // first 4-byte codepoint
	const char16_t u_1f600[] = {0xd83d, 0xde00, 0};        // grinning face
	const char16_t u_10ffff[] = {0xdbff, 0xdfff, 0};       // last valid codepoint
	const char16_t mixed[] = {u'a', 0x00e9, 0x20ac, 0xd83d, 0xde00, u'z', 0};

	CheckUtf8(empty, "", "empty string");
	CheckUtf8(ascii, "abc", "ascii");

	// One codepoint per UTF-8 output length: 1, 2, 3 and 4 bytes.
	CheckUtf8(u_00e9, "\xc3\xa9", "2-byte codepoint");
	CheckUtf8(u_20ac, "\xe2\x82\xac", "3-byte codepoint");
	CheckUtf8(u_1f600, "\xf0\x9f\x98\x80", "4-byte codepoint (surrogate pair)");

	// Both sides of every length change.
	CheckUtf8(u_007f, "\x7f", "last 1-byte codepoint");
	CheckUtf8(u_0080, "\xc2\x80", "first 2-byte codepoint");
	CheckUtf8(u_07ff, "\xdf\xbf", "last 2-byte codepoint");
	CheckUtf8(u_0800, "\xe0\xa0\x80", "first 3-byte codepoint");
	CheckUtf8(u_ffff, "\xef\xbf\xbf", "last 3-byte codepoint");
	CheckUtf8(u_10000, "\xf0\x90\x80\x80", "first 4-byte codepoint");
	CheckUtf8(u_10ffff, "\xf4\x8f\xbf\xbf", "last valid codepoint");

	CheckUtf8(mixed, "\x61\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\x7a", "mixed string");
}

// Regression test. Utf16ToUtf8 used to be std::wstring_convert::to_bytes, which throws
// std::range_error on malformed input. Nothing in this codebase catches, and the Clang/GCC build
// paths set -fno-exceptions, so this was a process kill either way.
//
// It matters because the only caller -- ReadGuestWideCString in libs/libAmpr.cpp -- fills its
// buffer straight from guest memory, so the guest picks these code units. Returning empty is what
// lets that caller report failure through the bool it already returns.
void TestUtf16ToUtf8RejectsMalformedInput() {
	const char16_t lone_high[]       = {0xd800, 0};
	const char16_t lone_low[]        = {0xdc00, 0};
	const char16_t high_then_ascii[] = {0xd800, u'a', 0};
	const char16_t high_then_high[]  = {0xd800, 0xd800, 0};
	const char16_t low_then_high[]   = {0xdc00, 0xd800, 0};
	const char16_t valid_then_lone[] = {u'a', 0xd800, 0};

	Check(Common::Utf16ToUtf8(lone_high).empty(), "unpaired high surrogate at end of string");
	Check(Common::Utf16ToUtf8(lone_low).empty(), "lone low surrogate");
	Check(Common::Utf16ToUtf8(high_then_ascii).empty(), "high surrogate followed by ascii");
	Check(Common::Utf16ToUtf8(high_then_high).empty(), "high surrogate followed by high surrogate");
	Check(Common::Utf16ToUtf8(low_then_high).empty(), "surrogates in the wrong order");
	Check(Common::Utf16ToUtf8(valid_then_lone).empty(), "unpaired surrogate after valid text");
}

void TestUtf16ToUtf8HandlesNull() {
	Check(Common::Utf16ToUtf8(nullptr).empty(), "null input");
}

} // namespace

int main() {
	TestUtf16ToUtf8ConvertsWellFormedInput();
	TestUtf16ToUtf8RejectsMalformedInput();
	TestUtf16ToUtf8HandlesNull();

	std::puts("StringUtilsTests: all cases passed");
	return 0;
}
