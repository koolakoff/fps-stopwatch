#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../src/stopwatch.h"
#include <cassert>
#include <iostream>
int main() {
    Stopwatch stopwatch(10000000);
    assert(stopwatch.milliseconds(90000000) == 0);
    stopwatch.toggle(10000000);
    assert(stopwatch.milliseconds(10009999) == 0);
    assert(stopwatch.milliseconds(10010000) == 1);
    // No frame updates for over a minute: elapsed time must still be correct.
    assert(stopwatch.milliseconds(1234830000) == 122483);
    stopwatch.toggle(1234830000);
    assert(stopwatch.milliseconds(9000000000) == 122483);
    stopwatch.toggle(9000000000);
    assert(stopwatch.milliseconds(9010000000) == 123483);
    assert(formatTime(0) == L"00:00:000");
    assert(formatTime(137483) == L"02:17:483");
    assert(formatTime(59999) == L"00:59:999");
    assert(formatTime(60000) == L"01:00:000");
    assert(formatTime(6000000) == L"100:00:000");
    stopwatch.reset(9010000000);
    assert(stopwatch.running());
    assert(stopwatch.milliseconds(9010000000) == 0);
    assert(stopwatch.milliseconds(9020000000) == 1000);
    stopwatch.toggle(9020000000);
    stopwatch.reset(9030000000);
    assert(!stopwatch.running());
    assert(stopwatch.milliseconds(9990000000) == 0);
    stopwatch.toggle(9990000000);
    assert(stopwatch.milliseconds(10000000000) == 1000);
    std::cout << "Stopwatch tests passed\n";
}
