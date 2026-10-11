#pragma once
// Signatures from natimerry/rivals_offset_finder @ df06adfb67277dd27e004107ea8af5b8ca5010d7
#include <array>
#include <cstdint>
#include <string_view>
namespace Rivals {
enum class Kind { GObjectsStorage, GNamesAddress, FNameAppendAddress };
struct Signature { Kind kind; std::string_view pattern; uint32_t ripOffset; };
inline constexpr std::array Signatures = {
Signature{Kind::GNamesAddress, "48 89 44 24 48 48 D1 F9 0F 57 C0 0F 11 44 24 58 89 4C 24 50 48 8D 0D ? ? ? ?", 20},
Signature{Kind::GNamesAddress, "48 8D 0D ? ? ? ? E8 ? ? ? ? 83 3D ? ? ? ? FF 0F 85 ? ? ? ? 48 8D 0D ? ? ? ?", 25},
Signature{Kind::GNamesAddress, "48 8D 0D ? ? ? ? E9 ? ? ? ?", 0},
Signature{Kind::GObjectsStorage, "3B 05 ? ? ? ? 7D ? 8B C8 0F B7 D0 48 8B 05 ? ? ? ?", 13},
Signature{Kind::GObjectsStorage, "7D ? 8B C8 0F B7 D0 48 8B 05 ? ? ? ?", 7},
Signature{Kind::GObjectsStorage, "7D ? 8B D0 0F B7 C8 48 8B 05 ? ? ? ?", 7},
Signature{Kind::GObjectsStorage, "7D ? 8B C8 0F B7 D8 48 8B 05 ? ? ? ?", 7},
Signature{Kind::FNameAppendAddress, "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 80 3D ?? ?? ?? ?? 00 48 8B F2 8B 59 08 48 8B F9 74 09 4C 8D 05 ?? ?? ?? ?? EB 16", 0}
};
}
