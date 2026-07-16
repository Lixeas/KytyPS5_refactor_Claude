#ifndef KYTY_COMMON_SINGLETON_H_
#define KYTY_COMMON_SINGLETON_H_

#include "common/common.h" // IWYU pragma: keep

namespace Common {

template <class T>
class Singleton {
public:
	// The instance is deliberately never freed: guest threads may still reach a singleton while the
	// emulator is tearing down, and shutdown goes through std::_Exit anyway, so no destructor would
	// run even if one were registered.
	static T* Instance() {
		static T* instance = new T;
		return instance;
	}

	KYTY_CLASS_NO_COPY(Singleton);

protected:
	Singleton();
	~Singleton();
};

} // namespace Common

#endif /* KYTY_COMMON_SINGLETON_H_ */
