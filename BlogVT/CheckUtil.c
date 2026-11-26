
#pragma once
#include"CheckUtil.h"

BOOLEAN checkWait_for_SIPI()
{
    ULONG64 vmxMisc = __readmsr(MsrVmxMisc);

    BOOLEAN supportWaitSipi =
        (vmxMisc & (1ULL << 8)) != 0;
    return supportWaitSipi;
}
