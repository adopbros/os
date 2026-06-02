/*
 * Arrowix OS - Minimal freestanding mem*/str* routines.
 *
 * These have external linkage with the standard names because the compiler may
 * emit implicit calls to memset/memcpy/memmove/memcmp even in freestanding mode.
 */
#pragma once

#include <arrowix/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);

#ifdef __cplusplus
}
#endif
