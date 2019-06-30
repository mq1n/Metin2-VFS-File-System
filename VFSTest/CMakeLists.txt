cmake_minimum_required(VERSION 3.8)
project(VFSTest)
set(EXE_NAME VFSTest)

# Output dir
set(OUTPUT_DIR ${PROJECT_SOURCE_DIR}/../Bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${OUTPUT_DIR})

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
set(ARCHIVER_HEADERS
    ${PROJECT_SOURCE_DIR}/../3rd/lz4/lib/lz4hc.h
    ${PROJECT_SOURCE_DIR}/../3rd/xxHash/xxhash.h
)
set(ARCHIVER_SOURCES
	${PROJECT_SOURCE_DIR}/src/main.cpp
)

set(EXTRA_LIBS dbghelp VFSCryptLib tbb tbb_static VFSLib)
add_executable(${EXE_NAME}
    ${ARCHIVER_HEADERS}
    ${ARCHIVER_SOURCES}
)

target_link_libraries(${EXE_NAME} ${EXTRA_LIBS})

