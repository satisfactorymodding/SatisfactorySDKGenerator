// Unreal Engine SDK Generator
// by KN4CK3R
// https://www.oldschoolhack.me

#define NOMINMAX
#include <windows.h>

#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <bitset>
namespace fs = std::experimental::filesystem;
#include "cpplinq.hpp"

#include "Logger.hpp"

#include "IGenerator.hpp"
#include "ObjectsStore.hpp"
#include "NamesStore.hpp"
#include "Package.hpp"
#include "NameValidator.hpp"
#include "PrintHelper.hpp"

extern IGenerator* generator;

/// <summary>
/// Dumps the objects and names to files.
/// </summary>
/// <param name="path">The path where to create the dumps.</param>
void Dump(const fs::path& path)
{
	{
		std::ofstream o(path / "ObjectsDump.txt");
		tfm::format(o, "Address: 0x%P\n\n", ObjectsStore::GetAddress());

		for (auto obj : ObjectsStore())
		{
			tfm::format(o, "[%06i] %-100s 0x%P\n", obj.GetIndex(), obj.GetFullName(), obj.GetAddress());
		}
	}

	{
		std::ofstream o(path / "NamesDump.txt");
		tfm::format(o, "Address: 0x%P\n\n", NamesStore::GetAddress());

		for (auto name : NamesStore())
		{
			tfm::format(o, "[%06i] %s\n", name.Index, name.Name);
		}
	}
}

/// <summary>
/// Generates the sdk header.
/// </summary>
/// <param name="path">The path where to create the sdk header.</param>
/// <param name="processedObjects">The list of processed objects.</param>
/// <param name="packageOrder">The package order info.</param>
void SaveSDKHeader(const fs::path& path, const std::unordered_map<UEObject, bool>& processedObjects, const std::vector<Package>& packages)
{
	std::ofstream os(path / "SDK.hpp");

	os << "#pragma once\n\n"
		<< tfm::format("// %s SDK\n\n", generator->GetGameName());

	//include the basics
	{
		{
			std::ofstream os2(path / "SDK" / tfm::format("%s_Basic.hpp", generator->GetGameNameShort()));

			std::vector<std::string> includes{ { "<unordered_set>" }, { "<string>" } };

			auto&& generatorIncludes = generator->GetIncludes();
			includes.insert(includes.end(), std::begin(generatorIncludes), std::end(generatorIncludes));

			PrintFileHeader(os2, includes, true);

			os2 << generator->GetBasicDeclarations() << "\n";

			PrintFileFooter(os2);

			os << "\n#include \"SDK/" << tfm::format("%s_Basic.hpp", generator->GetGameNameShort()) << "\"\n";
		}
		{
			std::ofstream os2(path / "SDK" / tfm::format("%s_Basic.cpp", generator->GetGameNameShort()));

			std::vector<std::string> includes2{ 
				{ tfm::format("%s_CoreUObject_classes.hpp", generator->GetGameNameShort()) }, 
				{ tfm::format("%s_Engine_classes.hpp", generator->GetGameNameShort()) } };

			PrintFileHeader(os2, includes2, false);

			os2 << generator->GetBasicDefinitions() << "\n";

			PrintFileFooter(os2);
		}
	}

	using namespace cpplinq;

	//check for missing structs
	const auto missing = from(processedObjects) >> where([](auto&& kv) { return kv.second == false; });
	if (missing >> any())
	{
		std::ofstream os2(path / "SDK" / tfm::format("%s_MISSING.hpp", generator->GetGameNameShort()));

		PrintFileHeader(os2, true);

		for (auto&& s : missing >> select([](auto&& kv) { return kv.first.Cast<UEStruct>(); }) >> experimental::container())
		{
			os2 << "// " << s.GetFullName() << "\n// ";
			os2 << tfm::format("0x%04X\n", s.GetPropertySize());

			os2 << "struct " << MakeValidName(s.GetNameCPP()) << "\n{\n";
			os2 << "\tunsigned char UnknownData[0x" << tfm::format("%X", s.GetPropertySize()) << "];\n};\n\n";
		}

		PrintFileFooter(os2);

		os << "\n#include \"SDK/" << tfm::format("%s_MISSING.hpp", generator->GetGameNameShort()) << "\"\n";
	}

	os << "\n";

	for (auto&& package : packages)
	{
		os << R"(#include "SDK/)" << GenerateFileName(FileContentType::Classes, package) << "\"\n";
	}

	os << "\n" << "namespace " << generator->GetNamespaceName() << "\n";

	os << R"({
	static bool DataCompare(PBYTE pData, PBYTE bSig, const char* szMask)
	{
		for (; *szMask; ++szMask, ++pData, ++bSig)
		{
			if (*szMask == 'x' && *pData != *bSig)
				return false;
		}
		return (*szMask) == 0;
	}

	static DWORD_PTR FindPattern(DWORD_PTR dwAddress, DWORD dwSize, const char* pbSig, const char* szMask, long offset)
	{
		size_t length = strlen(szMask);
		for (size_t i = NULL; i < dwSize - length; i++)
		{
			if (DataCompare((PBYTE)dwAddress + i, (PBYTE)pbSig, szMask))
				return dwAddress + i + offset;
		}
		return 0;
	}

	static void InitSDK()
	{
		DWORD_PTR BaseAddress = (DWORD_PTR)GetModuleHandle(NULL);

		MODULEINFO ModuleInfo;
		GetModuleInformation(GetCurrentProcess(), (HMODULE)BaseAddress, &ModuleInfo, sizeof(ModuleInfo));

		auto GNamesAddress = FindPattern(BaseAddress, ModuleInfo.SizeOfImage,
			"\x48\x8B\x05\x00\x00\x00\x03\x48\x85\xC0\x0F\x85\x81", "xxx???xxxxxxx", 0);
		auto GNamesOffset = *reinterpret_cast<uint32_t*>(GNamesAddress + 3);
		FName::GNames = *reinterpret_cast<TNameEntryArray**>(GNamesAddress + 7 + GNamesOffset);

		auto GObjectsAddress = FindPattern(BaseAddress, ModuleInfo.SizeOfImage,
			"\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x00\x00\x00\x0E\x00\x00\xE8", "xxx????x???xx???xxxx", 0);
		auto GObjectsOffset = *reinterpret_cast<uint32_t*>(GObjectsAddress + 3);
		UObject::GObjects = reinterpret_cast<FUObjectArray*>(GObjectsAddress + 7 + GObjectsOffset);

		auto GWorldAddress = FindPattern(BaseAddress, ModuleInfo.SizeOfImage,
			"\x48\x8B\x1D\x00\x00\x00\x04\x48\x85\xDB\x74\x3B", "xxx???xxxxxx", 0);
		auto GWorldOffset = *reinterpret_cast<uint32_t*>(GWorldAddress + 3);
		UWorld::GWorld = reinterpret_cast<UWorld**>(GWorldAddress + 7 + GWorldOffset);
	}
})";
}

/// <summary>
/// Process the packages.
/// </summary>
/// <param name="path">The path where to create the package files.</param>
void ProcessPackages(const fs::path& path)
{
	using namespace cpplinq;

	const auto sdkPath = path / "SDK";
	fs::create_directories(sdkPath);

	std::vector<Package> packages;

	std::unordered_map<UEObject, bool> processedObjects;

	auto packageObjects = from(ObjectsStore())
		>> select([](auto&& o) { return o.GetPackageObject(); })
		>> where([](auto&& o) { return o.IsValid(); })
		>> distinct()
		>> to_vector();

	for (auto obj : packageObjects)
	{
		Package package(obj);

		package.Process(processedObjects);
		if (package.Save(sdkPath))
		{
			packages.emplace_back(std::move(package));
		}
	}

	SaveSDKHeader(path, processedObjects, packages);
}

DWORD WINAPI OnAttach(LPVOID lpParameter)
{
	if (!ObjectsStore::Initialize())
	{
		MessageBoxA(nullptr, "ObjectsStore::Initialize failed", "Error", 0);
		return -1;
	}
	if (!NamesStore::Initialize())
	{
		MessageBoxA(nullptr, "NamesStore::Initialize failed", "Error", 0);
		return -1;
	}

	if (!generator->Initialize(lpParameter))
	{
		MessageBoxA(nullptr, "Initialize failed", "Error", 0);
		return -1;
	}

	fs::path outputDirectory(generator->GetOutputDirectory());
	if (!outputDirectory.is_absolute())
	{
		char buffer[2048];
		if (GetModuleFileNameA(static_cast<HMODULE>(lpParameter), buffer, sizeof(buffer)) == 0)
		{
			MessageBoxA(nullptr, "GetModuleFileName failed", "Error", 0);
			return -1;
		}

		outputDirectory = fs::path(buffer).remove_filename() / outputDirectory;
	}

	outputDirectory /= generator->GetGameNameShort();
	fs::create_directories(outputDirectory);

	std::ofstream log(outputDirectory / "Generator.log");
	Logger::SetStream(&log);

	if (generator->ShouldDumpArrays())
	{
		Dump(outputDirectory);
	}

	fs::create_directories(outputDirectory);

	const auto begin = std::chrono::system_clock::now();

	ProcessPackages(outputDirectory);

	Logger::Log("Finished, took %d seconds.", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - begin).count());

	Logger::SetStream(nullptr);

	MessageBoxA(nullptr, "Finished!", "Info", 0);

	return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);

		CreateThread(nullptr, 0, OnAttach, hModule, 0, nullptr);

		return TRUE;
	}

	return FALSE;
}
