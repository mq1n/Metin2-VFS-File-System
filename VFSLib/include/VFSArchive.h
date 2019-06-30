#pragma once
#include "VFSFile.h"
#include <memory>
#include <string>
#include <mutex>
#include <vector>

namespace VFS
{
	enum EArchiveFlags
	{
		FLAG_RAW_DATA = 0, // Stored as raw
		FLAG_COMPRESSED_LZ4 = 1, // Compressed with lz4
		FLAG_CRYPTED_AES256 = 2, // Crypted with AES256
		FLAG_MAX = 4,
	};

	#pragma pack(push, 1)
	typedef struct _FILE_INFORMATIONS
	{
		uint32_t index;
		uint32_t hash;
		uint32_t version;
		uint8_t flags;
		uint32_t rawsize;
		uint32_t compressedsize;
		uint32_t cryptedsize;
		wchar_t filename[255];
	} SFileInformation;
	#pragma pack(pop)

	class CVFSArchive : public std::enable_shared_from_this <CVFSArchive>
	{
		typedef bool (__stdcall* TEnumFiles)(std::shared_ptr <CVFSFile> pcPack, const SFileInformation& pcFileInformations, void* pvUserContext);

		public:	
			virtual ~CVFSArchive() noexcept;
			CVFSArchive(const CVFSArchive&) = delete;
			CVFSArchive(CVFSArchive&&) noexcept = delete;
			CVFSArchive& operator=(const CVFSArchive&) = delete;
			CVFSArchive& operator=(CVFSArchive&&) noexcept = delete;

		public:
			CVFSArchive();
			
			static bool CopyArchive(std::shared_ptr <CVFSArchive> from, std::shared_ptr <CVFSArchive> to);
		 
			bool Create(std::shared_ptr <CVFSFile> file, const uint8_t* key);
			bool Load(std::shared_ptr <CVFSFile> file, const uint8_t* key);
			void Unload();

			std::shared_ptr <CVFSFile> Open(uint32_t index, const std::wstring& filename = L"") const;
			std::shared_ptr <CVFSFile> Open(const std::wstring& filename) const;
			bool Write(const std::wstring& filename, const void* data, uint32_t length, uint8_t flags = FLAG_RAW_DATA, uint32_t version = 0);
			bool Delete(uint32_t index);
			bool Delete(const std::wstring& filename);				

			uint32_t ReadRawData(uint32_t index, void* buffer, uint32_t maxlength) const;
			bool WriteRawData(const void* buffer, uint32_t length);			
			
			uint32_t GenerateNameIndex(std::wstring filename) const;

			bool Exists(uint32_t index) const;
			bool Exists(const std::wstring& filename) const;
			bool Exists(const std::string& filename) const;

			std::vector <SFileInformation> EnumerateFiles() const;
			bool EnumerateFiles(TEnumFiles pfnEnumFiles, LPVOID pvUserContext);
			std::shared_ptr <CVFSFile> GetFileStream() const;
			
		private:
			mutable std::recursive_mutex m_archiveMutex;
			std::shared_ptr <CVFSFile> m_vfsFile;
			uint8_t m_archiveKey[32];
			void* m_archiveData;
	};
}