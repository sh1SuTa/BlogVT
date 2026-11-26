#include"ExitHandler.h"
VCPU g_vcpu[16] = { 
    {0,0,0,0,0,0xFFFF0FF0},
    {0,0,0,0,0,0xFFFF0FF0}
};
void VmxRestoreHostCr2()
{
    ULONG index = KeGetCurrentProcessorNumberEx(NULL);

    NT_ASSERT(index < RTL_NUMBER_OF(g_vcpu));

    __writecr2(g_vcpu[index].HostCR2);
}
void VmxSaveHostCr2()
{
    ULONG index = KeGetCurrentProcessorNumberEx(NULL);

    NT_ASSERT(index < RTL_NUMBER_OF(g_vcpu));

    g_vcpu[index].HostCR2 = __readcr2();
}
void VmxInitGuestStateAfterExitInit(GpRegisters* guestRegs, ULONG currentCPU
) {
	(currentCPU);
	

	__vmx_vmwrite(GuestRflags, 0x2);
	__vmx_vmwrite(GuestRip, 0xFFF0);
	size_t oldCr0;
	__vmx_vmread(GuestCr0, &oldCr0);
	ULONG64 initCr0 =
		(oldCr0 & (CR0_CD | CR0_NW))
		| CR0_ET
		| CR0_NE;
	__vmx_vmwrite(GuestCr0, initCr0);
	guestRegs->Cr2 = 0;
	__vmx_vmwrite(GuestCr3, 0);
	__vmx_vmwrite(GuestCr4, 0);
	__vmx_vmwrite(GuestCsSelector, 0xF000);
	__vmx_vmwrite(GuestCsBase, 0xFFFF0000);
	__vmx_vmwrite(GuestCsLimit, 0xFFFF);
	__vmx_vmwrite(GuestCsAccessRight, 0x9B);
	__vmx_vmwrite(GuestSsSelector, 0);
	__vmx_vmwrite(GuestSsBase, 0);
	__vmx_vmwrite(GuestSsLimit, 0xFFFF);
	__vmx_vmwrite(GuestSsAccessRight, 0x9B);
	__vmx_vmwrite(GuestDsSelector, 0);
	__vmx_vmwrite(GuestDsBase, 0);
	__vmx_vmwrite(GuestDsLimit, 0xFFFF);
	__vmx_vmwrite(GuestDsAccessRight, 0x9B);
	__vmx_vmwrite(GuestEsSelector, 0);
	__vmx_vmwrite(GuestEsBase, 0);
	__vmx_vmwrite(GuestEsLimit, 0xFFFF);
	__vmx_vmwrite(GuestEsAccessRight, 0x9B);
	__vmx_vmwrite(GuestFsSelector, 0);
	__vmx_vmwrite(GuestFsBase, 0);
	__vmx_vmwrite(GuestFsLimit, 0xFFFF);
	__vmx_vmwrite(GuestFsAccessRight, 0x9B);
	__vmx_vmwrite(GuestGsSelector, 0);
	__vmx_vmwrite(GuestGsBase, 0);
	__vmx_vmwrite(GuestGsLimit, 0xFFFF);
	__vmx_vmwrite(GuestGsAccessRight, 0x9B);
	//不需要通过段描述符的 Access Rights 来描述自身属性
	__vmx_vmwrite(GuestGDTRBase, 0);
	__vmx_vmwrite(GuestGDTRLimit, 0xFFFF);
	__vmx_vmwrite(GuestIDTRBase, 0);
	__vmx_vmwrite(GuestIDTRLimit, 0xFFFF);

	__vmx_vmwrite(GuestLDTRSelector, 0);
	__vmx_vmwrite(GuestLDTRLimit, 0xFFFF);
	__vmx_vmwrite(GuestLDTRBase, 0);
	__vmx_vmwrite(GuestLDTRAccessRight, 0x82);
	__vmx_vmwrite(GuestTRSelector, 0);
	__vmx_vmwrite(GuestTRBase, 0);
	__vmx_vmwrite(GuestTRLimit, 0xFFFF);
	__vmx_vmwrite(GuestTRAccessRight, 0x83);

	__vmx_vmwrite(GuestIa32BndCfgs, 0);
	__vmx_vmwrite(GuestIa32EferCtl, 0);
	ULONG64 entryControls = 0;
	__vmx_vmread(VmEntryControls, &entryControls);
	entryControls &= ~(1u << 9); // IA-32e mode guest = 0
	__vmx_vmwrite(VmEntryControls, entryControls);

	guestRegs->ax = 0x04;
	guestRegs->bx = 0;
	guestRegs->cx = 0;
	int cpuInfo[4] = {0};
	__cpuid(cpuInfo, 1);// CPUID.01H:EAX
	ULONG32 processorSignature = (ULONG32)cpuInfo[0];
	guestRegs->dx = processorSignature;
	__vmx_vmwrite(GuestRsp, 0);
	guestRegs->bp = 0;
	guestRegs->si = 0;
	guestRegs->di = 0;
	guestRegs->r8 = 0;
	guestRegs->r9 = 0;
	guestRegs->r10 = 0;
	guestRegs->r11 = 0;
	guestRegs->r12 = 0;
	guestRegs->r13 = 0;
	guestRegs->r14 = 0;
	guestRegs->r15 = 0;

	guestRegs->DR0 = 0;
	guestRegs->DR1 = 0;
	guestRegs->DR2 = 0;
	guestRegs->DR3 = 0;
	guestRegs->DR6 = 0xFFFF0FF0;
	__vmx_vmwrite(GuestDr7, 0x400);
	__vmx_vmwrite(GuestInterruptibilityState, 0);

	
}