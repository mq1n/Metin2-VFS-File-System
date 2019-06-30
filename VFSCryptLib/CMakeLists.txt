cmake_minimum_required(VERSION 3.2)
project(VFSCryptLib CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ./lib)

set(CRYPTLIB_HEADERS
	${PROJECT_SOURCE_DIR}/include/aes.h
	${PROJECT_SOURCE_DIR}/include/aes256_decrypt.h
	${PROJECT_SOURCE_DIR}/include/aes256_encrypt.h
	${PROJECT_SOURCE_DIR}/include/DataBuffer.h
	${PROJECT_SOURCE_DIR}/include/Exception.h
)
set(CRYPTLIB_SOURCES
	${PROJECT_SOURCE_DIR}/src/aes.cpp
	${PROJECT_SOURCE_DIR}/src/aes256_decrypt.cpp
	${PROJECT_SOURCE_DIR}/src/aes256_encrypt.cpp
	${PROJECT_SOURCE_DIR}/src/DataBuffer.cpp
	${PROJECT_SOURCE_DIR}/src/Exception.cpp
)

add_definitions(-DNOMINMAX)

add_library(${PROJECT_NAME} STATIC ${CRYPTLIB_HEADERS} ${CRYPTLIB_SOURCES})
