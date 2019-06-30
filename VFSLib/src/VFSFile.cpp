#include "../include/VFSPack.h"
#include "../include/VFSFile.h"
#include "../include/LogHelper.h"

namespace VFS
{
    extern CVFSLog* gs_pVFSLogInstance;
    
	CVFSFile::CVFSFile() :
		m_fileName(L""),
		m_fileHandle(INVALID_HANDLE_VALUE),  m_mapHandle(nullptr),
		m_mappedData(nullptr), m_mappedSize(0),
		m_rawData(nullptr), m_rawSize(0),
		m_currPos(0), m_memOwner(false),
		m_fileType(FILE_TYPE_NONE)
	{
	}
	CVFSFile::~CVFSFile()
	{
		Close();
	}
	

	bool CVFSFile::Create(const std::wstring& filename, bool append)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		Close();

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "File: %ls Type: %ls", filename.c_str(), append ? "append" : "create");

		m_fileHandle = CreateFileW(CVFSPack::GetAbsolutePath(filename).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (m_fileHandle != INVALID_HANDLE_VALUE)
		{
			m_fileType = FILE_TYPE_OUTPUT;
			m_fileName = CVFSPack::GetAbsolutePath(filename);
		}
        else
        {
		    gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "File: %ls can not create! Error: %u",  CVFSPack::GetAbsolutePath(filename).c_str(), GetLastError());
            return false;
        }

		return (m_fileHandle != INVALID_HANDLE_VALUE);
	}

	bool CVFSFile::Open(const std::wstring& filename)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		Close();

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "File: %ls", CVFSPack::GetAbsolutePathW(filename).c_str());

		m_fileHandle = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (m_fileHandle != INVALID_HANDLE_VALUE)
		{
			m_fileType = FILE_TYPE_INPUT;
			m_fileName = filename;
		}
		else
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "File: %ls can not open! Error: %u", filename.c_str(), GetLastError());
			return false;
		}

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Handle: %p", m_fileHandle);
		return (m_fileHandle != INVALID_HANDLE_VALUE);
	}

	void CVFSFile::Close()
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls", m_fileName.c_str());

		if (m_rawData)
		{
			if (m_rawData != m_mappedData && m_memOwner)
			{
				free(m_rawData);
			}
			m_rawData = nullptr;
		}

		if (m_mappedData)
		{
			UnmapViewOfFile(m_mappedData);
			m_mappedData = nullptr;
		}

		if (m_mapHandle)
		{
			CloseHandle(m_mapHandle);
			m_mapHandle = nullptr;
		}

		if (m_fileHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_fileHandle);
			m_fileHandle = INVALID_HANDLE_VALUE;
		}

		m_currPos = 0;
		m_rawSize = 0;
		m_mappedSize = 0;
		m_fileName.clear();
		m_fileType = FILE_TYPE_NONE;
		m_memOwner = false;
	}

	bool CVFSFile::Map(const std::wstring& filename, uint64_t offset, uint32_t size)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		Close();

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, " %ls\n, %lu, %u", filename.c_str(), offset, size);

		m_fileHandle = CreateFileW(CVFSPack::GetAbsolutePath(filename).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (m_fileHandle == INVALID_HANDLE_VALUE)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "CreateFile fail! Error: %u", GetLastError());
			return false;
		}

		m_mapHandle = CreateFileMappingA(m_fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (!m_mapHandle)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "CreateFileMapping fail! Error: %u", GetLastError());

			CloseHandle(m_fileHandle);
			m_fileHandle = INVALID_HANDLE_VALUE;
			return false;
		}

		SYSTEM_INFO sys {};
		GetSystemInfo(&sys);

		offset -= offset % sys.dwAllocationGranularity;

		m_mappedData = static_cast<uint8_t*>(MapViewOfFile(m_mapHandle, FILE_MAP_READ, offset >> 32, offset & 0xffffffff, size));
		
		LARGE_INTEGER s;
		GetFileSizeEx(m_fileHandle, &s);
		if (size == 0)
		{
			size = static_cast<uint32_t>(s.QuadPart);
		}
		m_mappedSize = size;

		m_rawData = m_mappedData + (offset % sys.dwAllocationGranularity);
		m_rawSize = std::min<uint64_t>(size, s.QuadPart);
		m_currPos = 0;
	
		if (m_rawData)
		{
			m_fileType = FILE_TYPE_MAPPED;
			m_fileName = CVFSPack::GetAbsolutePath(filename);
		}

		return (m_rawData != nullptr);
	}

	bool CVFSFile::Assign(const std::wstring& filename, const void* memory, uint32_t length, bool copy)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		Close();

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls, %p, %u, %ls", filename.c_str(), memory, length, copy ? "copy" : "assign");

		if (copy)
		{
			m_rawData = static_cast<uint8_t*>(malloc(length));
			if (m_rawData)
			{
				memcpy(m_rawData, memory, length);
				m_rawSize = length;
			}
			m_memOwner = true;
		}
		else
		{
			m_rawData = static_cast<uint8_t*>(const_cast<void*>(memory));
			m_rawSize = length;
			m_memOwner = false;
		}

		if (m_rawData)
		{
			m_fileType = FILE_TYPE_MEMORY;
			m_fileName = filename;
		}

		return m_rawData;
	}


	uint32_t CVFSFile::Read(void* buffer, uint32_t size)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls, %p, %u", m_fileName.c_str(), buffer, size);
	
		if (!IsReadable())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Is not readable!"); 
			return 0;
		}

		switch (m_fileType)
		{
			case FILE_TYPE_OUTPUT:
			case FILE_TYPE_INPUT:
			{
				DWORD dwRead;
				if (!ReadFile(m_fileHandle, buffer, size, &dwRead, nullptr))
				{
					gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "ReadFile fail! Error: %u", GetLastError());
					return 0;			
				}
				return dwRead;
			} break;

			case FILE_TYPE_MAPPED:
			case FILE_TYPE_MEMORY:
			{
				auto len = std::min<uint32_t>(static_cast<uint32_t>(m_rawSize - m_currPos), size);
				memcpy(buffer, &m_rawData[m_currPos], len);
				m_currPos += len;
				return len;
			} break;
		}

		return 0;
	}

	uint32_t CVFSFile::Write(const void* buffer, uint32_t size)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls, %p, %u", m_fileName.c_str(), buffer, size);

		if (!IsWriteable())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Is not writeable!"); 
			return 0;
		}

		DWORD dwWritten;
		if (!WriteFile(m_fileHandle, buffer, size, &dwWritten, nullptr))
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "WriteFile fail! Error: %u", GetLastError());
			return 0;
		}
		
		return dwWritten;
	}


	void CVFSFile::SetPosition(int64_t offset, bool relative)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%ls, %lld, %ls", m_fileName.c_str(), offset, relative ? "relative" : "absolute");

		if (!IsValid())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Is not valid!"); 
			return;
		}

		switch (m_fileType)
		{
			case FILE_TYPE_OUTPUT:
			case FILE_TYPE_INPUT:
			{
				LARGE_INTEGER m;
				m.QuadPart = offset;

				if (!SetFilePointerEx(m_fileHandle, m, 0, relative ? FILE_CURRENT : FILE_BEGIN))
				{
					gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "SetFilePointerEx fail! Error: %u", GetLastError());
					return;
				}
			} break;

			case FILE_TYPE_MAPPED:
			case FILE_TYPE_MEMORY:
			{
				m_currPos = relative ? m_currPos + offset : offset;
			} break;
		}
	}


	bool CVFSFile::IsValid() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

        auto ret = false;
		switch (m_fileType)
		{
			case FILE_TYPE_OUTPUT:
			case FILE_TYPE_INPUT:
			{
				ret = m_fileHandle && m_fileHandle != INVALID_HANDLE_VALUE;
			} break;

			case FILE_TYPE_MAPPED:
			case FILE_TYPE_MEMORY:
			{
				ret = m_rawData != nullptr;
			} break;
		}

 //		gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "%ls %d %d", m_fileNameA.c_str(), m_fileType, ret);        
		return ret;
	}
	bool CVFSFile::IsReadable() const
	{
		return IsValid();
	}
	bool CVFSFile::IsWriteable() const
	{
		return (m_fileType == FILE_TYPE_OUTPUT && IsValid());
	}


	int32_t CVFSFile::GetFileType() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		return m_fileType;
	}
	
	const uint8_t* CVFSFile::GetData() const
	{
		return m_rawData;
	}

	const std::wstring& CVFSFile::GetFileName() const
	{
		return m_fileName;
	}

	uint64_t CVFSFile::GetSize() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		if (!IsValid())
        {
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "%ls Is not valid!", m_fileNameA.c_str()); 
    		return 0;
        }

        uint64_t size = 0;
		switch (m_fileType)
		{
			case FILE_TYPE_OUTPUT:
			case FILE_TYPE_INPUT:
			{
				LARGE_INTEGER s;
				GetFileSizeEx(m_fileHandle, &s);
				size = s.QuadPart;
			} break;

			case FILE_TYPE_MAPPED:
			case FILE_TYPE_MEMORY:
			{
				size = m_rawSize;
			} break;
		}

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "%ls-%u type %d err %u", m_fileNameA.c_str(), size, m_fileType, GetLastError()); 
		return size;
	}
	uint64_t CVFSFile::GetPosition() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_fileMutex);

		if (!IsValid())
			return 0;

		switch (m_fileType)
		{
			case FILE_TYPE_OUTPUT:
			case FILE_TYPE_INPUT:
			{
				LARGE_INTEGER newptr{};
				LARGE_INTEGER distance{};

				SetFilePointerEx(m_fileHandle, distance, &newptr, FILE_CURRENT);
				return newptr.QuadPart;
			} break;

			case FILE_TYPE_MAPPED:
			case FILE_TYPE_MEMORY:
			{
				return m_currPos;
			} break;
		}
		return 0;
	}
}
