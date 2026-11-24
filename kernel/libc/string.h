/* minimal headers for building minimp3 inside FlashKernel */
#ifndef FLASH_STRING_H
#define FLASH_STRING_H
typedef unsigned int size_t;
void *memcpy(void *d, const void *s, unsigned int n);
void *memset(void *d, int c, unsigned int n);
void *memmove(void *d, const void *s, unsigned int n);
int memcmp(const void *a, const void *b, unsigned int n);
#endif
