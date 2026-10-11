#define NOMINMAX
#include "Unreal/RivalsDiscovery.h"
#include "Unreal/UnrealTypes.h"
#include <Windows.h>
#include <cstring>
#include <sstream>
#include <set>
namespace Rivals {
static std::optional<ObjectArrayInfo> Override;
static uintptr_t NamePool = 0, Append = 0;
bool Read(uintptr_t at, void* out, size_t length) {
    SIZE_T got = 0;
    return Pointer(at) && length <= 0x800000000000ull - at && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), out, length, &got) && got == length;
}
bool Executable(uintptr_t at) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(reinterpret_cast<void*>(at), &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD)) return false;
    return (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}
static bool ValidPool(uintptr_t at) {
    const auto block = Load<uint32_t>(at + 8, Read), cursor = Load<uint32_t>(at + 12, Read);
    const auto first = Load<uintptr_t>(at + 16, Read);
    uint8_t bytes[16]{};
    if (!block || !cursor || !first || *block >= 8192 || *cursor > 262144 || !Read(*first, bytes, sizeof(bytes))) return false;
    for (uint32_t data = 2; data <= 8; ++data) {
        if (std::memcmp(bytes + data, "None", 4)) continue;
        uint16_t header = 0; std::memcpy(&header, bytes + data - 2, 2);
        if (header & 1) continue;
        for (uint32_t shift = 0; shift < 16; ++shift) if ((header >> shift) == 4) return true;
    }
    return false;
}
std::vector<Hit> Scan() {
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if (!Read(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000000
        || !Read(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return {};
    std::vector<Hit> hits;
    std::set<std::pair<Kind, uintptr_t>> seen;
    const auto headers = base + dos.e_lfanew + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (uint32_t s = 0; s < nt.FileHeader.NumberOfSections; ++s) {
        IMAGE_SECTION_HEADER section{};
        if (!Read(headers + s * sizeof(section), &section, sizeof(section))) break;
        if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE) || section.VirtualAddress >= nt.OptionalHeader.SizeOfImage) continue;
        const auto size = std::min(section.Misc.VirtualSize, nt.OptionalHeader.SizeOfImage - section.VirtualAddress);
        for (size_t off = 0; off < size; off += 0x100000) {
            const auto length = std::min<size_t>(0x100100, size - off);
            std::vector<uint8_t> bytes(length);
            // A failed page is excluded, including matches crossing that page.
            std::vector<bool> readable((length + 4095) / 4096, false);
            for (size_t page = 0; page < length; page += 4096) readable[page / 4096] = Read(base + section.VirtualAddress + off + page, bytes.data() + page, std::min<size_t>(4096, length - page));
            for (const auto& sig : Signatures) {
                std::vector<int> pattern; std::istringstream tokens(std::string{sig.pattern}); std::string token;
                while (tokens >> token) pattern.push_back(token[0] == '?' ? -1 : std::stoi(token, nullptr, 16));
                for (size_t pos = 0; pos + pattern.size() <= bytes.size(); ++pos) {
                    if (!readable[pos / 4096] || !readable[(pos + pattern.size() - 1) / 4096]) continue;
                    bool matches = true;
                    for (size_t n = 0; n < pattern.size(); ++n) if (pattern[n] >= 0 && pattern[n] != bytes[pos + n]) { matches = false; break; }
                    if (!matches) continue;
                    const auto instruction = base + section.VirtualAddress + off + pos + sig.ripOffset;
                    int32_t displacement = 0;
                    if (sig.kind != Kind::FNameAppendAddress) std::memcpy(&displacement, bytes.data() + pos + sig.ripOffset + 3, 4);
                    const auto target = sig.kind == Kind::FNameAppendAddress ? instruction : instruction + 7 + displacement;
                    if (target < base || target - base >= nt.OptionalHeader.SizeOfImage || !seen.insert({sig.kind, target}).second) continue;
                    hits.push_back({sig.kind, instruction, target});
                }
            }
        }
    }
    return hits;
}
std::optional<ObjectArrayInfo> FindObjects() {
    if (Override) return Validate(Override->address, Read, Executable);
    for (const auto& hit : Scan()) if (hit.kind == Kind::GObjectsStorage) if (auto info = Validate(hit.target, Read, Executable)) return info;
    return std::nullopt;
}
bool SetObjectArray(uint64_t rva, uint32_t per, uint32_t stride, uint32_t item, Layout layout) {
    if (rva > INT32_MAX) return false;
    const auto address = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + rva;
    auto info = Validate(address, Read, Executable);
    if (!info || info->perChunk != per || info->stride != stride || info->itemOffset != item || !(info->layout == layout)) return false;
    Override = info;
    return true;
}
bool SetNames(uint64_t poolRva, uint64_t appendRva) {
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (poolRva > INT32_MAX || appendRva > INT32_MAX || (poolRva && !ValidPool(base + poolRva)) || (appendRva && !Executable(base + appendRva))) return false;
    NamePool = poolRva ? base + poolRva : 0; Append = appendRva ? base + appendRva : 0;
    return true;
}
bool InitializeNames() {
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!NamePool && !Append) for (const auto& hit : Scan()) {
        if (hit.kind == Kind::GNamesAddress && ValidPool(hit.target)) NamePool = hit.target;
        if (hit.kind == Kind::FNameAppendAddress && Executable(hit.target)) Append = hit.target;
    }
    if (NamePool) { FName::Init(static_cast<int32_t>(NamePool - base), FName::EOffsetOverrideType::GNames, true); return true; }
    if (Append) { FName::Init(static_cast<int32_t>(Append - base), FName::EOffsetOverrideType::AppendString); return true; }
    return false;
}
}
