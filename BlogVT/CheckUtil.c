
#pragma once
#include"CheckUtil.h"

BOOLEAN checkWait_for_SIPI()
{
    ULONG64 vmxMisc = __readmsr(MsrVmxMisc);

    BOOLEAN supportWaitSipi =
        (vmxMisc & (1ULL << 8)) != 0;
    return supportWaitSipi;
}
BOOLEAN checkIsBsp(ULONG index) {
    ULONG64 apicBase = __readmsr(MsrApicBase);
    DbgPrintEx(0, 0, "[+] CPU%u APIC_BASE=%llX, isBsp=%u", index, apicBase,
        (apicBase >> 8) & 1);
    return TRUE;
}