#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original input adapter, not an X-Ray engine implementation. */
typedef struct STInput STInput;
STInput *st_input_create(void);
void st_input_destroy(STInput *input);
/* IDs 0..4: left, forward, right, aim, fire. */
void st_input_set(STInput *input, int32_t button, int32_t pressed);
uint32_t st_input_snapshot(const STInput *input);
void st_input_reset(STInput *input);
#ifdef __cplusplus
}
#endif
