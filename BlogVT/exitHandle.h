#pragma once
// 一个简化的按索引读取 Guest GPR 的辅助函数
ULONG64 ReadGuestGpr(int idx, PGUEST_REGS regs) {
    switch (idx) {
    case 0: return regs->Rax;
    case 1: return regs->Rcx;
    case 2: return regs->Rdx;
    case 3: return regs->Rbx;
    case 4: return regs->Rsp; // 注意：RSP 是特殊成员
    case 5: return regs->Rbp;
    case 6: return regs->Rsi;
    case 7: return regs->Rdi;
    case 8: return regs->R8;
    case 9: return regs->R9
        // ... R9 ~ R15
    default: return 0; // 不应发生
    }
}

void HandleCrAccess() {
    ULONG64 qual = 0;
    __vmx_vmread(VmExitQualification, &qual);

    UINT32 crNum = qual & 0xF;           // 3:0 0=CR0, 3=CR3, 4=CR4, 8=CR8；CLTS/LMSW 为 0
    UINT32 access = (qual >> 4) & 0x3;    // 5:4 0 = MOV to CR，1 = MOV from CR，2 = CLTS，3 = LMSW
    UINT32 gpr = (qual >> 8) & 0xF;    // 11:8 0:rax 1:rcx

    switch (access) {
    case 0: // MOV to CR
        HandleMovToCr(crNum, gpr);
        break;
    case 1: // MOV from CR
        HandleMovFromCr(crNum, gpr);
        break;
    case 2: // CLTS
        HandleClts();
        break;
    case 3: // LMSW
        HandleLmsw(qual);
        break;
    }
}