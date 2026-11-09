#include "STInput.h"
#include <atomic>
#include <new>
struct STInput { std::atomic<uint32_t> buttons{0}; };
extern "C" {
STInput *st_input_create(void) { return new (std::nothrow) STInput; }
void st_input_destroy(STInput *input) { delete input; }
void st_input_set(STInput *input, int32_t button, int32_t pressed) {
    if (!input || button < 0 || button >= 5) return;
    const uint32_t bit = uint32_t(1) << button;
    if (pressed) input->buttons.fetch_or(bit);
    else input->buttons.fetch_and(~bit);
}
uint32_t st_input_snapshot(const STInput *input) {
    return input ? input->buttons.load() : 0;
}
void st_input_reset(STInput *input) { if (input) input->buttons.store(0); }
}
