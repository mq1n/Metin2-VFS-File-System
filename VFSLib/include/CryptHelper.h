#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "../../VFSCryptLib/include/DataBuffer.h"

namespace VFS
{
	void convert_ascii(const char* src, std::vector<unsigned char>& dest);

    class CAes256
    {
        public:
            CAes256() = default;
            ~CAes256() = default;

            DataBuffer Encrypt(const uint8_t * data, uint32_t size, const std::string & iv, const uint8_t * key);
            DataBuffer Decrypt(const uint8_t * data, uint32_t size, const std::string & iv, const uint8_t * key);
    };
}
