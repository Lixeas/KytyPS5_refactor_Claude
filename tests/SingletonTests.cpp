#include "common/singleton.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

constexpr int THREAD_COUNT = 16;

// Payload that records how many times it gets constructed. A correct Instance() constructs it
// exactly once, however many threads race into it at the same time.
struct Counted {
	Counted() { constructed.fetch_add(1, std::memory_order_relaxed); }

	static std::atomic<int> constructed;
};

std::atomic<int> Counted::constructed {0};

void Check(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "SingletonTests: FAILED: %s\n", what);
		std::exit(1);
	}
}

// Regression test. Instance() used to be:
//
//     if (!g_m_instance) { g_m_instance = malloc(sizeof(T)); new (g_m_instance) T; }
//
// on a plain non-atomic pointer, so two threads could both see null, both construct, and hand two
// different objects to two different callers. Singleton<CContext> in libs/libC.cpp is reachable
// that way: guest code creates it lazily from atexit()/__cxa_atexit(), on any thread.
//
// Being a race, this reproduces probabilistically rather than deterministically -- the start gate
// is what makes the threads collide often enough to matter. It passes deterministically against a
// correct Instance(), which is what guards the fix.
void TestConcurrentInstanceConstructsOnce() {
	std::atomic<bool>        start {false};
	std::vector<Counted*>    seen(THREAD_COUNT, nullptr);
	std::vector<std::thread> threads;
	threads.reserve(THREAD_COUNT);

	for (int i = 0; i < THREAD_COUNT; i++) {
		threads.emplace_back([&start, &seen, i]() {
			while (!start.load(std::memory_order_acquire)) {
				std::this_thread::yield();
			}
			seen[i] = Common::Singleton<Counted>::Instance();
		});
	}

	start.store(true, std::memory_order_release);

	for (auto& t: threads) {
		t.join();
	}

	Check(Counted::constructed.load(std::memory_order_relaxed) == 1,
	      "payload was constructed more than once");

	for (int i = 0; i < THREAD_COUNT; i++) {
		Check(seen[i] != nullptr, "Instance() returned null");
		Check(seen[i] == seen[0], "Instance() handed different objects to different threads");
	}
}

void TestInstanceIsStable() {
	Check(Common::Singleton<Counted>::Instance() == Common::Singleton<Counted>::Instance(),
	      "Instance() is not stable across calls");
	Check(Counted::constructed.load(std::memory_order_relaxed) == 1,
	      "a later call constructed another payload");
}

} // namespace

int main() {
	TestConcurrentInstanceConstructsOnce();
	TestInstanceIsStable();

	std::puts("SingletonTests: all cases passed");
	return 0;
}
