#pragma once
// Fault-contained, independently testable validation for signature-derived storage.
#include <array>
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
namespace Rivals {
struct Layout {
    uint32_t objects, maximum, count, maxChunks, chunks;
    bool operator==(const Layout&) const = default;
};
inline constexpr std::array Layouts = {
    Layout{0, 0x10, 0x14, 0x18, 0x1c},
    Layout{0, 8, 0xc, 0x10, 0x14},
    Layout{0, 0xc, 8, 0x14, 0x10}
};
struct ObjectArrayInfo {
    uintptr_t address = 0, table = 0;
    uint32_t maximum = 0, count = 0, maxChunks = 0, chunks = 0;
    uint32_t perChunk = 0, stride = 0, itemOffset = 0, classOffset = 0;
    Layout layout{};
};
inline bool Pointer(uintptr_t p) { return p >= 0x10000 && p < 0x800000000000ull; }
template<typename T, typename Read>
std::optional<T> Load(uintptr_t address, Read&& read) {
    T value{};
    return Pointer(address) && read(address, &value, sizeof(value)) ? std::optional<T>(value) : std::nullopt;
}
template<typename Read, typename Executable>
std::optional<ObjectArrayInfo> Validate(uintptr_t address, Read&& read, Executable&& executable) {
    auto ptr = [&](uintptr_t at) { auto p = Load<uintptr_t>(at, read); return p && Pointer(*p) ? p : std::nullopt; };
    auto vtable = [&](uintptr_t obj) { auto vt = ptr(obj); auto fn = vt ? ptr(*vt) : std::nullopt; return fn && executable(*fn); };
    for (const auto& layout : Layouts) {
        auto table = ptr(address + layout.objects);
        auto count = Load<uint32_t>(address + layout.count, read), maximum = Load<uint32_t>(address + layout.maximum, read);
        auto chunks = Load<uint32_t>(address + layout.chunks, read), maxChunks = Load<uint32_t>(address + layout.maxChunks, read);
        if (!table || !count || !maximum || !chunks || !maxChunks || *count < 2048 || *count > 100000000 || *maximum < *count || *maximum > 100000000
            || !*chunks || *chunks > 4096 || *maxChunks < *chunks || *maxChunks > 4096 || *maximum % *maxChunks) continue;
        const auto per = *maximum / *maxChunks;
        if (per != 0x4000 && per != 0x8000 && per != 0x10000 && per != 0x20000 && per != 0x40000 && per != 0x80000) continue;
        if (uint64_t(*chunks) * per < *count || *chunks > *count / per + 1) continue;
        bool allChunks = true;
        for (uint32_t i = 0; i < *chunks; ++i) if (!ptr(*table + i * sizeof(uintptr_t))) { allChunks = false; break; }
        if (!allChunks) continue;
        std::set<uint32_t> indices;
        for (uint32_t i = 0; i < 64; ++i) { indices.insert(i); indices.insert(uint64_t(i) * (*count - 1) / 63); }
        for (uint32_t stride : {0x20, 0x18, 0x28, 0x10, 0x30, 0x38}) for (uint32_t item : {0, 8, 16}) {
            if (item + sizeof(uintptr_t) > stride) continue;
            std::set<uintptr_t> objects;
            std::map<uint32_t, uint32_t> classes;
            uint32_t checked = 0;
            for (auto index : indices) {
                const auto chunk = ptr(*table + (index / per) * sizeof(uintptr_t));
                const auto object = chunk ? ptr(*chunk + uintptr_t(index % per) * stride + item) : std::nullopt;
                if (!object) continue;
                ++checked;
                auto actualIndex = Load<uint32_t>(*object + 0xc, read);
                if (!actualIndex || *actualIndex != index || !vtable(*object) || !objects.insert(*object).second) continue;
                for (uint32_t off = 0x10; off <= 0x50; off += 8) {
                    const auto cls = ptr(*object + off);
                    const auto meta = cls ? ptr(*cls + off) : std::nullopt;
                    if (meta && vtable(*cls) && ptr(*meta + off) == meta) ++classes[off];
                }
            }
            if (objects.size() < 16 || objects.size() * 4 < checked * 3 || classes.empty()) continue;
            auto best = std::max_element(classes.begin(), classes.end(), [](auto& a, auto& b) { return a.second < b.second; });
            if (best->second < 8) continue;
            return ObjectArrayInfo{address, *table, *maximum, *count, *maxChunks, *chunks, per, stride, item, best->first, layout};
        }
    }
    return std::nullopt;
}
}
