#include <Windows.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <algorithm>
#include <fstream>
#include <filesystem>

#include <xxhash.h>
#include <lz4.h>
#include <lz4hc.h>
#include <tbb/tbb.h>
#include <tbb/task_scheduler_init.h>

#include "../../VFSLib/include/json.hpp"
using json = nlohmann::json;

#include "../../VFSLib/include/VFSArchive.h"
#include "../../VFSLib/include/VFSFile.h"
#include "../../VFSLib/include/VFSPack.h"
using namespace VFS;

void CheckSpecificFileIntegrity(CVFSPack * vfs, std::shared_ptr <CVFSArchive> archive, const SFileInformation& file, const std::wstring& targetdir)
{
    vfs->Log(0, "PACKED | %ls - %p - %u - %u -- Raw: %u Compressed: %u Crypted(Final): %u",
        file.filename, file.hash, file.version, file.flags, file.rawsize, file.compressedsize, file.cryptedsize);

	auto target = targetdir + L"\\" + file.filename;

	auto stream = archive->Open(file.filename);
	if (!stream || !stream.get())
    {
        vfs->Log(0, "Virtual file: %ls can NOT open!", file.filename);
        return;
    }

	uint8_t* pbData = new uint8_t[stream->GetSize() + 1];
	memcpy(pbData, stream->GetData(), stream->GetSize());

	pbData[stream->GetSize()] = 0;

	auto unpackedfile = std::make_shared<CVFSFile>();;
	if (unpackedfile->Create(target) == false)
	{
		vfs->Log(0, "Unpacked file: %ls can NOT created!", target.c_str());
		return;
	}

	auto writecount = unpackedfile->Write(pbData, stream->GetSize());
	if (writecount != stream->GetSize())
	{
		vfs->Log(0, "Unpacked content can not writed %u/%u", writecount, stream->GetSize());
		return;
	}
	unpackedfile->Close();
}
void CheckArchivedFilesIntegrity(CVFSPack * vfs, std::shared_ptr <CVFSArchive>& archive, const std::wstring& targetdir)
{
	CreateDirectoryW(targetdir.c_str(), nullptr);

    auto files = archive->EnumerateFiles();
    for (const auto& file : files)
    {
        CheckSpecificFileIntegrity(vfs, archive, file, targetdir);
    }
}
void CheckAllArchiveIntegritys(CVFSPack * vfs)
{

}

bool CreateTestArchive(CVFSPack * vfs, const std::wstring& strArchiveName, const uint8_t * pArchiveKey, 
	const std::wstring& stArchiveDirectory, const std::wstring& stVisualDirectory, uint32_t uVersion, uint32_t uFlag)
{
	vfs->Log(0, "Curr archive %ls: %ls", strArchiveName.c_str(), stArchiveDirectory.c_str());

	DeleteFileW(strArchiveName.c_str());

	auto file = std::make_shared<CVFSFile>();
	if (!file || !file.get())
	{
		vfs->Log(1, "VFS File container can NOT allocated");
		return false;
	}

	if (file->Create(strArchiveName) == false)
	{
		vfs->Log(1, "File can NOT created");
		return false;
	}

	auto archive = std::make_shared<CVFSArchive>();
	if (!archive || !archive.get())
	{
		vfs->Log(1, "VFS Archive container can NOT allocated");
		return false;
	}

	vfs->SetArchiveKey(strArchiveName, pArchiveKey);

	if (archive->Create(file, pArchiveKey) == false)
	{
		vfs->Log(1, "Archive can NOT created");
		return false;
	}

	uint32_t counter = 0;

	auto workingdirectory = vfs->GetWorkingDirectory();
	for (const auto& entry : std::filesystem::recursive_directory_iterator(stArchiveDirectory))
	{
		std::wstring wstrEntryFile = entry.path().c_str();

		vfs->Log(0, "%ls", wstrEntryFile.c_str());

		if (entry.is_directory())
			continue;

		auto namewithoutpath = wstrEntryFile;
		std::size_t pos = namewithoutpath.find(stArchiveDirectory + L"\\");
		if (pos != std::wstring::npos)
		{
			namewithoutpath.erase(pos, stArchiveDirectory.size() + 1);
		}

//		vfs->Log(0, "%ls", namewithoutpath.c_str());

		auto visualdirectory = stVisualDirectory;
		if (visualdirectory.length() && visualdirectory[visualdirectory.length() - 1] != L'\\' && visualdirectory[visualdirectory.length() - 1] != L'/')
		{
			for (size_t i = 0; i < visualdirectory.length(); ++i)
			{
				if (visualdirectory[i] == L'\\')
				{
					visualdirectory[i] = L'/';
				}
			}
		}

		for (size_t i = 0; i < namewithoutpath.length(); ++i)
		{
			if (namewithoutpath[i] == L'\\')
			{
				namewithoutpath[i] = L'/';
			}
		}

//		vfs->Log(0, "'%ls'->'%ls'", visualdirectory.c_str(), namewithoutpath.c_str());

		auto entryfile = std::make_unique<CVFSFile>();
		if (!entryfile || !entryfile.get())
		{
			vfs->Log(1, "Entry file container can NOT allocated");
			return false;
		}
		if (entryfile->Open(wstrEntryFile) == false)
		{
			vfs->Log(1, "Entry file can NOT opened");
			return false;
		}
		auto entryfilesize = entryfile->GetSize();
		if (entryfilesize == 0)
		{
			vfs->Log(1, "Entry file is null");
			continue;
		}
//		vfs->Log(0, "entry size: %u", entryfilesize);

		auto vEntryData = std::vector<uint8_t>(static_cast<uint32_t>(entryfilesize));
		if (entryfile->Read(&vEntryData[0], static_cast<uint32_t>(entryfilesize)) != entryfilesize)
		{
			vfs->Log(1, "Entry file can NOT readed");
			return false;
		}

		if (visualdirectory.empty() == false)
			namewithoutpath = visualdirectory + namewithoutpath;

		if (archive->Write(namewithoutpath, &vEntryData[0], vEntryData.size(), uFlag, uVersion) == false)
		{
			vfs->Log(1, "Entry file can NOT writed");
			return false;
		}
		counter++;
	}
	
	vfs->Log(0, "Real size: %u", counter);
	vfs->Log(0, "Archive size: %u", archive->EnumerateFiles().size());
	return true;
}

int main(int argc, char* argv[])
{
	DeleteFileA("VFSLog.log");

	auto vfs = new CVFSPack();
	if (!vfs || vfs->InitializeVFSPack() == false)
	{
		printf("VFS Initilization fail!\n");
		return EXIT_FAILURE;
	}

	auto targetpack = L"test.vpf";
	auto targetdir = L"test";
	auto targetunpackdir = L"test_unpacked";
	auto key = vfs->ConvertKeyFromAscii("0000000000000000000000000000000000000000000000000000000000000001");

	if (CreateTestArchive(vfs, targetpack, key.data(), targetdir, L"", 1, 3) == false)
	{
		vfs->Log(1, "Target archive can NOT created!");
		return EXIT_FAILURE;
	}

    vfs->SetArchiveKey(targetpack, key.data());
    auto root = vfs->LoadArchive(targetpack);
    if (!root || !root.get())
    {
        vfs->Log(1, "Target archive can not load!");
		return EXIT_FAILURE;
    }
	CheckArchivedFilesIntegrity(vfs, root, targetunpackdir);

	vfs->Log(0, "VFS completed!");

	vfs->FinalizeVFSPack();
    std::system("PAUSE");
	return EXIT_SUCCESS;
}
