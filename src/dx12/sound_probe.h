#pragma once
#include <windows.h>
#include <cstdint>

namespace mxl::diag {
enum class NativeSound : uint32_t {
    AsyncLoad, AsyncBuffer, AsyncFree, ClientOpen, ClientRead, ClientClose,
    LockWait, LockHold, SoundWait, SoundSleep, MusicBegin, MusicEnd, MusicPosition,
    ClientWait, ClientSleep, Count
};
bool start_sound_probe();
// Native scopes may nest. Object IDs are pointers/handles and may be reused.
void native_sound_call(NativeSound operation, uint64_t began, uint64_t ended,
    uintptr_t caller, uintptr_t object, uint32_t argument, uintptr_t result,
    const char* path = "", unsigned path_status = 3) noexcept;

#ifdef MXL_SOUND_TEST
// Client: async load/buffer/free, open/read/close. Sound: enter/leave,
// wait/sleep, music begin/end/position. Client: wait/sleep.
bool test_sound_imports(void*** slots, void** originals, void* first_lock, void* second_lock);
#endif
}
