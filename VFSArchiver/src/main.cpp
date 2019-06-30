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

#include "../../VFSLib/include/config.h"
#include "../../VFSLib/include/VFSArchive.h"
#include "../../VFSLib/include/VFSFile.h"
#include "../../VFSLib/include/VFSPack.h"
using namespace VFS;

typedef struct _PATCH_CONTEXT
{
	std::wstring from;
	std::wstring to;
} SPatchContext;

typedef struct _ARCHIVER_CONTEXT
{
	std::wstring				strArchiveName;
	std::array <uint8_t, 32>	arArchiveKey;
	std::wstring				stArchiveDirectory;
	std::wstring				strVisualDirectory;
	int32_t						iType;
	int32_t						iVersion;
	std::vector <std::wstring>	vIgnores;
	std::vector <SPatchContext>	vPatches;
} SArchiveContext;

static inline bool FindAndReplaceString(std::wstring& str, const std::wstring& from, const std::wstring& to)
{
	size_t start_pos = str.find(from);
	if (start_pos == std::string::npos)
		return false;

	str.replace(start_pos, from.length(), to);
	return true;
}

bool InitializeConfigFile(CVFSPack * vfs, const std::wstring & strConfigFile, std::vector <std::shared_ptr <SArchiveContext>> & packs)
{
	if (std::filesystem::exists(strConfigFile) == false)
	{
		vfs->Log(1, "Config file: %ls is not exist!", strConfigFile.c_str());
		return false;
	}
	vfs->Log(0, "Config file: %ls", strConfigFile.c_str());

	try
	{
		std::ifstream is(strConfigFile);

		json root;
		is >> root;

		if (root.size() <= 0)
		{
			vfs->Log(1, "Json root is empty!");
			return false;
		}

		for (const auto& group : root)
		{
			if (group.type() != json::value_t::object)
			{
				vfs->Log(1, "Unknown config context(element)");
				return false;
			}
			if (group["dir"].type() != json::value_t::string)
			{
				vfs->Log(1, "Unknown config context(element['dir'])");
				return false;
			}
			if (group["visualdir"].type() != json::value_t::string)
			{
				vfs->Log(1, "Unknown config context(element['visualdir'])");
				return false;
			}
			if (group["file"].type() != json::value_t::string)
			{
				vfs->Log(1, "Unknown config context(element['file'])");
				return false;
			}
#ifndef VFS_PRECOMPILED_LITE
			if (group["key"].type() != json::value_t::array)
			{
				vfs->Log(1, "Unknown config context(element['key'])");
				return false;
			}
#endif
			if (group["type"].type() != json::value_t::number_integer && group["type"].type() != json::value_t::number_unsigned)
			{
				vfs->Log(1, "Unknown config context(element['type'])");
				return false;
			}
			if (group["version"].type() != json::value_t::number_integer && group["version"].type() != json::value_t::number_unsigned)
			{
				vfs->Log(1, "Unknown config context(element['version'])");
				return false;
			}
			if (group.count("ignores") != 0 && group["ignores"].type() != json::value_t::array)
			{
				vfs->Log(1, "Unknown config context(element['ignores'])");
				return false;
			}
			if (group.count("patches") != 0 && group["patches"].type() != json::value_t::object)
			{
				vfs->Log(1, "Unknown config context(element['patches'])");
				return false;
			}

			auto ctx = std::make_shared<SArchiveContext>();
			if (!ctx || !ctx.get())
			{
				vfs->Log(1, "Archiver container can NOT allocated");
				return false;
			}

			auto dir = group["dir"].get<std::string>();
			ctx->stArchiveDirectory = std::wstring(dir.begin(), dir.end());
			if (std::filesystem::exists(ctx->stArchiveDirectory) == false)
			{
				vfs->Log(1, "Working directory: %ls is NOT exist", ctx->stArchiveDirectory.c_str());
				return false;
			}

			auto visdir = group["visualdir"].get<std::string>();
			ctx->strVisualDirectory = std::wstring(visdir.begin(), visdir.end());

			auto file = group["file"].get<std::string>();
			ctx->strArchiveName = std::wstring(file.begin(), file.end());
			if (std::filesystem::exists(ctx->strArchiveName))
			{
				vfs->Log(1, "Target file: %ls is already exist", ctx->strArchiveName.c_str());
				return false;
			}

#ifdef VFS_PRECOMPILED_LITE
			std::copy(std::begin(LITE_CRYPT_KEY), std::end(LITE_CRYPT_KEY), std::begin(ctx->arArchiveKey));
#else
			auto key = group["key"];
			std::copy(std::begin(key), std::end(key), std::begin(ctx->arArchiveKey));
#endif

			ctx->iType = group["type"].get<int32_t>();
			if (ctx->iType >= FLAG_MAX)
			{
				vfs->Log(1, "Unallowerd type: %d", ctx->iType);
				return false;
			}

			ctx->iVersion = group["version"].get<int32_t>();
			if (ctx->iVersion == 0)
			{
				vfs->Log(1, "Version is null");
				return false;
			}

			if (group.count("ignores") != 0)
			{
				auto ignores = std::vector <std::wstring>();

				for (const auto& ignore : group["ignores"])
				{
					auto curr = ignore.get<std::string>();
					auto wcurr = std::wstring(curr.begin(), curr.end());
					ignores.emplace_back(wcurr.c_str());
				}

				ctx->vIgnores = ignores;
			}
			if (group.count("patches") != 0)
			{
				auto patches = std::vector <SPatchContext>();

				for (const auto& [key, val] : group["patches"].items())
				{
					SPatchContext patch{};

					auto wstkey = std::wstring(key.begin(), key.end());
					patch.from = wstkey;

					auto stval = val.get<std::string>();
					patch.to = std::wstring(stval.begin(), stval.end());

					patches.push_back(patch);
				}

				ctx->vPatches = patches;
			}

			vfs->Log(0, "%ls: %ls(%ls)", ctx->strArchiveName.c_str(), ctx->stArchiveDirectory.c_str(), ctx->strVisualDirectory.c_str());
			packs.push_back(ctx);
		}

		is.close();
	}
	catch (std::exception & e)
	{
		vfs->Log(1, "Exception: handled: %s", e.what());
		return false;
	}
	catch (...)
	{
		vfs->Log(1, "Unhandled Exception!");
		return false;
	}

	if (packs.empty())
	{
		vfs->Log(1, "Pack work list is empty!");
		return false;
	}

	vfs->Log(0, "Pack work list: %u", packs.size());
	return true;
}


bool ProcessArchiveFile(CVFSPack * vfs, const std::shared_ptr <SArchiveContext> & pack)
{
	vfs->Log(0, "Curr archive %ls: %ls", pack->strArchiveName.c_str(), pack->stArchiveDirectory.c_str());

	auto file = std::make_shared<CVFSFile>();
	if (!file || !file.get())
	{
		vfs->Log(1, "VFS File container can NOT allocated");
		return false;
	}

	if (file->Create(pack->strArchiveName) == false)
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

	vfs->SetArchiveKey(pack->strArchiveName, pack->arArchiveKey.data());

	if (archive->Create(file, pack->arArchiveKey.data()) == false)
	{
		vfs->Log(1, "Archive can NOT created");
		return false;
	}

	auto workingdirectory = vfs->GetWorkingDirectory();
	for (const auto& entry : std::filesystem::recursive_directory_iterator(pack->stArchiveDirectory))
	{
//		vfs->Log(0, "%ls", entry.path().c_str());

		if (entry.is_directory())
			continue;

		auto namewithoutpath = entry.path().wstring();
		auto wstrdirectory = std::wstring(pack->stArchiveDirectory.begin(), pack->stArchiveDirectory.end());
		std::size_t pos = namewithoutpath.find(wstrdirectory + L"\\");
		if (pos != std::string::npos)
		{
			namewithoutpath.erase(pos, pack->stArchiveDirectory.size() + 1);
		}
		for (const auto& patch : pack->vPatches)
		{
			if (FindAndReplaceString(namewithoutpath, patch.from, patch.to))
            {
                vfs->Log(0, "Patch: %ls->%ls applied to: %ls", patch.from.c_str(), patch.to.c_str(), namewithoutpath.c_str());
            }
		}

		vfs->Log(0, "%ls", namewithoutpath.c_str());

		if (pack->strVisualDirectory.length() && pack->strVisualDirectory[pack->strVisualDirectory.length() - 1] != L'\\' && pack->strVisualDirectory[pack->strVisualDirectory.length() - 1] != L'/')
		{
			for (size_t i = 0; i < pack->strVisualDirectory.length(); ++i)
			{
				if (pack->strVisualDirectory[i] == L'\\')
				{
					pack->strVisualDirectory[i] = L'/';
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

		auto skipfile = false;
		for (const auto & ignore : pack->vIgnores)
		{
			if (vfs->WildcardMatch(namewithoutpath.c_str(), ignore.c_str()))
			{
				skipfile = true;
				break;
			}
		}
		if (skipfile)
		{
			vfs->Log(0, "Content skipped: %ls", entry.path().c_str());
			continue;
		}

		vfs->Log(0, "'%ls'->'%ls'", pack->strVisualDirectory.c_str(), namewithoutpath.c_str());

		auto entryfile = std::make_unique<CVFSFile>();
		if (!entryfile || !entryfile.get())
		{
			vfs->Log(1, "Entry file container can NOT allocated");
			return false;
		}
		if (entryfile->Open(entry.path().wstring()) == false)
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

		auto vEntryData = std::vector<uint8_t>(static_cast<uint32_t>(entryfilesize));
		if (entryfile->Read(&vEntryData[0], static_cast<uint32_t>(entryfilesize)) != entryfilesize)
		{
			vfs->Log(1, "Entry file can NOT readed");
			return false;
		}

		if (pack->strVisualDirectory.empty() == false)
			namewithoutpath = pack->strVisualDirectory + namewithoutpath;

		if (archive->Write(namewithoutpath, &vEntryData[0], vEntryData.size(), pack->iType, pack->iVersion) == false)
		{
			vfs->Log(1, "Entry file can NOT writed");
			return false;
		}
	}

	return true;
}

struct SPackProcessor
{
	std::vector <std::shared_ptr <SArchiveContext>>& _archives;
	CVFSPack* _vfs;
	SPackProcessor(CVFSPack *& vfs, std::vector <std::shared_ptr <SArchiveContext>>& archives) :
		_vfs(vfs), _archives(archives)
	{
	}

	void operator()(const tbb::blocked_range <uint32_t>& range) const
	{
		for (size_t i = range.begin(); i != range.end(); ++i)
		{
			if (ProcessArchiveFile(_vfs, _archives.at(i)) == false)
			{
				_vfs->Log(1, "ProcessArchiveFile fail! Index: %u", i);
				abort();
			}
		}
	}
};

int main(int argc, wchar_t* argv[])
{
	auto vfs = new CVFSPack();
	if (!vfs || vfs->InitializeVFSPack() == false)
	{
		printf("VFS Initilization fail!\n");
		return EXIT_FAILURE;
	}
	vfs->Log(0, "VFS Archiver started! Koray/(c)2019");
	Sleep(2000);

	auto configfile = L"config.json";
	if (argc == 2)
		configfile = argv[1];

	auto packs = std::vector<std::shared_ptr<SArchiveContext>>();

	if (InitializeConfigFile(vfs, configfile, packs) == false)
	{
		return EXIT_FAILURE;
	}

	// ------------------------------------------------------

#ifdef VFS_PRECOMPILED_LITE
	auto litekeyhash = XXH32(LITE_CRYPT_KEY.data(), LITE_CRYPT_KEY.size(), 0);
	if (litekeyhash != 0xD4E79439)
		return EXIT_SUCCESS;

	auto magichash = XXH32((void*)&ARCHIVE_MAGIC, sizeof(ARCHIVE_MAGIC), 0);
	if (magichash != 0x35579559)
		return EXIT_SUCCESS;

	auto ivhash = XXH32(ARCHIVE_IV, strlen(ARCHIVE_IV), 0);
	if (ivhash != 0x672376E9)
		return EXIT_SUCCESS;
#endif

	try
	{
		tbb::task_scheduler_init init(tbb::task_scheduler_init::default_num_threads());

		tbb::parallel_for(tbb::blocked_range<uint32_t>(0, packs.size()), SPackProcessor(vfs, packs));
	}
	catch (std::exception & e)
	{
		vfs->Log(1, "Exception: handled: %s", e.what());
		return false;
	}
	catch (...)
	{
		vfs->Log(1, "Unhandled Exception!");
		return false;
	}

	// ------------------------------------------------------

	for (const auto& pack : packs)
	{
		auto archive = vfs->LoadArchive(pack->strArchiveName);
		vfs->Log(0, "archive: %ls %p-%p", pack->strArchiveName.c_str(), archive, archive.get());

		if (archive && archive.get())
		{
			auto files = archive->EnumerateFiles();
			vfs->Log(0, "files: %u", files.size());

			std::wofstream f(pack->strArchiveName + L".log", std::ofstream::out | std::ofstream::app);
			for (const auto& file : files)
			{
                char fileinfo[512];
                sprintf_s(fileinfo, "%ls: %p", file.filename, (void*)file.hash);
                
				f << fileinfo << std::endl;
//				vfs->Log(0, "File: %s", fileinfo);
			}
			f.close();
		}
	}

	vfs->Log(0, "VFS completed!");

	vfs->FinalizeVFSPack();
	std::system("PAUSE");
	return EXIT_SUCCESS;
}
