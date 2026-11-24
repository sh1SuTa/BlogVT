#pragma once

#include <ntddk.h>

typedef struct _MSR_BITMAP
{
    UCHAR ReadLow[0x400];
    UCHAR ReadHigh[0x400];
    UCHAR WriteLow[0x400];
    UCHAR WriteHigh[0x400];

} MSR_BITMAP, * PMSR_BITMAP;

static __forceinline BOOLEAN SetMsrBitmap(
    PMSR_BITMAP bitmap,
    ULONG msr,
    BOOLEAN isWrite,
    BOOLEAN intercept
)
{
    if (bitmap == NULL)
        return FALSE;

    ULONG offset;
    ULONG index;

    if (msr <= 0x1FFF)
    {
        offset = isWrite ? 0x800 : 0x000;
        index = msr;
    }
    else if (msr >= 0xC0000000 &&
        msr <= 0xC0001FFF)
    {
        offset = isWrite ? 0xC00 : 0x400;
        index = msr - 0xC0000000;
    }
    else
    {
        return FALSE;
    }

    UCHAR* bytes = (UCHAR*)(bitmap);

    UCHAR mask = (UCHAR)(1u << (index % 8));

    if (intercept)
        bytes[offset + index / 8] |= mask;
    else
        bytes[offset + index / 8] &= (UCHAR)(~mask);

    return TRUE;
}

#define IA32_APIC_BASE 0x1B

#ifdef __cplusplus
extern "C" {
#endif

void CheckApicMode(ULONG CPUIndex);

#ifdef __cplusplus
}
#endif
