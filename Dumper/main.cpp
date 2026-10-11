#include <Windows.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <mutex>
#include "Unreal/RivalsDiscovery.h"

#include "Generators/CppGenerator.h"
#include "Generators/MappingGenerator.h"
#include "Generators/IDAMappingGenerator.h"
#include "Generators/DumpspaceGenerator.h"

#include "Generators/Generator.h"

enum class EFortToastType : uint8
{
    Default                                  = 0,
    Subdued                                  = 1,
    Impactful                                = 2,
    Lock                                     = 3,
    EFortToastType_MAX                       = 4,
};

DWORD MainThread(HMODULE Module)
{
	AllocConsole();
	FILE* Dummy;
	freopen_s(&Dummy, "CONIN$", "r", stdin);
	freopen_s(&Dummy, "CONOUT$", "w", stderr);
	std::cerr.clear(); // clear internal error flags on cerr after redirect
	std::cerr << std::boolalpha << std::hex;

	std::cerr << "Initializing [Dumper-7]\n";

	Settings::Config::Load();
	Settings::Config::DelayDumperStart();

	std::cerr << "Started Generation [Dumper-7]!\n";
	auto DumpStartTime = std::chrono::high_resolution_clock::now();

	try {
		Generator::InitEngineCore();
		Generator::InitInternal();

		if (Settings::Generator::GameName.empty() && Settings::Generator::GameVersion.empty())
		{
			// Only Possible in Main()
			FString Name;
			FString Version;
			UEClass Kismet = ObjectArray::FindClassFast("KismetSystemLibrary");
			UEFunction GetGameName = Kismet.GetFunction("KismetSystemLibrary", "GetGameName");
			UEFunction GetEngineVersion = Kismet.GetFunction("KismetSystemLibrary", "GetEngineVersion");

			Kismet.ProcessEvent(GetGameName, &Name);
			Kismet.ProcessEvent(GetEngineVersion, &Version);

			Settings::Generator::GameName = Name.ToString();
			Settings::Generator::GameVersion = Version.ToString();
		}

		std::cerr << "GameName: " << Settings::Generator::GameName << "\n";
		std::cerr << "GameVersion: " << Settings::Generator::GameVersion << "\n\n";

		std::cerr << "FolderName: " << (Settings::Generator::GameVersion + '-' + Settings::Generator::GameName) << "\n\n";

		Generator::Generate<CppGenerator>();
		Generator::Generate<MappingGenerator>();
		Generator::Generate<IDAMappingGenerator>();
		Generator::Generate<DumpspaceGenerator>();
	} catch (const std::exception& Error) {
		std::cerr << "Dumper-7 generation failed: " << Error.what() << '\n';
	}

	auto DumpFinishTime = std::chrono::high_resolution_clock::now();

	std::chrono::duration<double, std::milli> DumpTime = DumpFinishTime - DumpStartTime;

	std::cerr << "\n\nGenerating SDK took (" << DumpTime.count() << "ms)\n\n\n";

	if (Settings::Debug::bExecuteSDKTestScript)
	{
		/* Executes a python script to test if the SDK compiles correctly. */
		CppGenerator::ExecuteSDKCompilationTestScript();
	}

	std::cerr << "\n\nPress F6 to unload\n\n\n";

	while (true)
	{
		if (GetAsyncKeyState(VK_F6) & 1)
		{
			fclose(stderr);
			if (Dummy) 
			{
				fclose(Dummy);
			}
			FreeConsole();

			FreeLibraryAndExitThread(Module, 0);
		}

		Sleep(100);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
#if defined(DUMPER7_LIBRARY_MODE)
	if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(hModule);
	return TRUE;
#else
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
		break;
	}

	return TRUE;
#endif
}

static std::mutex LibraryMutex;
static bool GenerationAttempted = false;
extern "C" __declspec(dllexport) uint32_t Dumper7_ApiVersion() {
#if defined(DUMPER7_LIBRARY_MODE)
	return 1;
#else
	return 0;
#endif
}
extern "C" __declspec(dllexport) void Dumper7_SetOutputRoot(const wchar_t* Cpp, const wchar_t* Common) {
	std::lock_guard guard(LibraryMutex);
	if (!GenerationAttempted && Cpp && Common) Generator::SetOutputRoots(Cpp, Common);
}
extern "C" __declspec(dllexport) bool Dumper7_SetObjectArray(uint64_t Rva, uint32_t Per, uint32_t Stride, uint32_t Item,
	uint32_t Objects, uint32_t Max, uint32_t Num, uint32_t MaxChunks, uint32_t NumChunks) {
	std::lock_guard guard(LibraryMutex);
	return !GenerationAttempted && Rivals::SetObjectArray(Rva, Per, Stride, Item, {Objects, Max, Num, MaxChunks, NumChunks});
}
extern "C" __declspec(dllexport) bool Dumper7_SetNames(uint64_t PoolRva, uint64_t AppendRva) {
	std::lock_guard guard(LibraryMutex);
	return !GenerationAttempted && Rivals::SetNames(PoolRva, AppendRva);
}
static int GenerateLibrarySDK() {
	{
		std::lock_guard guard(LibraryMutex);
		// Manager caches are process-global in upstream; do not reinitialize them in one DLL instance.
		if (GenerationAttempted) return -3;
		GenerationAttempted = true;
	}
	try {
		if (!Rivals::FindObjects()) return -4;
		if (Settings::Generator::GameName.empty()) Settings::Generator::GameName = "MarvelRivals";
		if (Settings::Generator::GameVersion.empty()) Settings::Generator::GameVersion = "runtime";
		Generator::InitEngineCore();
		Generator::InitInternal();
		Generator::Generate<CppGenerator>();
		Generator::Generate<MappingGenerator>();
		Generator::Generate<IDAMappingGenerator>();
		Generator::Generate<DumpspaceGenerator>();
		return fs::exists(CppGenerator::MainFolder / "SDK.hpp") ? 0 : -5;
	} catch (const std::exception& Error) {
		std::cerr << "Dumper-7 generation failed: " << Error.what() << '\n';
		return -1;
	}
}
extern "C" __declspec(dllexport) int Dumper7_Regenerate() {
	// Keep destructible C++ locals in the inner function (MSVC C2712).
	__try { return GenerateLibrarySDK(); }
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { return -2; }
}
