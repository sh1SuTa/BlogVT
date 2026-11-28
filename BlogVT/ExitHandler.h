#pragma once

#include<intrin.h>
#include"ia32.h"
#include"CPU.h"

#define CR0_CD (1ULL << 30)
#define CR0_NW (1ULL << 29)
#define CR0_ET (1ULL << 4)
#define CR0_NE (1ULL << 5)
struct ExitLoopTracker
{
	volatile LONG Reason;
	volatile LONG64 Rip;
	volatile LONG Count;
};
#ifdef __cplusplus
extern "C" {
#endif

	extern VCPU g_vcpu[16];
	void VmxInitGuestStateAfterExitInit(GpRegisters* guestRegs, ULONG currentCPU
	);

#ifdef __cplusplus
}
#endif