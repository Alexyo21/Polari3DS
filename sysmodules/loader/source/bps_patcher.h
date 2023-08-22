#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdalign.h>
#include <unistd.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <3ds/types.h>

bool patcherApplyCodeBpsPatch(u64 progId, u8* code, u32 size);

#ifdef __cplusplus
}
#endif
