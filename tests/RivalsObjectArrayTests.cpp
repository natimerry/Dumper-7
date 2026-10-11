#define NOMINMAX
#include <Windows.h>
#include "Unreal/RivalsObjectArray.h"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstring>
static void Native() {}
static bool Read(uintptr_t at, void* buffer, size_t n) {
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), buffer, n, &read) && read == n;
}
static void Require(bool value) { if (!value) throw std::runtime_error("Rivals validator assertion failed"); }
int main() {
    const uintptr_t vtable[] = {reinterpret_cast<uintptr_t>(&Native)};
    std::array<uintptr_t, 12> meta{};
    meta[0] = reinterpret_cast<uintptr_t>(vtable); meta[2] = reinterpret_cast<uintptr_t>(meta.data());
    std::vector<std::array<uintptr_t, 8>> objects(65537);
    std::vector<uintptr_t> first(65536 * 4), second(4);
    for (size_t i = 0; i < objects.size(); ++i) {
        objects[i][0] = reinterpret_cast<uintptr_t>(vtable); objects[i][1] = i << 32; objects[i][2] = reinterpret_cast<uintptr_t>(meta.data());
        (i < 65536 ? first[i * 4] : second[0]) = reinterpret_cast<uintptr_t>(objects[i].data());
    }
    const uintptr_t table[] = {reinterpret_cast<uintptr_t>(first.data()), reinterpret_cast<uintptr_t>(second.data())};
    std::array<uint8_t, 40> array{};
    const uintptr_t tableAddress = reinterpret_cast<uintptr_t>(table);
    std::memcpy(array.data(), &tableAddress, 8);
    auto put = [&](size_t off, uint32_t value) { std::memcpy(array.data() + off, &value, 4); };
    put(0x10, 65536 * 4); put(0x14, 65537); put(0x18, 4); put(0x1c, 2);
    const auto at = reinterpret_cast<uintptr_t>(array.data());
    auto executable = [&](uintptr_t p) { return p == reinterpret_cast<uintptr_t>(&Native); };
    auto hit = Rivals::Validate(at, Read, executable);
    Require(hit && hit->stride == 0x20 && hit->itemOffset == 0 && hit->perChunk == 65536 && hit->count == 65537);
    Require(!Rivals::Validate(at, Read, [](uintptr_t) { return false; }));
    put(0x1c, 1); Require(!Rivals::Validate(at, Read, executable)); put(0x1c, 2);
    for (size_t i = 0; i < first.size(); i += 4) first[i] = reinterpret_cast<uintptr_t>(objects[0].data());
    Require(!Rivals::Validate(at, Read, executable));
    Require(!Rivals::Validate(0x10000, Read, executable));
    std::cout << "Rivals validator: indexed chunk crossing, stride, executable vtable, false array and unreadable pointer checks passed\n";
}
