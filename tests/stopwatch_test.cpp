#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/stopwatch.h"
#include <cassert>
#include <iostream>
int main() {
    Stopwatch timer(10000000);
    assert(timer.milliseconds(90000000) == 0);
    timer.toggle(10000000);
    assert(timer.milliseconds(10009999) == 0);
    assert(timer.milliseconds(10010000) == 1);
    // No frame updates for over a minute: elapsed time must still be correct.
    assert(timer.milliseconds(1234830000) == 122483);
    timer.toggle(1234830000);
    assert(timer.milliseconds(9000000000) == 122483);
    timer.toggle(9000000000);
    assert(timer.milliseconds(9010000000) == 123483);
    assert(formatTime(0) == L"00:00:000");
    assert(formatTime(137483) == L"02:17:483");
    assert(formatTime(59999) == L"00:59:999");
    assert(formatTime(60000) == L"01:00:000");
    assert(formatTime(6000000) == L"100:00:000");
    timer.reset(9010000000);
    assert(timer.running());
    assert(timer.milliseconds(9010000000) == 0);
    assert(timer.milliseconds(9020000000) == 1000);
    timer.toggle(9020000000);
    timer.reset(9030000000);
    assert(!timer.running());
    assert(timer.milliseconds(9990000000) == 0);
    timer.toggle(9990000000);
    assert(timer.milliseconds(10000000000) == 1000);
    std::cout << "Stopwatch tests passed\n";
}
