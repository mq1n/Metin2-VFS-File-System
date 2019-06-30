#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <array>

#include "VFSArchive.h"
#include "VFSFile.h"

namespace VFS
{
	static const auto KEY_LENGTH = 32;
	static const std::array <uint8_t, 32> LITE_CRYPT_KEY = {
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1
	};
	static const auto ARCHIVE_IV = "000102030405060708090A0B0C0D0E0F";
	static const auto ARCHIVE_MAGIC = 0x00003169;

	class CVFSPack
	{
		// Lifecycle
		public:	
			virtual ~CVFSPack() noexcept;
			CVFSPack(const CVFSPack&) = delete;
			CVFSPack(CVFSPack&&) noexcept = delete;
			CVFSPack& operator=(const CVFSPack&) = delete;
			CVFSPack& operator=(CVFSPack&&) noexcept = delete;
		
		// Public methods
		public:
			// Constructor
			CVFSPack();

			// Initilization
			bool InitializeVFSPack() const;

			// Finalization
			bool FinalizeVFSPack() const;

			// VFS Pack singletons
			static CVFSPack* InstancePtr();
			static CVFSPack& Instance();

			// Archive methods
            void LoadRegistiredArchives();

			void RegisterArchive(std::wstring name, std::wstring path = L"*");
			void UnregisterArchive(std::wstring name);
			
			std::shared_ptr <CVFSArchive> LoadArchive(std::wstring name);
			std::shared_ptr <CVFSArchive> FindArchive(std::wstring name) const;
			void UnloadArchive(std::shared_ptr <CVFSArchive> ptr);

			// File methods
			std::shared_ptr <CVFSFile> Create(const std::wstring & name, bool append = false);
			std::shared_ptr <CVFSFile> Open(std::wstring name);

			// Utilities
			void SetWorkingDirectory(const std::wstring & dir);
			void SetArchiveKey(const std::wstring & name, const uint8_t * key);

			std::wstring GetWorkingDirectory() const;
			std::wstring GetExecutableDirectory() const;         
			
			static std::wstring GetAbsolutePath(const std::wstring& path);

			const std::unordered_map <std::wstring, std::wstring> & GetRegisteredArchives() const;
			const std::list <std::shared_ptr <CVFSArchive> > & GetArchives() const;

			void Log(int32_t level, const char* c_szFormat, ...);

			bool WildcardMatch(const std::wstring& str, const std::wstring& match);

			std::vector <uint8_t> ConvertKeyFromAscii(const char* src);
			std::string ToString(const std::wstring& wstInput);
			std::wstring ToWstring(const std::string& stInput);

		private:
			mutable std::recursive_mutex m_packMutex;

			std::unordered_map <std::wstring, std::vector <uint8_t> >	m_archiveKeys;
			std::unordered_map <std::wstring, std::wstring>			    m_registiredArchives;
			std::list <std::wstring>									m_archiveNames;
			std::list <std::shared_ptr <CVFSArchive> >					m_archives;
	};
}
