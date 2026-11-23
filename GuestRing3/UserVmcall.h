#pragma once
#include<Windows.h>
extern "C" ULONG_PTR UserVmcall(
    ULONG_PTR command,
    ULONG_PTR arg1,
    ULONG_PTR arg2
);