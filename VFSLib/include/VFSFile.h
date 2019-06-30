#pragma once
#include <Windows.h>
#include <memory>
#include <algorithm>
#include <string>
#include <mutex>

namespace VFS
{
	enum EFileType : int32_t
	{
		FILE_TYPE_NONE,
		FILE_TYPE_OUTPUT, // READ | WRITE (disk)
		FILE_TYPE_INPUT,  // WRITE ONLY (disk)
		FILE_TYPE_MAPPED, // READ ONLY (mapped)
		FILE_TYPE_MEMORY, // READ ONLY (memory)
		FILE_TYPE_MAX
	};

	class CVFSFile : public std::enable_shared_from_this <CVFSFile>
	{
		public:	
			virtual ~CVFSFile() noexcept;
			CVFSFile(const CVFSFile&) = delete;
			CVFSFile(CVFSFile&&) noexcept = delete;
			CVFSFile& operator=(const CVFSFile&) = delete;
			CVFSFile& operator=(CVFSFile&&) noexcept = delete;

		public:
			CVFSFile();
			
			bool Create(const std::wstring& filename, bool append = false);
			bool Open(const std::wstring& filename);
			void Close();
			bool Map(const std::wstring& filename, uint64_t offset = 0, uint32_t size = 0);
			bool Assign(const std::wstring& filename, const void* memory, uint32_t length, bool copy = true);

			uint32_t Read(void* buffer, uint32_t size);
			uint32_t Write(const void* buffer, uint32_t size);
			
			void SetPosition(int64_t offet, bool relative = false);

			bool IsValid() const;
			bool IsReadable() const;
			bool IsWriteable() const;

			int32_t GetFileType() const;
			const uint8_t* GetData() const;
			const std::wstring& GetFileName() const;
			uint64_t GetSize() const;
			uint64_t GetPosition() const;		
		
		private:
			mutable std::recursive_mutex m_fileMutex;
			
			std::wstring m_fileName;

			HANDLE m_fileHandle;
			HANDLE m_mapHandle;

			uint8_t* m_mappedData;
			uint64_t m_mappedSize;

			uint8_t* m_rawData;
			uint64_t m_rawSize;

			uint64_t m_currPos;
			bool m_memOwner;
			int32_t m_fileType;
	};
}
