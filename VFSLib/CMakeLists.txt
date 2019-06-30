cmake_minimum_required(VERSION 3.8)
project(VFSLibrary)
set(EXE_NAME VFSLib)

# Output dir
set(OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})

# Preprocessor flags
if (WIN32)
	add_definitions(-DWIN32)
	add_definitions(-D_WIN32)
endif()

add_definitions(-DNOMINMAX)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	add_definitions(-D_DEBUG)
    add_definitions(-DDEBUG)
    set(BUILD_MODE "debug")
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	add_definitions(-D_NDEBUG)
	add_definitions(-DNDEBUG)
    set(BUILD_MODE "normal")
elseif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
	add_definitions(-D_NDEBUG)
	add_definitions(-DNDEBUG)
    set(BUILD_MODE "normal")
elseif(${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
	add_definitions(-D_NDEBUG)
	add_definitions(-DNDEBUG)
    set(BUILD_MODE "normal")
endif()

# Project files
set(LIB_HEADERS
	${PROJECT_SOURCE_DIR}/../3rd/lz4/lib/lz4.h
	${PROJECT_SOURCE_DIR}/../3rd/lz4/lib/lz4hc.h
	${PROJECT_SOURCE_DIR}/../3rd/xxHash/xxhash.h
	${PROJECT_SOURCE_DIR}/include/json.hpp
	${PROJECT_SOURCE_DIR}/include/BasicLog.h
	${PROJECT_SOURCE_DIR}/include/config.h
	${PROJECT_SOURCE_DIR}/include/CryptHelper.h
	${PROJECT_SOURCE_DIR}/include/LogHelper.h
	${PROJECT_SOURCE_DIR}/include/VFSPropertyManager.h
	${PROJECT_SOURCE_DIR}/include/VFSArchive.h
	${PROJECT_SOURCE_DIR}/include/VFSFile.h
	${PROJECT_SOURCE_DIR}/include/VFSPack.h
)
set(LIB_SOURCES
    ${PROJECT_SOURCE_DIR}/../3rd/lz4/lib/lz4.c
    ${PROJECT_SOURCE_DIR}/../3rd/lz4/lib/lz4hc.c
    ${PROJECT_SOURCE_DIR}/../3rd/xxHash/xxhash.c
	${PROJECT_SOURCE_DIR}/src/CryptHelper.cpp
	${PROJECT_SOURCE_DIR}/src/LogHelper.cpp
	${PROJECT_SOURCE_DIR}/src/VFSPropertyManager.cpp
	${PROJECT_SOURCE_DIR}/src/VFSArchive.cpp
	${PROJECT_SOURCE_DIR}/src/VFSFile.cpp
	${PROJECT_SOURCE_DIR}/src/VFSPack.cpp
)

add_library(${EXE_NAME}
	STATIC
	${LIB_HEADERS}
	${LIB_SOURCES}
)
