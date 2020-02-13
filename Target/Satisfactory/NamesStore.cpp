#include "NamesStore.hpp"
#include "PatternFinder.hpp"

class FNameEntry
{
public:
	FNameEntry* HashNext;
	int Index;
	char AnsiName[1024];
};

class TNameEntryArray
{
public:
	enum
	{
		ElementsPerChunk = 16 * 1024,
		ChunkTableSize = (4 * 1024 * 1024 + ElementsPerChunk - 1) / ElementsPerChunk
	};

	bool IsValidIndex(int Index)
	{
		return Index >= 0 && Index < NumElements && GetById(Index);
	}

	FNameEntry*& GetById(int Index)
	{
		return *GetItemPtr(Index);
	}

	FNameEntry** GetItemPtr(int Index)
	{
		auto ChunkIndex = Index / ElementsPerChunk;
		auto WithinChunkIndex = Index % ElementsPerChunk;
		auto Chunk = Chunks[ChunkIndex];
		return Chunk + WithinChunkIndex;
	}

	FNameEntry** Chunks[ChunkTableSize];
	int NumElements;
	int NumChunks;
};

TNameEntryArray* GlobalNames;

bool NamesStore::Initialize()
{
	auto Address = FindPattern(GetModuleHandleW(0), (unsigned char*)"\x48\x8B\x05\x00\x00\x00\x03\x48\x85\xC0\x0F\x85\x81", "xxx???xxxxxxx");
	GlobalNames = *(TNameEntryArray**)(Address + *(DWORD*)(Address + 0x3) + 0x7);

	return true;
}

void* NamesStore::GetAddress()
{
	return GlobalNames;
}

size_t NamesStore::GetNamesNum() const
{
	return GlobalNames->NumElements;
}

bool NamesStore::IsValid(size_t id) const
{
	return GlobalNames->IsValidIndex(id);
}

std::string NamesStore::GetById(size_t id) const
{
	return GlobalNames->GetById(id)->AnsiName;
}
