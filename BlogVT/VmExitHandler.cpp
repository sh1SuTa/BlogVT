#include"HOOK.h"
#include"CPU.h"
#include"MsrBitmap.h"
#include"ExitHandler.h"
union EptViolationQualification {
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;                   //!< [0]
		ULONG64 write_access : 1;                  //!< [1]
		ULONG64 execute_access : 1;                //!< [2]
		ULONG64 ept_readable : 1;                  //!< [3]
		ULONG64 ept_writeable : 1;                 //!< [4]
		ULONG64 ept_executable : 1;                //!< [5]
		ULONG64 ept_executable_for_user_mode : 1;  //!< [6]
		ULONG64 valid_guest_linear_address : 1;    //!< [7]
		ULONG64 caused_by_translation : 1;         //!< [8]
		ULONG64 user_mode_linear_address : 1;      //!< [9]
		ULONG64 readable_writable_page : 1;        //!< [10]
		ULONG64 execute_disable_page : 1;          //!< [11]
		ULONG64 nmi_unblocking : 1;                //!< [12]
	} fields;
};
static_assert(sizeof(EptViolationQualification) == 8, "Size check");

#pragma warning(push)
#pragma warning(disable:4244)

//有些VmExit事件触发处理完成后，回到Guest时需要跳过触发VmExit的代码
//比如VmCall的时候，不跳过的话，回到Guest又继续触发VmCall
//跳过Guest当前执行的代码
void VmmAdjustGuestRip()
{
	SIZE_T  instLen = 0;
	ULONG_PTR rip = 0;
	__vmx_vmread(GuestRip, &rip);
	//获取Guest当前执行指令的长度
	__vmx_vmread(VmExitInstructionLength, &instLen);
	if (instLen == 0 || instLen > 15) {
		KeBugCheckEx(0x7A89, rip, instLen, 0, 0);
	}
	__vmx_vmwrite(GuestRip, (SIZE_T)(rip + instLen));
}

//退出VT，这里我是CV了其他项目的
void VmxPrepareOff(GpRegisters* pGuestRegisters)
{
	/*
	当发生VM退出时，处理器将IDT和GDT的Limit设置为ffff。
	这里把它改回正确的值
	*/
	ULONG_PTR gdt_limit = 0;
	__vmx_vmread(GuestGDTRLimit, &gdt_limit);

	ULONG_PTR gdt_base = 0;
	__vmx_vmread(GuestGDTRBase, &gdt_base);
	ULONG_PTR idt_limit = 0;
	__vmx_vmread(GuestIDTRLimit, &idt_limit);
	ULONG_PTR idt_base = 0;
	__vmx_vmread(GuestIDTRBase, &idt_base);

	Gdtr gdtr = { (USHORT)gdt_limit, gdt_base };
	Idtr idtr = { (USHORT)(idt_limit), idt_base };
	AsmWriteGDT(&gdtr);
	__lidt(&idtr);


	//跳过VmCall指令
	ULONG_PTR exit_instruction_length = 0;
	__vmx_vmread(VmExitInstructionLength, &exit_instruction_length);
	ULONG_PTR rip = 0;
	__vmx_vmread(GuestRip, &rip);
	ULONG_PTR return_address = rip + exit_instruction_length;

	// Since the flag register is overwritten after VMXOFF, we should manually
	// indicates that VMCALL was successful by clearing those flags.
	// See: CONVENTIONS
	FlagRegister rflags = { 0 };
	__vmx_vmread(GuestRflags, (SIZE_T*)&rflags);

	rflags.fields.cf = FALSE;
	rflags.fields.pf = FALSE;
	rflags.fields.af = FALSE;
	rflags.fields.zf = FALSE;
	rflags.fields.sf = FALSE;
	rflags.fields.of = FALSE;
	rflags.fields.cf = FALSE;
	rflags.fields.zf = FALSE;

	// Set registers used after VMXOFF to recover the context. Volatile
	// registers must be used because those changes are reflected to the
	// guest's context after VMXOFF.
	pGuestRegisters->cx = return_address;
	__vmx_vmread(GuestRsp, &pGuestRegisters->dx);
	pGuestRegisters->ax = rflags.all;
}

//处理MSR的读写，不用理，CV即可
VOID ReadWriteMsrHandle(GpRegisters* pGuestRegisters, BOOLEAN isRead)
{
	ULONG msrIndex = (ULONG)pGuestRegisters->cx;
	MSR msr = (MSR)msrIndex;

	BOOLEAN transfer_to_vmcs = false;
	VmcsField vmcs_field = {};
	switch (msr) {
	case MSR::MsrSysenterCs:
		vmcs_field = VmcsField::GuestIa32SYSENTERCS;
		transfer_to_vmcs = true;
		break;
	case MSR::MsrSysenterEsp:
		vmcs_field = VmcsField::GuestIa32SYSENTERESP;
		transfer_to_vmcs = true;
		break;
	case MSR::MsrSysenterEip:
		vmcs_field = VmcsField::GuestIa32SYSENTEREIP;
		transfer_to_vmcs = true;
		break;
	case MSR::MsrDebugctl:
		vmcs_field = VmcsField::GuestIa32DebugCtl;
		transfer_to_vmcs = true;
		break;
	case MSR::MsrGsBase:
		vmcs_field = VmcsField::GuestGsBase;
		transfer_to_vmcs = true;
		break;
	case MSR::MsrFsBase:
		vmcs_field = VmcsField::GuestFsBase;
		transfer_to_vmcs = true;
		break;
	default:
		break;
	}

	LARGE_INTEGER msr_value = {};
	if (isRead) {
		if (transfer_to_vmcs) {
			__vmx_vmread(vmcs_field, (SIZE_T*)&msr_value.QuadPart);
		}
		else {
			msr_value.QuadPart = __readmsr((ULONG)pGuestRegisters->cx);
		}

		pGuestRegisters->ax = msr_value.LowPart;
		pGuestRegisters->dx = msr_value.HighPart;
	}
	else
	{
		msr_value.LowPart = (ULONG)pGuestRegisters->ax;
		msr_value.HighPart = (ULONG)pGuestRegisters->dx;
		if (transfer_to_vmcs) {
			__vmx_vmwrite(vmcs_field, (ULONG_PTR)msr_value.QuadPart);
		}
		else {
			__writemsr(pGuestRegisters->cx, msr_value.QuadPart);
		}
	}
}


VOID EptViolationHandle()
{
	//获取触发EptViolation的地址
	ULONG_PTR ExitPhyAddr = 0;
	__vmx_vmread(VmExitGuestPhysicalAddress, &ExitPhyAddr);

	//通过触发EptViolation的地址，查找是不是跟我们HOOK有关
	PEptHookInfo hookInfo = GetHookInfoByPA(ExitPhyAddr);

	if (hookInfo)
	{
		EptCommonEntry* pte = GetPteByPhyAddr(ExitPhyAddr);

		//如果触发EptViolation时，当前页是能执行，而这里只有两种情况，要么能执行不能读写，要么能读写不能执行
		//能执行说明它是读或写只能执行的页面而触发了EptViolation，让它读写原函数页
		if (pte->fields.execute_access) {
			pte->fields.execute_access = 0;
			pte->fields.read_access = 1;
			pte->fields.write_access = 1;
			pte->fields.physial_address = hookInfo->RealPagePhyAddr >> 12;
		}
		else {
			//如果触发EptViolation时，当前页是可读可写，说明它是执行触发了EptViolation，让它执行HOOK函数页
			pte->fields.execute_access = 1;
			pte->fields.read_access = 0;
			pte->fields.write_access = 0;
			pte->fields.physial_address = hookInfo->FakePagePhyAddr >> 12;
		}
	}
	else {
		KeBugCheckEx(0x7748, 0, 0, 0, 0);
	}
}


BOOLEAN VmCallHandle(GpRegisters* pGuestRegisters)
{
	//x64下都是fastcall调用约定
	//VmCall的功能号
	ULONG_PTR num = pGuestRegisters->cx;
	//附加的参数
	ULONG_PTR param = pGuestRegisters->dx;

	BOOLEAN ContinueVmx = TRUE;

	EptCommonEntry* pte = 0;
	PEptHookInfo hookInfo = 0;

	switch (num)
	{
	case CallExitVT://call 0表示关闭VT
		ContinueVmx = FALSE;
		Log("vmcall(0)");
		VmxPrepareOff(pGuestRegisters);
		break;
	case CallEptHook:
		//HOOK的时候，把原函数所在页改成复制出来的页面，并且只能执行
		hookInfo = (PEptHookInfo)param;
		pte = GetPteByPhyAddr(hookInfo->RealPagePhyAddr);
		if (pte) {
			pte->fields.physial_address = hookInfo->FakePagePhyAddr >> 12;
			pte->fields.execute_access = 1;
			pte->fields.read_access = 0;
			pte->fields.write_access = 0;
		}
		break;
	case CallEptUnHook:
		//HOOK的时候，把原函数所在页改回去，并且赋予全部权限
		hookInfo = (PEptHookInfo)param;
		pte = GetPteByPhyAddr(hookInfo->RealPagePhyAddr);
		if (pte) {
			pte->fields.physial_address = hookInfo->RealPagePhyAddr >> 12;
			pte->fields.execute_access = 1;
			pte->fields.read_access = 1;
			pte->fields.write_access = 1;
		}
		break;
	default:
		Log("未知的VmCall");
		KeBugCheckEx(0x7A75, 0, 0, 0, 0);
		break;
	}

	return ContinueVmx;
}

static ExitLoopTracker g_ExitTracker[64];
EXTERN_C BOOLEAN VmexitHandler(GpRegisters* pGuestRegisters)
{
	KIRQL irql = KeGetCurrentIrql();
	if (irql < DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel();
	}

	ULONG CurrentProcessorIndex = KeGetCurrentProcessorNumberEx(NULL);
	VmExitInformation ExitReason = { 0 };
	FlagRegister guestRflag = { 0 };
	BOOLEAN ContinueVmx = TRUE;
	ULONG_PTR Rip = 0;
	__vmx_vmread(GuestRip, &Rip);
	__vmx_vmread(VmExitReason, (SIZE_T*)(&ExitReason));


	/*
	auto& tracker = g_ExitTracker[CurrentProcessorIndex];
	if (tracker.Reason == (LONG)ExitReason.fields.reason &&tracker.Rip == (LONG64)Rip)
	{
		LONG count = InterlockedIncrement(&tracker.Count);

		if (count >= 64)
		{
			SIZE_T instLen = 0;

			__vmx_vmread(VmExitInstructionLength, &instLen);

			ULONG msrIndex = (ULONG)pGuestRegisters->cx;

			KeBugCheckEx(
				0x7A88,
				ExitReason.all,
				msrIndex,
				Rip,instLen
			);
		}
	}
	else
	{
		tracker.Reason = ExitReason.fields.reason;
		tracker.Rip = Rip;
		InterlockedExchange(&tracker.Count, 1);
	}*/
	switch (ExitReason.fields.reason)
	{
	case ExitTripleFault: {
		Log("TripleFault %llx", Rip);
		//VmmAdjustGuestRip();
		KeBugCheckEx(0x7777, Rip, 0, 0, 0);
		break;
	}
	case ExitEptMisconfig: {

		Log("ExitEptMisconfig");
		KeBugCheckEx(0x7778, Rip, 0, 0, 0);
		break;
	}
	case ExitEptViolation: {

		EptViolationHandle();
		break;
	}
	case ExitCrAccess: {

		KeBugCheckEx(0x7728, Rip, 0, 0, 0);
		break;
	}
	//msr读写必须处理
	case ExitMsrRead:
	{
		
		ReadWriteMsrHandle(pGuestRegisters, TRUE);
		VmmAdjustGuestRip();
		break;
	}
	case ExitMsrWrite:
	{
		Log("ExitMsrWrite");
		ReadWriteMsrHandle(pGuestRegisters, FALSE);
		VmmAdjustGuestRip();
		break;
	}
	case ExitCpuid:
	{
		//Log("ExitCpuid");
		int leaf = (int)pGuestRegisters->ax;
		int sub_leaf = (int)pGuestRegisters->cx;
		int result[4] = { 0 };
		__cpuidex((int*)&result, leaf, sub_leaf);

		//if (leaf ==1)
		//{
		//	//((CpuFeaturesEcx*)&result[2])->fields.
		//}
		pGuestRegisters->ax = result[0];
		pGuestRegisters->bx = result[1];
		pGuestRegisters->cx = result[2];
		pGuestRegisters->dx = result[3];
		VmmAdjustGuestRip();
		break;
	}
	case ExitIoInstruction:
	{
		Log("ExitIoInstruction");
		VmmAdjustGuestRip();
		break;
	}
	case ExitVmcall:
	{
		ContinueVmx = VmCallHandle(pGuestRegisters);
		//如果不是退出VT，跳过VmCall指令继续执行
		if (ContinueVmx) VmmAdjustGuestRip();
		break;
	}
	case ExitExceptionOrNmi:
	{
		Log("ExitExceptionOrNmi");
		VmExitInterruptionInformationField exception = { 0 };
		__vmx_vmread(VmExitInterruptionInformation, (SIZE_T*)&exception);

		if (exception.fields.interruption_type == kHardwareException)
		{
			//VmmpInjectInterruption(exception.fields.interruption_type,)
			exception.fields.valid = TRUE;
			__vmx_vmwrite(VmEntryInterruptionInformation, exception.all);
		}
		else if (exception.fields.interruption_type == kSoftwareException)
		{
			__vmx_vmwrite(VmEntryInterruptionInformation, exception.all);
			SIZE_T exit_inst_length = 0;
			__vmx_vmread(VmExitInstructionLength, &exit_inst_length);
			__vmx_vmwrite(VmEntryInstructionLength, exit_inst_length);
		}
		break;
	}
	case ExitMonitorTrapFlag:
	{
		Log("ExitMonitorTrapFlag");

		KeBugCheckEx(0x7737, Rip, 0, 0, 0);
		break;
	}
	case ExitHlt:
	{
		Log("ExitHlt");
		KeBugCheckEx(0x7712, Rip, 0, 0, 0);
		break;
	}
	case ExitVmclear: {
		KeBugCheckEx(0x7719, Rip, 0, 0, 0);
	}
	case ExitVmptrld: {
		SIZE_T cs = 0;
		__vmx_vmread(GuestCsSelector, &cs);
		SIZE_T csBase = 0;
		__vmx_vmread(GuestCsBase, &csBase);
		SIZE_T instructionInfo = 0;
		__vmx_vmread(VmEntryInterruptionInformation, &instructionInfo);
		
		KeBugCheckEx(0x7A74, Rip, cs, CurrentProcessorIndex, instructionInfo);
		break;
	}
	case ExitVmptrst: {
		KeBugCheckEx(0x7722, Rip, 0, 0, 0);
	}
	case ExitVmread: {
		KeBugCheckEx(0x7723, Rip, 0, 0, 0);
	}
	case ExitVmwrite: {
		KeBugCheckEx(0x7725, Rip, 0, 0, 0);
	}
	case ExitVmresume: {
		KeBugCheckEx(0x7724, Rip, 0, 0, 0);
	}
	case ExitVmoff: {
		KeBugCheckEx(0x7726, Rip, 0, 0, 0);
	}
	case ExitVmon: {

		KeBugCheckEx(0x7727, Rip, 0, 0, 0);
	}
	case ExitVmlaunch: {
		KeBugCheckEx(0x7720, Rip, 0, 0, 0);
	}
	case ExitVmfunc: {
		KeBugCheckEx(0x7759, Rip, 0, 0, 0);
	}
	case ExitInvept: {
		KeBugCheckEx(0x7750, Rip, 0, 0, 0);
	}
	case ExitInvvpid:
	{
		Log("vm inst %d", ExitReason.fields.reason);
		__vmx_vmread(GuestRflags, (SIZE_T*)&guestRflag);
		guestRflag.fields.cf = 1;
		__vmx_vmwrite(GuestRflags, guestRflag.all);
		VmmAdjustGuestRip();
		break;
	}
	case ExitInvd:
	{
		Log("ExitInvd");
		AsmInvd();
		VmmAdjustGuestRip();
		break;
	}
	case ExitInvlpg:
	{
		Log("ExitInvlpg");
		ExitQualification eq = { 0 };
		__vmx_vmread(VmExitQualification, (SIZE_T*)&eq);
		InvVpidDescriptor desc = { 0 };
		desc.vpid = CurrentProcessorIndex + 1;
		desc.linear_address = eq.all;
		AsmInvvpid(kIndividualAddressInvalidation, (SIZE_T*)&desc);
		VmmAdjustGuestRip();
		break;
	}
	case ExitRdtsc:
	{
		Log("ExitRdtsc");

		ULARGE_INTEGER tsc = { 0 };
		tsc.QuadPart = __rdtsc();
		pGuestRegisters->dx = tsc.HighPart;
		pGuestRegisters->ax = tsc.LowPart;
		VmmAdjustGuestRip();
		break;
	}
	case ExitRdtscp:
	{
		Log("ExitRdtscp");

		unsigned int tsc_aux = 0;
		ULARGE_INTEGER tsc = { 0 };
		tsc.QuadPart = __rdtscp(&tsc_aux);
		pGuestRegisters->dx = tsc.HighPart;
		pGuestRegisters->ax = tsc.LowPart;
		pGuestRegisters->cx = tsc_aux;
		VmmAdjustGuestRip();
		break;
	}
	case ExitXsetbv:
	{
		Log("ExitXsetbv");

		ULARGE_INTEGER value = { 0 };
		value.LowPart = pGuestRegisters->ax;
		value.HighPart = pGuestRegisters->dx;
		_xsetbv(pGuestRegisters->cx, value.QuadPart);

		VmmAdjustGuestRip();
		break;
	}
	case ExitInit: {
		ULONG64 apicBase = __readmsr(MsrApicBase);
		BOOLEAN isBsp = (apicBase & (1ULL << 8)) != 0;

		VmxInitGuestStateAfterExitInit(pGuestRegisters, CurrentProcessorIndex);
		// 清除事件注入状态
		__vmx_vmwrite(GuestInterruptibilityState, 0);
		__vmx_vmwrite(VmEntryInterruptionInformation, 0);
		if (isBsp)
		{

			Log("Current CPU-%i is BSP", CurrentProcessorIndex);
			__vmx_vmwrite(
				GuestActivityState,GUEST_ACTIVITY_ACTIVE
			);
		}
		else
		{
			Log(" CPU-%i is AP", CurrentProcessorIndex);
			// AP 进入等待 SIPI 状态
			__vmx_vmwrite(GuestActivityState,GUEST_ACTIVITY_WAIT_FOR_SIPI	);
		}
		break;
	}
	case ExitSipi: 
	{
		

		SIZE_T exitQualification = 0;
		__vmx_vmread(VmExitQualification, &exitQualification);
		UCHAR sipiVector = (UCHAR)(exitQualification & 0xFF);
		// 计算 CS Selector 和 Base
		ULONG64 csSelector = (ULONG64)sipiVector << 8;
		ULONG64 csBase = (ULONG64)sipiVector << 12;
		// 设置 Guest 启动状态
		__vmx_vmwrite(GuestCsSelector, csSelector);
		__vmx_vmwrite(GuestCsBase, csBase);
		__vmx_vmwrite(GuestRip, 0);

		Log("Current CPU-%i ExitSipi ,vector= %02X,CS Selector = %llX,CS Base = %llX", CurrentProcessorIndex, sipiVector, csSelector, csBase);
		__vmx_vmwrite(GuestActivityState, GUEST_ACTIVITY_ACTIVE);
		__vmx_vmwrite(VmEntryInterruptionInformation, 0);
		KeBugCheckEx(0x7A04, exitQualification, sipiVector, csSelector, csBase);
		break;
	}
	case ExitInvalidGuestState: {
		SIZE_T guestCr4 = 0;
		SIZE_T csAR = 0;
		SIZE_T trSelector = 0;
		SIZE_T trAR = 0;

		__vmx_vmread(GuestCr4, &guestCr4);
		__vmx_vmread(GuestCsAccessRight, &csAR);
		__vmx_vmread(GuestTRSelector, &trSelector);
		__vmx_vmread(GuestTRAccessRight, &trAR);

		KeBugCheckEx(
			0x7A33,
			guestCr4,
			csAR,
			trSelector,
			trAR
		);
		break;
	}
	default: {
		SIZE_T qualification = 0;
		SIZE_T entryInfo = 0;

		__vmx_vmread(VmExitQualification, &qualification);
		__vmx_vmread(VmEntryInterruptionInformation, &entryInfo);
		KeBugCheckEx(0x7A77, CurrentProcessorIndex, qualification, ExitReason.all,  entryInfo);
		break;
	}
	}

	if (irql < DISPATCH_LEVEL) {
		KeLowerIrql(irql);
	}
	//保存当前 Host CR2，恢复 Guest CR2
	//g_vcpu[CurrentProcessorIndex].HostCr2 = __readcr2();
	//__writecr2(g_vcpu[CurrentProcessorIndex].GuestCr2);
	return ContinueVmx;
}

#pragma warning(pop)