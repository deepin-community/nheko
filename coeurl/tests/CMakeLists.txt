cmake_minimum_required(VERSION 3.12...3.13)


set(
	CMAKE_TOOLCHAIN_FILE
	"${CMAKE_CURRENT_LIST_DIR}/../toolchain.cmake"
	CACHE
	FILEPATH
	"Default toolchain"
	)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard to be supported")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "compile as PIC by default")

option(HUNTER_ENABLED "Enable Hunter package manager" OFF)
include("../cmake/HunterGate.cmake")
HunterGate(
	URL "https://github.com/cpp-pm/hunter/archive/v0.24.8.tar.gz"
	SHA1 "ca7838dded9a1811b04ffd56175f629e0af82d3d"
)

project(coeurl-cmake-buildtest)

include(FetchContent)
FetchContent_Declare(
	coeurl
	GIT_REPOSITORY "${CMAKE_CURRENT_LIST_DIR}/.."
	GIT_TAG        HEAD
	)
FetchContent_MakeAvailable(coeurl)
