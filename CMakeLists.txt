cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(VFSPro)

# Rainbooow
SET(CMAKE_COLOR_MAKEFILE ON)

# Output folder
set(OUTPUT_DIR  ${PROJECT_SOURCE_DIR}/Bin)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
	  "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# C++ Standard 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Convert MD to MT
set(LIB_RT_SUFFIX "mt")
set(LIB_RT_OPTION "/MT")

foreach(flag_var  CMAKE_C_FLAGS  CMAKE_CXX_FLAGS)
	 foreach(config_name  ""  DEBUG  RELEASE  MINSIZEREL  RELWITHDEBINFO)
		set(var_name "${flag_var}")

		if(NOT "${config_name}" STREQUAL "")
			set(var_name "${var_name}_${config_name}")
		endif()
			
		string(REPLACE "/MD" "${LIB_RT_OPTION}" ${var_name} "${${var_name}}")
		set(${var_name} "${${var_name}}" CACHE STRING "" FORCE)
	endforeach()
endforeach()

# Multiprocessor
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

# General linker options
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /NODEFAULTLIB:libci.lib")

# Configrations
set(BUILD_STATIC ON)
set(BUILD_SHARED OFF)
set(BUILD_TESTING OFF)

# External include folders
include_directories(${PROJECT_SOURCE_DIR}/3rd)
include_directories(${PROJECT_SOURCE_DIR}/3rd/lz4/lib)
include_directories(${PROJECT_SOURCE_DIR}/3rd/spdlog/include)
include_directories(${PROJECT_SOURCE_DIR}/3rd/tbb/include)
include_directories(${PROJECT_SOURCE_DIR}/3rd/xxHash)

# Precompiled library folder
link_directories(${PROJECT_SOURCE_DIR}/3rd/precompiled)

# Sub projects
add_subdirectory(${PROJECT_SOURCE_DIR}/VFSCryptLib)
add_subdirectory(${PROJECT_SOURCE_DIR}/VFSLib)
add_subdirectory(${PROJECT_SOURCE_DIR}/VFSArchiver)
add_subdirectory(${PROJECT_SOURCE_DIR}/VFSTest)
add_subdirectory(${PROJECT_SOURCE_DIR}/VFSPropertyGenerator)
