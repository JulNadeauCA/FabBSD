/* { dg-do compile { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O2 -msse2 -march=athlon" } */
/* { dg-final { scan-assembler "pand.*magic" } } */
/* { dg-final { scan-assembler "pandn.*magic" } } */
/* { dg-final { scan-assembler "pxor.*magic" } } */
/* { dg-final { scan-assembler "por.*magic" } } */
/* { dg-final { scan-assembler "movdqa" } } */
/* { dg-final { scan-assembler-not "movaps.*magic" } } */

/* Verify that we generate proper instruction with memory operand.  */

#include <xmmintrin.h>
__m128i magic_a, magic_b;
__m128i
t1(void)
{
return _mm_and_si128 (magic_a,magic_b);
}
__m128i
t2(void)
{
return _mm_andnot_si128 (magic_a,magic_b);
}
__m128i
t3(void)
{
return _mm_or_si128 (magic_a,magic_b);
}
__m128i
t4(void)
{
return _mm_xor_si128 (magic_a,magic_b);
}

