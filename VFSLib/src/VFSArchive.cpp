#include "../include/VFSArchive.h"
#include "../include/VFSFile.h"
#include "../include/VFSPack.h"
#include "../include/LogHelper.h"
#include "../include/CryptHelper.h"
#include "../include/config.h"

#include <lz4.h>
#include <lz4hc.h>
#include <xxhash.h>

#ifndef ALIGNTO
	#define ALIGNTO(x, a) ((x) + ((a) - ((x) % (a))))
#endif

namespace VFS
{
	extern CVFSLog* gs_pVFSLogInstance;

#pragma pack(push, 1)
	typedef struct _ARCHIVE_HEADER
	{
		uint32_t magic;
		uint32_t bytesPerBlock;
		uint32_t firstEntry;
	} SArchiveHeader;

	typedef struct _m_vfsFileENTRY
	{
		SFileInformation	info;
		uint32_t			finalSize;
		uint32_t			numBlocks;
		uint64_t			offset;
	} SFileEntry;
#pragma pack(pop)

	typedef struct _ARCHIVE_DATA
	{
		std::unordered_map <uint32_t, SFileEntry>	files;
		std::list <SFileEntry>	entries;
		SArchiveHeader			header;
	} SArchiveData;


	CVFSArchive::CVFSArchive()
	{
//		assert(!m_archiveData);
		m_archiveData = new SArchiveData();
	}
	CVFSArchive::~CVFSArchive()
	{
//		assert(m_archiveData == this);

		if (m_archiveData)
		{
			Unload();

			delete static_cast<SArchiveData*>(m_archiveData);
			m_archiveData = nullptr;
		}
	}

	bool CVFSArchive::Load(std::shared_ptr <CVFSFile> file, const uint8_t* key)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		Unload();

		if (!file || !file.get())
		{
	//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Null VFS file ptr");
			return false;
		}

		if (!file->IsReadable())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "VFS archive not readable");
			file.reset();
			return false;
		}

		if (!key)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Null key data");
			file.reset();
			return false;
		}

		m_vfsFile = file;
		memcpy(m_archiveKey, key, VFS::KEY_LENGTH);

		m_vfsFile->SetPosition(0, false);
		if (m_vfsFile->Read(&static_cast<SArchiveData*>(m_archiveData)->header, sizeof(SArchiveHeader)) != sizeof(SArchiveHeader))
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "VFS archive: %ls header can not read", file->GetFileNameA().c_str());
			file.reset();
			memset(m_archiveKey, 0, VFS::KEY_LENGTH);
			return false;
		}

		if (static_cast<SArchiveData*>(m_archiveData)->header.magic != ARCHIVE_MAGIC)
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "VFS archive: %ls Wrong magic", file->GetFileNameA().c_str());
			m_vfsFile.reset();
			memset(m_archiveKey, 0, VFS::KEY_LENGTH);
			return false;
		}

		m_vfsFile->SetPosition(static_cast<SArchiveData*>(m_archiveData)->header.firstEntry, false);
		while (m_vfsFile->GetPosition() < m_vfsFile->GetSize())
		{
			SFileEntry entry;
			m_vfsFile->Read(&entry, sizeof(SFileEntry));

			if (entry.info.index == 0)
				static_cast<SArchiveData*>(m_archiveData)->entries.emplace_back(entry);
			else
				static_cast<SArchiveData*>(m_archiveData)->files.insert({ entry.info.index, entry });

			m_vfsFile->SetPosition(entry.offset - sizeof(SFileEntry) + (entry.numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock), false);
		}

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "VFS archive: %ls loaded", file->GetFileNameA().c_str());
		return true;
	}

	bool CVFSArchive::Create(std::shared_ptr <CVFSFile> file, const uint8_t* keydata)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		if (!file || !file.get())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Null VFS file ptr");
			return false;
		}

		if (!file->IsWriteable())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "VFS archive not writeable");
			return false;
		}

		if (!Load(file, keydata))
		{
			m_vfsFile = file;
			memcpy(m_archiveKey, keydata, VFS::KEY_LENGTH);

			auto header = &static_cast<SArchiveData*>(m_archiveData)->header;
			header->magic = ARCHIVE_MAGIC;

			SYSTEM_INFO sysInfo{};
			GetSystemInfo(&sysInfo);

			if (!sysInfo.dwPageSize)
				header->bytesPerBlock = 4096;
			else
				header->bytesPerBlock = sysInfo.dwPageSize;

			header->firstEntry = ALIGNTO(sizeof(SArchiveHeader), header->bytesPerBlock);

			m_vfsFile->SetPosition(0, false);
			m_vfsFile->Write(header, sizeof(SArchiveHeader));

			if (header->firstEntry - sizeof(SArchiveHeader) > 0)
			{
				auto diff = static_cast<uint8_t*>(malloc(header->firstEntry - sizeof(SArchiveHeader)));
				m_vfsFile->Write(diff, header->firstEntry - sizeof(SArchiveHeader));
				free(diff);
			}
		}
		return true;
	}

	void CVFSArchive::Unload()
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		memset(m_archiveKey, 0, VFS::KEY_LENGTH);

		static_cast<SArchiveData*>(m_archiveData)->entries.clear();
		static_cast<SArchiveData*>(m_archiveData)->files.clear();

		memset(&static_cast<SArchiveData*>(m_archiveData)->header, 0, sizeof(SArchiveHeader));
		m_vfsFile.reset();
	}

	std::shared_ptr <CVFSFile> CVFSArchive::GetFileStream() const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);
		
		return m_vfsFile;
	}

	uint32_t CVFSArchive::GenerateNameIndex(std::wstring filename) const
	{
		for (size_t i = 0; i < filename.size(); ++i)
		{
			if (filename[i] == L'\\')
			{
				filename[i] = L'/';
			}
		}

		std::transform(filename.begin(), filename.end(), filename.begin(), towlower);

		auto hash = XXH32(filename.c_str(), filename.size() * sizeof(wchar_t), 0);
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "%ls(%u) : %p", filename.c_str(), filename.size() * sizeof(wchar_t), hash);

		return hash;
	}

	bool CVFSArchive::Exists(uint32_t index) const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

	//	gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "coming idx: %u", index);			

		auto archive = static_cast<SArchiveData*>(m_archiveData);
		if (archive)
		{
			/*
			for (const auto& file : archive->files)
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "filename %ls, index %u",
					file.second.info.filename, file.second.info.index);			
			}
			*/

			return archive->files.find(index) != archive->files.end();
		}

		return false;
	}

	bool CVFSArchive::Exists(const std::wstring& filename) const
	{
		return Exists(GenerateNameIndex(filename));
	}
	bool CVFSArchive::Exists(const std::string& filename) const
	{
		auto wstfilename = std::wstring(filename.begin(), filename.end());
		return Exists(GenerateNameIndex(wstfilename));
	}

	std::shared_ptr<CVFSFile> CVFSArchive::Open(uint32_t index, const std::wstring& filename) const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		std::shared_ptr <CVFSFile> output;

		auto iter = static_cast<SArchiveData*>(m_archiveData)->files.find(index);
		if (iter == static_cast<SArchiveData*>(m_archiveData)->files.end())
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "File not found for: %p(%ls)", index, filename.c_str());
			return output;
		}
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "%u %ls %u", index, iter->second.info.filename, iter->second.finalSize);

		output = std::make_shared<CVFSFile>();
		if (!output || !output.get() || !output->Open(m_vfsFile->GetFileName()))
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Output file can NOT created!");
			output.reset();
			return output;
		}
		output->SetPosition(iter->second.offset);

		std::vector <uint8_t> rawdata(iter->second.finalSize);
		auto readsize = output->Read(&rawdata[0], iter->second.finalSize);
		if (readsize != iter->second.finalSize)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Read size mismatch: %u-%u", readsize, iter->second.finalSize);
			output.reset();
			return output;
		}

		auto decrypted = DataBuffer(iter->second.finalSize);
		if (iter->second.info.flags & FLAG_CRYPTED_AES256)
		{
			auto aeshelper = std::make_shared<CAes256>();
			decrypted = aeshelper->Decrypt(reinterpret_cast<const uint8_t*>(rawdata.data()), rawdata.size(), ARCHIVE_IV, &m_archiveKey[0]);
		}
		else
		{
			decrypted = DataBuffer(rawdata.data(), rawdata.size());
		}

		auto decompressed = DataBuffer(iter->second.info.rawsize);
		if (iter->second.info.flags & FLAG_COMPRESSED_LZ4)
		{
			std::vector <uint8_t> decompresseddata(iter->second.info.rawsize);
			auto decompressedsize = LZ4_decompress_fast(decrypted.get_data(), reinterpret_cast<char*>(&decompresseddata[0]), iter->second.info.rawsize);
			if (decompressedsize != iter->second.info.compressedsize)
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Decomperssed size mismatch: %d-%u", decompressedsize, iter->second.info.compressedsize);
				output.reset();
				return output;
			}
			decompressed = DataBuffer(&decompresseddata[0], iter->second.info.rawsize);
		}
		else
		{
			decompressed = decrypted;
		}

		auto currenthash = XXH32(decompressed.get_data(), decompressed.get_size(), 0);
		if (currenthash != iter->second.info.hash)
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Hash mismatch: %p-%p", currenthash, iter->second.info.hash);
			output.reset();
			return output;
		}

		output->Assign(iter->second.info.filename, decompressed.get_data(), decompressed.get_size(), true);

		return output;
	}

	std::shared_ptr <CVFSFile> CVFSArchive::Open(const std::wstring & filename) const
	{
		return Open(GenerateNameIndex(filename), filename);
	}


	bool CVFSArchive::Write(const std::wstring& filename, const void* data, uint32_t length, uint8_t flags, uint32_t version)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		if (!m_vfsFile || !m_vfsFile.get() || !m_vfsFile->IsWriteable())
		{
			return false;
		}

		std::uint32_t index = GenerateNameIndex(filename);
		std::uint32_t hash = XXH32(reinterpret_cast<const char*>(data), length, 0);

		auto iter = static_cast<SArchiveData*>(m_archiveData)->files.find(index);
		if (iter != static_cast<SArchiveData*>(m_archiveData)->files.end())
		{
			if (iter->second.info.hash == hash)
			{
				return true;
			}
		}
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, 
//			"Target file: %ls(%u) Data: %p(%u) Hash: %p Flags: %u Version: %u",  filename.c_str(), index, data, length, hash, flags, version);

		int32_t compressedsize = 0;
		auto compressedbuffer = DataBuffer(LZ4_compressBound(length));
		if (flags & FLAG_COMPRESSED_LZ4)
		{
			std::vector <uint8_t> compressed(compressedbuffer.get_size());
			compressedsize = LZ4_compress_HC(reinterpret_cast<const char*>(data), reinterpret_cast<char*>(&compressed[0]), length, compressed.size(), LZ4HC_CLEVEL_MAX);
			if (static_cast<uint32_t>(compressedsize) >= compressedbuffer.get_size() || compressedsize == 0)
			{
				gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Compression fail! File: %ls Raw: %u Compressed: %u Cap: %u", filename.c_str(), length, compressedsize, compressedbuffer.get_size());

				compressedbuffer = DataBuffer(data, length);
				flags &= ~FLAG_COMPRESSED_LZ4;
			}
			else
			{
				compressedbuffer = DataBuffer(&compressed[0], compressedsize);
			}
		}
		if (compressedsize == 0)
		{
//			gs_pVFSLogInstance->Log(__FUNCTION__, LL_ERR, "Compression fail! Raw data moved to compressed buffer");

			flags &= ~FLAG_COMPRESSED_LZ4;
			compressedbuffer = DataBuffer(data, length);
		}

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Compression completed! Data: %p Size: %u - %u", compressedbuffer.get_data(), compressedbuffer.get_size(), compressedsize);

		auto crypted = DataBuffer(compressedbuffer.get_size());
		if (flags & FLAG_CRYPTED_AES256)
		{
			auto aeshelper = std::make_shared<CAes256>();
			crypted = aeshelper->Encrypt(reinterpret_cast<const uint8_t*>(compressedbuffer.get_data()), compressedbuffer.get_size(), ARCHIVE_IV, &m_archiveKey[0]);
		}
		else
		{
			crypted = compressedbuffer;
		}		

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Cryption completed! Data: %p Size: %u", crypted.get_data(), crypted.get_size());

		Delete(index);

		SFileEntry entry;
		memset(&entry, 0, sizeof(SFileEntry));
		entry.numBlocks = 0xffffffff;
		auto idx = static_cast<SArchiveData*>(m_archiveData)->entries.end();
		auto it = static_cast<SArchiveData*>(m_archiveData)->entries.begin();
		while (it != static_cast<SArchiveData*>(m_archiveData)->entries.end())
		{
			if ((*it).numBlocks < entry.numBlocks && (*it).numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock >= crypted.get_size() + sizeof(SFileEntry))
			{
				entry = (*it);
				idx = it;
			}
			++it;
		}

		if (entry.numBlocks == 0xffffffff)
		{
			m_vfsFile->SetPosition(m_vfsFile->GetSize(), false);
			entry.numBlocks = ALIGNTO(crypted.get_size() + sizeof(SFileEntry), static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock) / static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock;
			entry.offset = m_vfsFile->GetPosition() + sizeof(SFileEntry);
			std::uint8_t* mem = static_cast<std::uint8_t*>(malloc(entry.numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock));
			m_vfsFile->Write(mem, entry.numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock);
			free(mem);
		}

		entry.info.index = index;
		entry.info.hash = hash;
		entry.info.flags = flags;
		entry.info.version = version;
		entry.info.rawsize = length;
		entry.info.compressedsize = compressedbuffer.get_size();
		entry.info.cryptedsize = crypted.get_size();
#ifdef SHOW_FILE_NAMES
		wcscpy_s(entry.info.filename, filename.c_str());
#endif
		entry.finalSize = crypted.get_size();

		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Write completed %u-%ls-%u-%u-%p-%u-%u", index, filename.c_str(), length, crypted.get_size(), hash, flags, version);

		m_vfsFile->SetPosition(entry.offset - sizeof(SFileEntry), false);
		m_vfsFile->Write(&entry, sizeof(SFileEntry));
		m_vfsFile->Write(crypted.get_data(), crypted.get_size());

		if (idx != static_cast<SArchiveData*>(m_archiveData)->entries.end())
		{
			gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Entry removed");
			static_cast<SArchiveData*>(m_archiveData)->entries.erase(idx);
		}

//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Entry writed to archive new size: %u", static_cast<SArchiveData*>(m_archiveData)->files.size() + 1);
		static_cast<SArchiveData*>(m_archiveData)->files.emplace(index, entry);
		return true;
	}

	bool CVFSArchive::Delete(uint32_t index)
	{
		std::lock_guard<std::recursive_mutex> __lock(m_archiveMutex);

		if (!m_vfsFile || !m_vfsFile.get() || !m_vfsFile->IsWriteable())
		{
			return false;
		}

		auto iter = static_cast<SArchiveData*>(m_archiveData)->files.find(index);
		if (iter == static_cast<SArchiveData*>(m_archiveData)->files.end())
		{
			return false;
		}

		SFileEntry entry = iter->second;
		static_cast<SArchiveData*>(m_archiveData)->files.erase(iter);

		entry.info.index = 0;
		entry.info.hash = 0;
		entry.info.version = 0;
		entry.info.flags = 0;
		entry.info.rawsize = 0;
		entry.info.compressedsize = 0;
		entry.info.cryptedsize = 0;
#ifdef SHOW_FILE_NAMES
		wcscpy_s(entry.info.filename, L"");
#endif
		entry.finalSize = 0;

		m_vfsFile->SetPosition(entry.offset - sizeof(SFileEntry), false);
		m_vfsFile->Write(&entry, sizeof(SFileEntry));

		static_cast<SArchiveData*>(m_archiveData)->entries.push_back(entry);
		return true;
	}

	bool CVFSArchive::Delete(const std::wstring & filename)
	{
		return Delete(GenerateNameIndex(filename));
	}

	std::vector <SFileInformation> CVFSArchive::EnumerateFiles() const
	{
		std::vector <SFileInformation> result(static_cast<SArchiveData*>(m_archiveData)->files.size());
//		gs_pVFSLogInstance->Log(__FUNCTION__, LL_SYS, "Archived file size: %u", static_cast<SArchiveData*>(m_archiveData)->files.size());

		uint32_t index = 0;
		for (const auto & it : static_cast<SArchiveData*>(m_archiveData)->files)
		{
			result[index++] = it.second.info;
		}
		return result;
	}

	bool CVFSArchive::EnumerateFiles(TEnumFiles pfnEnumFiles, LPVOID pvUserContext)
	{
		if (!pfnEnumFiles)
			return false;

		for (const auto& it : static_cast<SArchiveData*>(m_archiveData)->files)
		{
			if (pfnEnumFiles(Open(it.second.info.index), it.second.info, pvUserContext) == false)
				return false;
		}
		
		return true;
	}

	uint32_t CVFSArchive::ReadRawData(uint32_t index, void* buffer, uint32_t maxlength) const
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		if (!m_vfsFile || !m_vfsFile.get() || !m_vfsFile->IsReadable())
		{
			return 0;
		}

		auto iter = static_cast<SArchiveData*>(m_archiveData)->files.find(index);
		if (iter == static_cast<SArchiveData*>(m_archiveData)->files.end())
		{
			return 0;
		}

		uint32_t blocksize = sizeof(SFileEntry) + iter->second.finalSize;
		if (maxlength > sizeof(SFileEntry))
		{
			memcpy(buffer, &iter->second, sizeof(SFileEntry));

			if (maxlength - sizeof(SFileEntry) > 0)
			{
				std::unique_ptr< CVFSFile> stream(new CVFSFile());
				if (!stream || !stream.get() || !stream->Map(m_vfsFile->GetFileName(), iter->second.offset, iter->second.finalSize))
				{
					return 0;
				}

				blocksize = sizeof(SFileEntry) + static_cast<uint32_t>(stream->GetSize());
				memcpy(reinterpret_cast<uint8_t*>(buffer) + sizeof(SFileEntry), stream->GetData(), std::min<uint32_t>(maxlength - sizeof(SFileEntry), static_cast<uint32_t>(stream->GetSize())));
			}
		}

		return blocksize;
	}
	bool CVFSArchive::WriteRawData(const void* buffer, uint32_t length)
	{
		std::lock_guard <std::recursive_mutex> __lock(m_archiveMutex);

		if (!m_vfsFile || !m_vfsFile .get()|| !m_vfsFile->IsWriteable())
		{
			return false;
		}

		const SFileEntry* ent = reinterpret_cast<const SFileEntry*>(buffer);

		auto iter = static_cast<SArchiveData*>(m_archiveData)->files.find(ent->info.index);
		if (iter != static_cast<SArchiveData*>(m_archiveData)->files.end())
		{
			Delete(ent->info.index);
		}

		SFileEntry entry;
		memset(&entry, 0, sizeof(SFileEntry));
		entry.numBlocks = 0xffffffff;
		auto idx = static_cast<SArchiveData*>(m_archiveData)->entries.end();
		auto it = static_cast<SArchiveData*>(m_archiveData)->entries.begin();
		while (it != static_cast<SArchiveData*>(m_archiveData)->entries.end())
		{
			if ((*it).numBlocks < entry.numBlocks && (*it).numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock >= ent->finalSize + sizeof(SFileEntry))
			{
				entry = (*it);
				idx = it;
			}
			++it;
		}

		if (entry.numBlocks == 0xffffffff)
		{
			m_vfsFile->SetPosition(m_vfsFile->GetSize(), false);
			entry.numBlocks = ALIGNTO(ent->finalSize + sizeof(SFileEntry), static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock) / static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock;
			entry.offset = m_vfsFile->GetPosition() + sizeof(SFileEntry);
			uint8_t* mem = static_cast<uint8_t*>(malloc(entry.numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock));
			m_vfsFile->Write(mem, entry.numBlocks * static_cast<SArchiveData*>(m_archiveData)->header.bytesPerBlock);
			free(mem);
		}

		entry.info.index = ent->info.index;
		entry.info.hash = ent->info.hash;
		entry.info.flags = ent->info.flags;
		entry.info.version = ent->info.version;
		entry.info.rawsize = ent->info.rawsize;
		entry.info.compressedsize = ent->info.compressedsize;
		entry.info.cryptedsize = ent->info.cryptedsize;
#ifdef SHOW_FILE_NAMES
		wcscpy_s(entry.info.filename, ent->info.filename);
#endif
		entry.finalSize = ent->finalSize;

		m_vfsFile->SetPosition(entry.offset - sizeof(SFileEntry), false);
		m_vfsFile->Write(&entry, sizeof(SFileEntry));
		m_vfsFile->Write(reinterpret_cast<const uint8_t*>(buffer) + sizeof(SFileEntry), entry.finalSize);

		if (idx != static_cast<SArchiveData*>(m_archiveData)->entries.end())
		{
			static_cast<SArchiveData*>(m_archiveData)->entries.erase(idx);
		}

		static_cast<SArchiveData*>(m_archiveData)->files.insert({ entry.info.index, entry });
		return true;
	}

	bool CVFSArchive::CopyArchive(std::shared_ptr<CVFSArchive> in, std::shared_ptr<CVFSArchive> out)
	{
		if (!in || !in.get() || !out || !out.get())
		{
			return false;
		}

		for (const auto & file : out->EnumerateFiles())
		{
			out->Delete((file).index);
		}

		for (const auto& file : out->EnumerateFiles())
		{
			uint32_t blocksize = in->ReadRawData(file.index, nullptr, 0);
			if (blocksize == 0)
				return false;

			uint8_t* ptr = static_cast<uint8_t*>(malloc(blocksize));
			if ((blocksize = in->ReadRawData(file.index, ptr, blocksize)) == 0)
			{
				free(ptr);
				return false;
			}

			if (!out->WriteRawData(ptr, blocksize))
			{
				free(ptr);
				return false;
			}

			free(ptr);
		}

		return true;
	}
}
