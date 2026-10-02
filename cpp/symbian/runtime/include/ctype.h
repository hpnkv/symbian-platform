#ifndef SYMBIAN_RUNTIME_CTYPE_H_
#define SYMBIAN_RUNTIME_CTYPE_H_

#include_next <ctype.h>

// The selected OpenC header omits the C99 isblank declaration.
#ifdef __cplusplus
extern "C" {
#endif
int isblank(int value);
#ifdef __cplusplus
}
#endif

#endif  // SYMBIAN_RUNTIME_CTYPE_H_
