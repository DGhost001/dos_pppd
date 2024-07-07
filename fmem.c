/* This is the far mem implementation needed to get the stuff
   to compile properly in Turbo C++ 1.0
 */

#include "fmem.h"

void far * far _fmemcpy(void far * dest, const void far *src, size_t n)
{
   char far * d = (char far*)dest;
   char far * s = (char far*)src;

   for(;n > 0;--n,++d,++s) {
     *d = *s;
   }

   return dest;
}

void far * far _fmemset(void far * s, int c, size_t n)
{
   char far * d = (char far*)s;
   for(; n> 0; --n, ++d) {
     *d = (char)c;
   }

   return s;
}

int far _fmemcmp(const void far *s1, const void far *s2, size_t n)
{
   const unsigned char far *st1 = (const unsigned char far*)s1;
   const unsigned char far *st2 = (const unsigned char far*)s2;

   for(;n > 0 && *st1 == *st2;--n, ++st1, ++st2);

   return 0 == n ? 0 : *st1 - *st2;
}
