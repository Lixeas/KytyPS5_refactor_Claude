#ifndef KYTY_COMMON_ASSERT_H_
#define KYTY_COMMON_ASSERT_H_

#include "common/common.h"
#include "common/logging/log.h"

#include <cstdlib>
#include <string_view>

namespace Common {

// The handlers report and return; DbgExit is the one that halts. Marking it [[noreturn]] is what
// lets the compiler see the EXIT macros below as terminating, which the previous
// analyzer_noreturn attribute never did -- that one only ever spoke to Clang's static analyzer,
// leaving codegen to believe every EXIT() could fall through.
int DbgExitHandler(char const* file, int line, std::string_view text);
int DbgExitHandler(char const* file, int line, fmt::text_style style, std::string_view text);
int DbgExitIfHandler(char const* expr, char const* file, int line);
int DbgNotImplementedHandler(char const* expr, char const* file, int line);

[[noreturn]] void DbgExit(int status);

} // namespace Common

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS || KYTY_PLATFORM == KYTY_PLATFORM_LINUX
#define EXIT_HALT() (Common::DbgExit(321), 1)
#else
#define EXIT_HALT() (std::_Exit(321), 1)
#endif

#ifndef KYTY_FINAL
#define EXIT_IF(x)                                                                                 \
	do {                                                                                           \
		if (x) {                                                                                   \
			(void)Common::DbgExitIfHandler(#x, __FILE__, __LINE__);                                \
			(void)EXIT_HALT();                                                                     \
		}                                                                                          \
	} while (0)
#else
#define EXIT_IF(x)                                                                                 \
	do {                                                                                           \
		constexpr bool kyty_exit_if_disabled = false && (x);                                       \
		(void)kyty_exit_if_disabled;                                                               \
	} while (0)
#endif

// EXIT_HALT() is reached unconditionally, so these expand to something the compiler can prove
// never returns. The reporting call used to gate it behind &&, which meant every caller ending in
// EXIT() was a non-void function falling off its end -- undefined behaviour, and 49 -Wreturn-type
// warnings.
#define EXIT(...)                                                                                  \
	do {                                                                                           \
		(void)Common::DbgExitHandler(__FILE__, __LINE__, ::fmt::sprintf(__VA_ARGS__));             \
		(void)EXIT_HALT();                                                                         \
	} while (0)

#define EXIT_COLOR(style, ...)                                                                     \
	do {                                                                                           \
		(void)Common::DbgExitHandler(__FILE__, __LINE__, (style), ::fmt::sprintf(__VA_ARGS__));    \
		(void)EXIT_HALT();                                                                         \
	} while (0)

#define EXIT_NOT_IMPLEMENTED(x)                                                                    \
	do {                                                                                           \
		if (x) {                                                                                   \
			(void)Common::DbgNotImplementedHandler(#x, __FILE__, __LINE__);                        \
			(void)EXIT_HALT();                                                                     \
		}                                                                                          \
	} while (0)
#define KYTY_NOT_IMPLEMENTED EXIT_NOT_IMPLEMENTED(true)

#endif /* KYTY_COMMON_ASSERT_H_ */
