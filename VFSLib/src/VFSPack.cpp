#include "../include/VFSPack.h"
#include "../include/LogHelper.h"
#include "../include/CryptHelper.h"
#include "../include/config.h"

#include <ppl.h>

namespace VFS
{
	CVFSPack* gs_pVFSInstance = nullptr;
	CVFSLog* gs_pVFSLogInstance = nullptr;

	CVFSPack* CVFSPack::InstancePtr()
	{
		return gs_pVFSInstance;
	}
	CVFSPack& CVFSPack::Instance()
	{
		assert(gs_pVFSInstance);
		return *gs_pVFSInstance;
	}

	CVFSPack::CVFSPack()
	{
		assert(!gs_pVFSInstance);
		gs_pVFSInstance = this;

//		if (gs_pVFSLogInstance)
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Singleton: %p declared", this);
//		else
//			printf("Singleton: %p declared\n", this);
	}
	CVFSPack::~CVFSPack()
	{
		assert(gs_pVFSInstance == this);
		gs_pVFSInstance = nullptr;

//		if (gs_pVFSLogInstance)
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Singleton: %p free'd", this);
//		else
//			printf("Singleton: %p free'd\n", this);
	}

	bool CVFSPack::InitializeVFSPack() const
	{
		assert(!gs_pVFSLogInstance);

        DeleteFileA("VFSLog.log");
		gs_pVFSLogInstance = new CVFSLog("VFSLog", "VFSLog.log");

		if (!gs_pVFSLogInstance)
		{
			printf("CVFSPack::InitializeVFSPack: Logger inilization fail!\n");
			return false;
		}
		return true;
	}
	bool CVFSPack::FinalizeVFSPack() const
	{
		assert(gs_pVFSLogInstance);
		
		delete gs_pVFSLogInstance;
		gs_pVFSLogInstance = nullptr;
		return true;
	}

    void CVFSPack::LoadRegistiredArchives()
    {
		concurrency::parallel_for_each(m_archiveNames.rbegin(), m_archiveNames.rend(), [&](const std::wstring& archivename)
		{ 
			auto archive = LoadArchive(archivename);
			if (!archive || !archive.get())
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "%ls can not load", archivename.c_str());
				abort();
			}
           // else
           // {
           //     gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls succesfully loaded", archivename.c_str());
           // }
		});
    }

	void CVFSPack::RegisterArchive(std::wstring name, std::wstring path /* = "*" */)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

		std::transform(path.begin(), path.end(), path.begin(), ::tolower);
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);

		m_registiredArchives.emplace(path, name);
		m_archiveNames.emplace_back(name);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Archive: %ls Path: %ls", name.c_str(), path.c_str());
	}
	void CVFSPack::UnregisterArchive(std::wstring name)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

		transform(name.begin(), name.end(), name.begin(), ::tolower);

		for (auto iter = m_registiredArchives.begin(); iter != m_registiredArchives.end(); ++iter)
		{
			if (iter->second == name)
			{
				m_registiredArchives.erase(iter);
				break;
			}
		}

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", name.c_str());
	}

	std::shared_ptr <CVFSArchive> CVFSPack::FindArchive(std::wstring filename) const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", filename.c_str());

		std::transform(filename.begin(), filename.end(), filename.begin(), tolower);

		std::shared_ptr <CVFSArchive> result;
		for (const auto& iter : m_archives)
		{
			if (iter->GetFileStream()->GetFileName() == filename)
			{
				result = iter;
				break;
			}
		}
		return result;
	}
	std::shared_ptr <CVFSArchive> CVFSPack::LoadArchive(std::wstring filename)
	{
		std::lock_guard<std::recursive_mutex> __lock(m_packMutex);

		std::shared_ptr <CVFSArchive> result;

		std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", filename.c_str());
		
		for (const auto& iter : m_archives)
		{
			if (iter->GetFileStream()->GetFileName() == filename || iter->GetFileStream()->GetFileName() == filename)
			{
				result = iter;
				break;
			}
		}

		if (!result)
		{
			result = std::make_shared<CVFSArchive>();

			auto file = std::make_shared<CVFSFile>();
			file->Open(GetAbsolutePath(filename));

//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "New file: %p for: %ls", file.get(), filename.c_str());

//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Getting key for filename: %ls", filename.c_str());

			auto iter = m_archiveKeys.find(filename);
			const uint8_t * keyArray = iter == m_archiveKeys.end() ? nullptr : iter->second.data();

			if (keyArray == nullptr)
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Key not found for archive %ls", file->GetFileName().c_str());
			}

			if (!result->Load(file, keyArray))
			{
				result.reset();
			}
			else
			{
				m_archives.push_back(result);
			}
		}

		return result;
	}
	void CVFSPack::UnloadArchive(std::shared_ptr<CVFSArchive> archive)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", archive->GetFileStream()->GetFileNameA().c_str());

		for (const auto& iter : m_archives)
		{
			if (iter == archive)
			{
				m_archives.remove(iter);
				break;
			}
		}
	}

	std::shared_ptr <CVFSFile> CVFSPack::Create(const std::wstring& filename, bool append)
	{
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls, %s", filename.c_str(), append ? "append" : "create");

		auto result = std::make_shared<CVFSFile>();
		if (!result->Create(GetAbsolutePath(filename), append))
		{
			result.reset();
		}

		return result;
	}
	std::shared_ptr <CVFSFile> CVFSPack::Open(std::wstring filename)
	{
		std::lock_guard<std::recursive_mutex> __lock(m_packMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", filename.c_str());

		std::transform(filename.begin(), filename.end(), filename.begin(), tolower);

		std::shared_ptr <CVFSFile> result;
		for (const auto & iter : m_archives)
		{
			result = iter->Open(filename);
			if (result)
				break;
		}

		if (!result)
		{
			result = std::make_shared<CVFSFile>();
			if (!result->Open(filename))
			{
				result.reset();
			}
		}

		return result;
	}

	void CVFSPack::SetWorkingDirectory(const std::wstring & dir)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

		SetCurrentDirectoryW(GetAbsolutePath(dir).c_str());
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", dir.c_str());
	}

	void CVFSPack::SetArchiveKey(const std::wstring & name, const uint8_t * key)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

		std::wstring archive = name;

//		std::size_t pos = archive.find_last_of("\\/");
//		if (pos != std::wstring::npos)
//			archive.erase(0, pos + 1);
		
		std::transform(archive.begin(), archive.end(), archive.begin(), ::tolower);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Key ptr %p File: %ls", key, archive.c_str());

		m_archiveKeys[archive] = std::vector<uint8_t>(&key[0], &key[KEY_LENGTH]);
		
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Key:");

//		for (size_t i = 0; i < KEY_LENGTH; ++i)
//		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%d) %02x", i, key[i]);
//		}
	}

	std::wstring CVFSPack::GetWorkingDirectory() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

		wchar_t buffer[MAX_PATH] = { 0 };
		GetCurrentDirectoryW(MAX_PATH, buffer);

		return buffer;
	}
	std::wstring CVFSPack::GetExecutableDirectory() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_packMutex);

		wchar_t buffer[MAX_PATH] = { 0 };
		GetModuleFileNameW(GetModuleHandleA(nullptr), buffer, MAX_PATH);

		for (size_t i = wcslen(buffer); i > 0; --i)
		{
			if (buffer[i] == L'\\' || buffer[i] == L'/')
			{
				buffer[i] = 0;
				break;
			}
		}
		for (size_t i = 0; i < MAX_PATH && buffer[i] != 0; ++i)
		{
			if (buffer[i] == L'\\')
			{
				buffer[i] = L'/';
			}
		}
		return buffer;
	}

	std::wstring CVFSPack::GetAbsolutePath(const std::wstring& path)
	{
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", path.c_str());

		wchar_t pathbuf[MAX_PATH] = { '\0' };
		_wfullpath(pathbuf, path.c_str(), MAX_PATH);

		return pathbuf;
	}

	const std::unordered_map <std::wstring, std::wstring>& CVFSPack::GetRegisteredArchives() const
	{
		return m_registiredArchives;
	}
	const std::list <std::shared_ptr <CVFSArchive> >& CVFSPack::GetArchives() const
	{
		return m_archives;
	}

	void CVFSPack::Log(int32_t level, const char* c_szFormat, ...)
	{
		if (gs_pVFSLogInstance)
		{
			char szLog[8192] = { 0 };

			va_list vaArgList;
			va_start(vaArgList, c_szFormat);
			vsprintf_s(szLog, c_szFormat, vaArgList);
			va_end(vaArgList);

			gs_pVFSLogInstance->Log(__FUNCTION__, level, szLog);
		}
	}

	bool CVFSPack::WildcardMatch(const std::wstring& str, const std::wstring& match)
	{
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls, %ls", str.c_str(), match.c_str());

		const wchar_t* pMatch = match.c_str(), * pString = str.c_str();
		while (*pMatch)
		{
			if (*pMatch == '?')
			{
				if (!*pString)
				{
					return false;
				}
				++pString;
				++pMatch;
			}
			else if (*pMatch == '*')
			{
				if (WildcardMatch(pString, pMatch + 1) || (*pString && WildcardMatch(pString + 1, pMatch)))
				{
					return true;
				}
				return false;
			}
			else
			{
				if (*pString++ != *pMatch++)
				{
					return false;
				}
			}
		}
		return !*pString && !*pMatch;
	}

	std::vector <uint8_t> CVFSPack::ConvertKeyFromAscii(const char* src)
	{
		auto output = std::vector<uint8_t>();
		convert_ascii(src, output);
		return output;
	}

	std::string CVFSPack::ToString(const std::wstring& wstInput)
	{
		auto stOut = std::string(wstInput.begin(), wstInput.end());
		return stOut;
	}

	std::wstring CVFSPack::ToWstring(const std::string& stInput)
	{
		auto wstOut = std::wstring(stInput.begin(), stInput.end());
		return wstOut;
	}
}
