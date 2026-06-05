#include <stddef.h>

#include "std.h"

int nv_clamp(int x, int min, int max)
{
    if (x > max) {
        return max;
    }

    if (x < min) {
        return min; 
    }

    return x;
}

int nv_min(int a, int b)
{
    return a > b ? b : a;
}

int nv_max(int a, int b)
{
    return a > b ? a : b;
}

size_t nv_positive_difference(size_t a, size_t b)
{
    return (a > b) ? (a - b) : (b - a);
}