#include"TinyVT.h"
#include"HOOK.h"
#include"CheckUtil.h"
#include<Ndis.h>

TinyVT* AllVT[16] = {0};
void* __cdecl operator new(size_t, void* p) noexcept { return p; }
void __cdecl operator delete(void*, void*) noexcept {}
EXTERN_C VOID LoadVT(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);

	//获取当前CPU核心的号数
	ULONG index = KeGetCurrentProcessorIndex();
	BOOLEAN b = checkWait_for_SIPI();
	if (b==0)
	{
		Log("CPU%d 不支持Wait_for_SIPI", index);
		goto done;
	}

	
	ULONG64 capability = __readmsr(0x48B);

	ULONG32 allowed1 = (ULONG32)(capability >> 32);

	Log("Unrestricted Guest allowed: %d",
		(allowed1 & (1 << 7)) != 0);
	
	if (CheckVTSupport() && CheckVTEnable())
	{
		TinyVT* tinyVt = (TinyVT*)ExAllocatePool2(
			POOL_FLAG_NON_PAGED, sizeof(TinyVT), 'tVTp');
		if (tinyVt == NULL) {
			Log("[CPU:%d]ExAllocatePoolWithTag 失败", index);
			goto done;
		}
		RtlZeroMemory(tinyVt, sizeof(TinyVT));
		// placement new：在已分配内存上构造，不分配新内存
		new (tinyVt) TinyVT(index);
		if (tinyVt->StartVT()) {
			AllVT[index] = tinyVt;
			Log("[CPU:%d]启动VT成功", index);
		}
		else {
			Log("[CPU:%d]启动VT失败",index);
		}
		
	}
	else {
		Log("[CPU:%d]不支持虚拟化", index);
	}
done:
	NT_ASSERT(SystemArgument1 != NULL);
	NT_ASSERT(SystemArgument2 != NULL);
	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);
}

EXTERN_C VOID UnloadVT(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);

	int index = KeGetCurrentProcessorIndex();

	if (AllVT[index] && AllVT[index]->isEnable) {
		AsmVmxCall(CallExitVT, NULL);
	}

	if (AllVT[index]) {
		delete AllVT[index];
		AllVT[index] = NULL;
	}
	Log("[CPU:%d]VT已退出", index);
	NT_ASSERT(SystemArgument1 != NULL);
	NT_ASSERT(SystemArgument2 != NULL);
	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);
}


EXTERN_C VOID DriverUnload(PDRIVER_OBJECT driver)
{
	UNREFERENCED_PARAMETER(driver);

	//先销毁HOOK再退VT
	DestroyEptHook();
	//延时等一下销毁完毕
	NdisStallExecution(50);

	KeGenericCallDpc(UnloadVT, NULL);

	//延时等一下VT退出完毕
	NdisStallExecution(50);
	if (EptMem) {
		kfree(EptMem);
		EptMem = 0;
	}

	Log("驱动卸载");
}

EXTERN_C VOID HookTest();


NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT driver, _In_  PUNICODE_STRING  path)
{
	//用不到的参数用UNREFERENCED_PARAMETER括起来，否则报错，也在属性->C/C++ ->“警告视为错误”关掉，这里及之后的文章里都不关
	UNREFERENCED_PARAMETER(path);
	UNREFERENCED_PARAMETER(driver);
	//driver->DriverUnload = DriverUnload;
	
	if (!InitEpt()) {
		UseEpt = FALSE;
		Log("Ept初始化失败!");
		return STATUS_SUCCESS;
	}

	KeGenericCallDpc(LoadVT, NULL);

	//等一下所有核心加载完VT
	NdisStallExecution(50);
	//HookTest();
	
	Log("驱动加载");
	return STATUS_SUCCESS;
}

EXTERN_C NTSTATUS CreateDriver() {
	UNICODE_STRING DriverName;
	RtlInitUnicodeString(&DriverName, L"\\Driver\\MyVtDriver");
	return IoCreateDriver(&DriverName, &DriverEntry);
}