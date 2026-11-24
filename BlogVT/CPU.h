#pragma once
#include<basetsd.h>
#define GUEST_ACTIVITY_ACTIVE        0
#define GUEST_ACTIVITY_WAIT_FOR_SIPI  3
typedef struct _VCPU
{
    ULONG64 GuestCr2;//Host 和 Guest 共享的物理寄存器
    ULONG64 HostCr2;
    ULONG64 DR0;
    ULONG64 DR1;
    ULONG64 DR2;
    ULONG64 DR3;
    ULONG64 DR6;//0xFFFF0FF0
} VCPU;