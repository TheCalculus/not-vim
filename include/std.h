#ifndef NV_STD_H
#define NV_STD_H

#include <stddef.h>

int nv_clamp(int x, int min, int max);
int nv_min(int a, int b);
int nv_max(int a, int b);
size_t nv_positive_difference(size_t a, size_t b);

#endif