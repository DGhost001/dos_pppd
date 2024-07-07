#ifndef FMEM_H
#define FMEM_H

#include <mem.h> /* Get all the memory definition stuff */

void far * far _fmemcpy(void far * dest, const void far *src, size_t n);
void far * far _fmemset(void far * s, int c, size_t n);
int far _fmemcmp(const void far *s1, const void far *s2, size_t n);

#endif
