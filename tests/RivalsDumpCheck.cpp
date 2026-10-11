#define NOMINMAX
#include <Windows.h>
#include "Unreal/RivalsObjectArray.h"
#include "Json/json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <memory>
#include <iomanip>
#include <cstring>
namespace fs = std::filesystem;
struct FileMap {
    HANDLE file = INVALID_HANDLE_VALUE, map = nullptr;
    const uint8_t* bytes = nullptr;
    uint64_t size = 0;
    explicit FileMap(const fs::path& path) {
        file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        LARGE_INTEGER length{};
        if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &length) || !length.QuadPart) throw std::runtime_error("cannot open dump file");
        size = length.QuadPart;
        map = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!map) throw std::runtime_error("cannot map dump file");
        bytes = static_cast<const uint8_t*>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
        if (!bytes) throw std::runtime_error("cannot map dump view");
    }
    ~FileMap() { if (bytes) UnmapViewOfFile(bytes); if (map) CloseHandle(map); if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
};
struct Region { uintptr_t base; uint64_t size, offset; uint32_t chars; std::shared_ptr<FileMap> file; };
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: rivals-dump-check <dump-directory> <GObjects-storage-RVA>");
        const fs::path root(argv[1]);
        uintptr_t base = 0;
        std::ifstream manifest(root / "00_manifest.txt"); std::string line;
        while (std::getline(manifest, line)) if (line.starts_with("image_base")) base = std::stoull(line.substr(line.find("0x")), nullptr, 16);
        if (!base) throw std::runtime_error("manifest has no image base");
        std::vector<Region> regions;
        std::ifstream sections(root / "02_sections.txt");
        while (std::getline(sections, line)) {
            std::istringstream row(line); uint32_t index, prot, chars; std::string name; uintptr_t va; uint64_t size, raw;
            if (!(row >> std::dec >> index >> name >> std::hex >> va >> std::dec >> size >> raw >> std::hex >> prot >> chars)) continue;
            for (const auto& entry : fs::directory_iterator(root / "03_all_sections_dump")) {
                std::ostringstream prefix; prefix << std::setfill('0') << std::setw(2) << index << '_';
                if (entry.path().filename().string().starts_with(prefix.str()) && entry.path().extension() == ".bin") {
                    auto file = std::make_shared<FileMap>(entry.path());
                    regions.push_back({va, std::min(size, file->size), 0, chars, file}); break;
                }
            }
        }
        for (const auto& entry : fs::directory_iterator(root / "37_vmp_dump")) {
            if (!entry.path().filename().string().ends_with(".extras.json")) continue;
            auto bin = entry.path(); bin.replace_extension(".bin");
            auto file = std::make_shared<FileMap>(bin);
            std::ifstream input(entry.path()); auto data = nlohmann::json::parse(input);
            for (const auto& r : data["regions"]) {
                uint64_t offset = r["offset"], size = r["size"];
                if (offset <= file->size && size <= file->size - offset) regions.push_back({r["base"].get<uintptr_t>(), size, offset, 0, file});
            }
        }
        std::sort(regions.begin(), regions.end(), [](auto& a, auto& b) { return a.base < b.base; });
        auto read = [&](uintptr_t at, void* dst, size_t length) {
            auto next = std::upper_bound(regions.begin(), regions.end(), at, [](uintptr_t p, const Region& r) { return p < r.base; });
            if (next == regions.begin()) return false;
            const auto& r = *--next;
            if (at - r.base > r.size || length > r.size - (at - r.base)) return false;
            std::memcpy(dst, r.file->bytes + r.offset + at - r.base, length); return true;
        };
        auto executable = [&](uintptr_t at) { return std::any_of(regions.begin(), regions.end(), [&](const Region& r) { return at >= r.base && at - r.base < r.size && (r.chars & 0x20000000); }); };
        const auto rva = std::stoull(argv[2], nullptr, 0);
        const auto result = Rivals::Validate(base + rva, read, executable);
        if (!result) throw std::runtime_error("no valid indexed UObject array in captured memory");
        std::cout << "Validated captured GObjects RVA=0x" << std::hex << rva << " stride=0x" << result->stride << " item_offset=0x" << result->itemOffset
            << " class_offset=0x" << result->classOffset << " per_chunk=0x" << result->perChunk << " count=" << std::dec << result->count << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
