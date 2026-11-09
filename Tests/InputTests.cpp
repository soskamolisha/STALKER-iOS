#include "../Sources/Native/STInput.h"
#include <cassert>
int main() {
 auto *s=st_input_create(); assert(s);
 st_input_set(s,1,1); st_input_set(s,4,1);
 assert(st_input_snapshot(s)==18);
 st_input_set(s,1,1); st_input_set(s,1,0);
 assert(st_input_snapshot(s)==16);
 st_input_set(s,-1,1); st_input_set(s,32,1);
 assert(st_input_snapshot(s)==16);
 st_input_reset(s); assert(st_input_snapshot(s)==0);
 st_input_destroy(s);
 st_input_set(nullptr,1,1); st_input_reset(nullptr);
 assert(st_input_snapshot(nullptr)==0);
 st_input_destroy(nullptr);
}
