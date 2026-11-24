#include"MsrBitmap.h"
void CheckApicMode(ULONG CPUIndex)
{
    ULONG64 apicBase = __readmsr(IA32_APIC_BASE);

    BOOLEAN apicEnabled = (apicBase & (1ULL << 11)) != 0;
    BOOLEAN x2ApicEnabled = (apicBase & (1ULL << 10)) != 0;

    if (!apicEnabled && !x2ApicEnabled)
    {
        DbgPrintEx(0, 0, "[CPU%i]APIC: Disabled\n", CPUIndex);
    }
    else if (apicEnabled && !x2ApicEnabled)
    {
        DbgPrintEx(0, 0, "[CPU%i]APIC: xAPIC\n", CPUIndex);
    }
    else if (apicEnabled && x2ApicEnabled)
    {
        DbgPrintEx(0, 0, "[CPU%i]APIC: x2APIC\n", CPUIndex);
    }
}