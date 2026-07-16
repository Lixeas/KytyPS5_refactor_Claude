#include "graphics/shader/recompiler/MemoryOps.h"
#include "graphics/shader/recompiler/ShaderDecoder.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using namespace Libs::Graphics::ShaderRecompiler::Decoder;

void Check(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "ShaderMemoryDecodeTests: FAILED: %s\n", what);
		std::exit(1);
	}
}

// Field layouts are from the RDNA 2 ISA reference (AMD doc 70648), chapter 13 "Microcode Formats".
//
// MUBUF word0: OFFSET [11:0], OFFEN [12], IDXEN [13], GLC [14], DLC [15], LDS [16], OP [25:18],
//              ENCODING [31:26] = 111000.
// MUBUF word1: VADDR [39:32], VDATA [47:40], SRSRC [52:48], SLC [54], TFE [55], SOFFSET [63:56]
//              -- i.e. bits [7:0], [15:8], [20:16], [22], [23], [31:24] of the second dword.
constexpr uint32_t MubufWord0(uint32_t opcode, bool lds = false) {
	return (0x38u << 26u) | ((opcode & 0xffu) << 18u) | (lds ? (1u << 16u) : 0u) | (1u << 12u);
}

constexpr uint32_t MubufWord1(bool tfe = false) {
	return (128u << 24u) | (tfe ? (1u << 23u) : 0u);
}

// MTBUF word0: OP [18:16] (low 3 bits), DFMT [22:19], NFMT [25:23], ENCODING [31:26] = 111010.
// MTBUF word1: OP bit 3 lives at [53], i.e. bit 21 of the second dword. TFE [55] -> bit 23.
constexpr uint32_t MtbufWord0(uint32_t opcode) {
	return (0x3au << 26u) | ((opcode & 0x7u) << 16u) | (14u << 19u) | (7u << 23u) | (1u << 12u);
}

constexpr uint32_t MtbufWord1(uint32_t opcode, bool tfe = false) {
	return (((opcode >> 3u) & 1u) << 21u) | (128u << 24u) | (tfe ? (1u << 23u) : 0u);
}

Instruction DecodeOne(uint32_t word0, uint32_t word1, bool mtbuf = false) {
	const std::array<uint32_t, 2> code {word0, word1};
	Instruction                   inst;
	std::string                   error;
	const bool                    ok = mtbuf ? DecodeMtbuf(0, code, 0, &inst, &error)
	                                         : DecodeMubuf(0, code, 0, &inst, &error);
	Check(ok, "decoder reported a hard failure on a well-formed instruction");
	return inst;
}

// BUFFER_LOAD_DWORD is opcode 0x0c and is implemented, so it is the baseline: without modifiers it
// must decode cleanly. If this breaks, the tests below prove nothing.
void TestPlainBufferLoadDecodes() {
	const auto inst = DecodeOne(MubufWord0(0x0cu), MubufWord1());
	Check(inst.family == Family::MUBUF, "family is not MUBUF");
	Check(inst.opcode_id == 0x0cu, "opcode_id is not 0x0c");
	Check(inst.opcode == Opcode::BufferLoadDword, "0x0c did not decode to BufferLoadDword");
}

// Regression test. MUBUF OP is [25:18] -- 8 bits. Opcodes 128-135 are the BUFFER_*_FORMAT_D16_*
// variants, which this emulator does not implement. The decoder reads all 8 bits, so they must
// arrive as themselves and be reported unsupported -- not truncated to 0-7, which are implemented
// opcodes and would have been executed as the wrong instruction.
void TestHighMubufOpcodesAreNotTruncated() {
	struct Case {
		uint32_t opcode;
		uint32_t would_truncate_to;
	};
	// 128 -> BUFFER_LOAD_FORMAT_X (0), 132 -> BUFFER_STORE_FORMAT_X (4), 135 -> ...XYZW (7).
	constexpr Case CASES[] = {{128u, 0u}, {129u, 1u}, {132u, 4u}, {135u, 7u}};

	for (const auto& c: CASES) {
		const auto inst = DecodeOne(MubufWord0(c.opcode), MubufWord1());
		Check(inst.opcode_id == c.opcode, "MUBUF opcode was truncated to 7 bits");
		Check(inst.opcode_id != c.would_truncate_to, "MUBUF opcode decoded as the truncated value");
		Check(inst.opcode == Opcode::Unsupported,
		      "an unimplemented D16 opcode decoded to a supported instruction");
	}
}

// Regression test. The LDS bit sends the loaded data to LDS instead of a VGPR, and TFE adds a
// return VGPR. DecodeMubuf used to extract neither, so a shader setting them was recompiled as a
// plain VGPR load: wrong results, no diagnostic. DecodeFlat has always rejected its own LDS bit;
// this asserts MUBUF now holds the same contract.
void TestMubufLdsAndTfeAreRejected() {
	const auto lds = DecodeOne(MubufWord0(0x0cu, true), MubufWord1());
	Check(lds.opcode == Opcode::Unsupported, "MUBUF with LDS=1 decoded as a plain load");

	const auto tfe = DecodeOne(MubufWord0(0x0cu), MubufWord1(true));
	Check(tfe.opcode == Opcode::Unsupported, "MUBUF with TFE=1 decoded as a plain load");

	const auto both = DecodeOne(MubufWord0(0x0cu, true), MubufWord1(true));
	Check(both.opcode == Opcode::Unsupported, "MUBUF with LDS=1 and TFE=1 decoded as a plain load");
}

// TBUFFER_LOAD_FORMAT_X is opcode 0, implemented. Opcode 8 is TBUFFER_LOAD_FORMAT_D16_X, which is
// not -- and its high bit lives in the second dword, so this also covers the split opcode field.
void TestMtbufOpcodeSplitAcrossWords() {
	const auto plain = DecodeOne(MtbufWord0(0u), MtbufWord1(0u), true);
	Check(plain.family == Family::MTBUF, "family is not MTBUF");
	Check(plain.opcode_id == 0u, "MTBUF opcode 0 did not decode as 0");
	Check(plain.opcode == Opcode::TBufferLoadFormatX, "opcode 0 is not TBufferLoadFormatX");

	const auto d16 = DecodeOne(MtbufWord0(8u), MtbufWord1(8u), true);
	Check(d16.opcode_id == 8u, "MTBUF opcode bit 3 (word1 bit 21) was not decoded");
	Check(d16.opcode == Opcode::Unsupported, "unimplemented MTBUF D16 opcode decoded as supported");
}

void TestMtbufTfeIsRejected() {
	const auto tfe = DecodeOne(MtbufWord0(0u), MtbufWord1(0u, true), true);
	Check(tfe.opcode == Opcode::Unsupported, "MTBUF with TFE=1 decoded as a plain load");
}

} // namespace

int main() {
	TestPlainBufferLoadDecodes();
	TestHighMubufOpcodesAreNotTruncated();
	TestMubufLdsAndTfeAreRejected();
	TestMtbufOpcodeSplitAcrossWords();
	TestMtbufTfeIsRejected();

	std::puts("ShaderMemoryDecodeTests: all cases passed");
	return 0;
}
