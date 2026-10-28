#pragma once
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

// Clock ticks are supplied explicitly, so pause/resume and dropped-frame behavior
// can be tested independently of the renderer and the Windows scheduler.
class Stopwatch {
public:
    explicit Stopwatch(std::int64_t frequency) : frequency_(frequency) {}
    void toggle(std::int64_t now) {
        if (running_) elapsed_ += now - started_;
        else started_ = now;
        running_ = !running_;
    }
    std::int64_t milliseconds(std::int64_t now) const {
        const auto ticks = elapsed_ + (running_ ? now - started_ : 0);
        return (ticks / frequency_) * 1000 + (ticks % frequency_) * 1000 / frequency_;
    }
    bool running() const { return running_; }
    void reset(std::int64_t now) { elapsed_ = 0; started_ = now; }
private:
    std::int64_t frequency_, elapsed_ = 0, started_ = 0;
    bool running_ = false;
};

inline std::wstring formatTime(std::int64_t ms) {
    std::wostringstream out;
    out << std::setfill(L'0') << std::setw(2) << ms / 60000 << L':'
        << std::setw(2) << (ms / 1000) % 60 << L':' << std::setw(3) << ms % 1000;
    return out.str();
}
