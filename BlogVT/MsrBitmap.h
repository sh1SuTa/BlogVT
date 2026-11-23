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
    if (bitmap == nullptr)
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

    UCHAR* bytes = reinterpret_cast<UCHAR*>(bitmap);

    UCHAR mask = static_cast<UCHAR>(1u << (index % 8));

    if (intercept)
        bytes[offset + index / 8] |= mask;
    else
        bytes[offset + index / 8] &= static_cast<UCHAR>(~mask);

    return TRUE;
}