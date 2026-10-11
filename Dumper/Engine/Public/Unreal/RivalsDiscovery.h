#pragma once
#include "RivalsObjectArray.h"
#include "RivalsSignatures.h"
#include <vector>
namespace Rivals {
struct Hit { Kind kind; uintptr_t instruction, target; };
bool Read(uintptr_t address, void* out, size_t length);
bool Executable(uintptr_t address);
std::vector<Hit> Scan();
std::optional<ObjectArrayInfo> FindObjects();
bool InitializeNames();
bool SetObjectArray(uint64_t rva, uint32_t per, uint32_t stride, uint32_t item, Layout layout);
bool SetNames(uint64_t poolRva, uint64_t appendRva);
}
