// Validates chapter start edge cases (non-zero first start, duration derivation).
#include <iostream>
#include <sstream>
#include <vector>

#include "chapter_timing.hpp"
#include "logging.hpp"

using namespace chapterforge;

struct Sample {
    uint32_t start_ms;
};

// Helper to capture stderr while a function runs.
template <typename Fn>
std::string capture_stderr(Fn &&fn) {
    std::ostringstream oss;
    auto *old_buf = std::cerr.rdbuf(oss.rdbuf());
    fn();
    std::cerr.rdbuf(old_buf);
    return oss.str();
}

int main() {
    // 1) First chapter not at 0 should emit a warning.
    set_log_verbosity(LogVerbosity::Warn);
    std::vector<Sample> non_zero{{1000}, {6000}};
    auto log_text = capture_stderr([&]() { derive_durations_ms_from_starts(non_zero); });
    if (log_text.find("first chapter start_ms") == std::string::npos) {
        std::cerr << "expected warning for non-zero first chapter, got: " << log_text << "\n";
        return 1;
    }

    // 2) Durations should honor the supplied start times (no implicit zeroing).
    // Starts: 3000ms, 8000ms, total 10000ms -> durations {5000, 2000}.
    std::vector<Sample> samples{{3000}, {8000}};
    auto durations = derive_durations_ms_from_starts(samples, 10000);
    if (durations.size() != 2 || durations[0] != 5000 || durations[1] != 2000) {
        std::cerr << "unexpected durations; got [" << (durations.empty() ? 0 : durations[0])
                  << ", " << (durations.size() > 1 ? durations[1] : 0) << "]\n";
        return 1;
    }

    std::cout << "chapter_start_rules OK\n";
    return 0;
}
