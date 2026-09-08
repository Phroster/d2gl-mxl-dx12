#pragma once
#include <windows.h>
#include <cstdint>

namespace mxl::sound_cancel {
using FreeFn=void(__fastcall*)(void*);
using CancelFn=void(WINAPI*)(void*);
using PriorityFn=void(WINAPI*)(void*,int);
struct Api {FreeFn release=nullptr;CancelFn cancel=nullptr;PriorityFn prioritize=nullptr;};
bool start();
#ifdef MXL_SOUND_CANCEL_TEST
bool test_install(void** free_slot,void** cancel_slot,Api api,uintptr_t free_caller,uintptr_t cancel_caller,bool enabled);
void test_release(void* job,uintptr_t caller);
void test_cancel(void* buffer,uintptr_t caller);
void test_enabled(bool enabled);
bool test_code_ready(uintptr_t client,uintptr_t fog,uintptr_t storm);
#endif
}
