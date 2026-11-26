#pragma once
#include<basetsd.h>
#define GUEST_ACTIVITY_ACTIVE           0
#define GUEST_ACTIVITY_HLT              1
#define GUEST_ACTIVITY_SHUTDOWN         2
#define GUEST_ACTIVITY_WAIT_FOR_SIPI    3
typedef struct _VCPU
{
    ULONG64 HostDR0;
    ULONG64 HostDR1;
    ULONG64 HostDR2;
    ULONG64 HostDR3;
    ULONG64 HostDR6;
    ULONG64 HostCR2;
} VCPU;