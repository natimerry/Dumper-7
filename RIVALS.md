# Marvel Rivals / DumpAll companion

Fork of Encryqed/Dumper-7 at `dd8fe34df8283378a1055386eee195005a100924`.
Signatures come from
[`natimerry/rivals_offset_finder`](https://github.com/natimerry/rivals_offset_finder)
at `df06adfb67277dd27e004107ea8af5b8ca5010d7`.

The GObjects signatures load the **chunk-table storage field** with a RIP-relative
`mov`. The resolved address is an inline object-array header, not the chunk table
itself and not necessarily the start of an enclosing FUObjectArray. No fixed
game RVA is compiled in. The scanner examines executable sections regardless of
their names and resolves signed displacements. All candidates require readable
chunks, coherent counts, distinct UObjects with matching InternalIndex, executable
vtable functions and a common self-classed UClass relationship. Sampling spans the
array; the item size/offset and chunk size are derived, not forced.

The default CMake build uses `DUMPER7_LIBRARY_MODE=ON`. DllMain performs no SDK
generation and starts no worker. Generation runs synchronously through exports;
DumpAll can safely unload the companion after the call returns. There is no
console allocation, stream redirection, keyboard wait, or ProcessEvent call to
obtain the game name/version in this path.

```powershell
cmake -S . -B out/rivals -A x64 -DDUMPER7_LIBRARY_MODE=ON
cmake --build out/rivals --config Release --parallel 4
ctest --test-dir out/rivals -C Release --output-on-failure
```

The Visual Studio project also defaults to library mode. CMake builds may opt
into the upstream standalone worker with `-DDUMPER7_LIBRARY_MODE=OFF`.

API v1 (C ABI, Windows x64):

```cpp
uint32_t Dumper7_ApiVersion();
void Dumper7_SetOutputRoot(const wchar_t* cppRoot, const wchar_t* commonRoot);
bool Dumper7_SetObjectArray(uint64_t storageRva, uint32_t perChunk,
    uint32_t itemSize, uint32_t itemOffset, uint32_t objectsOffset,
    uint32_t maxElementsOffset, uint32_t numElementsOffset,
    uint32_t maxChunksOffset, uint32_t numChunksOffset);
bool Dumper7_SetNames(uint64_t namePoolRva, uint64_t appendStringRva);
int Dumper7_Regenerate();
```

Offsets passed through the API are revalidated. RVAs are relative to the main
executable. Zero name offsets request discovery. Output directories must be
unique per capture; the generator retains upstream folder backup behavior.
Call generation once per loaded DLL instance because upstream manager caches
are global. Reload the library for another capture.

Return codes: `0` generated SDK header exists; `-1` C++ exception; `-2` access
violation; `-3` generation already attempted; `-4` no validated GObjects;
`-5` expected SDK header absent. A generation failure must not be retried in the
same DLL instance. GObjects discovery failure no longer calls `exit(1)`.

The isolated validator test covers chunk boundaries, item stride, false pointer
arrays, non-executable vtables and unreadable pointers. `rivals-dump-check` uses
the same production validator against captured section and extras files:

```powershell
.\out\rivals\bin\Release\rivals-dump-check.exe C:\path\to\dump 0xF8F74F0
```

That RVA was validated against the September 16 local dump with 266,367 objects,
0x20-byte items, item offset 0, class offset 0x10 and 0x10000 items per chunk.
It is evidence for that capture only. Current game RVAs must be rediscovered.
Full SDK generation in the running game remains unverified.
