cmake_minimum_required(VERSION 3.12...3.13)


set(
	CMAKE_TOOLCHAIN_FILE
	"${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake"
	CACHE
	FILEPATH
	"Default toolchain"
	)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard to be supported")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "compile as PIC by default")

option(HUNTER_ENABLED "Enable Hunter package manager" OFF)
include("cmake/HunterGate.cmake")
HunterGate(
	URL "https://github.com/cpp-pm/hunter/archive/v0.24.8.tar.gz"
	SHA1 "ca7838dded9a1811b04ffd56175f629e0af82d3d"
)

option(USE_BUNDLED_SPDLOG "Use the bundled version of spdlog." ${HUNTER_ENABLED})
option(USE_BUNDLED_LIBEVENT "Use the bundled version of spdlog." ${HUNTER_ENABLED})
option(USE_BUNDLED_LIBCURL "Use the bundled version of spdlog." ${HUNTER_ENABLED})
option(USE_BUNDLED_SPDLOG "Use the bundled version of spdlog." ${HUNTER_ENABLED})

project(coeurl
	VERSION 0.3.0
	DESCRIPTION "Simple library to do http requests asynchronously via CURL in C++."
	HOMEPAGE_URL "https://nheko.im/nheko-reborn/cocurl")

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
	message(FATAL_ERROR "The CMake build for coeurl is not intended for installation or usage outside of FetchContent. Use meson instead.")
endif()

add_library(coeurl lib/client.cpp lib/request.cpp lib/errors.cpp)
add_library(coeurl::coeurl ALIAS coeurl)
target_include_directories(coeurl PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)

find_package(Threads REQUIRED)
target_link_libraries(coeurl PUBLIC Threads::Threads)


# libevent
if (USE_BUNDLED_LIBEVENT)
	hunter_add_package(Libevent)
	find_package(Libevent CONFIG REQUIRED)
	target_link_libraries(coeurl PUBLIC Libevent::event_core)
	# Not needed with current hunter version
	#if (WIN32)
	#	target_link_libraries(coeurl PUBLIC Libevent::event_windows)
	#else()
	#	target_link_libraries(coeurl PUBLIC Libevent::event_pthreads)
	#endif()
else()
	find_package(PkgConfig REQUIRED) 
	pkg_check_modules(libevent_core REQUIRED IMPORTED_TARGET libevent_core)
	target_link_libraries(coeurl PUBLIC PkgConfig::libevent_core)
	if (WIN32)
		pkg_check_modules(libevent_windows REQUIRED IMPORTED_TARGET libevent_windows)
		target_link_libraries(coeurl PUBLIC PkgConfig::libevent_windows)
	else()
		pkg_check_modules(libevent_pthreads REQUIRED IMPORTED_TARGET
			libevent_pthreads)
		target_link_libraries(coeurl PUBLIC PkgConfig::libevent_pthreads)
	endif()
endif()

# curl
if (USE_BUNDLED_LIBCURL)
	hunter_add_package(CURL)
	find_package(CURL CONFIG REQUIRED)
	target_link_libraries(coeurl PUBLIC CURL::libcurl)
else()
	find_package(PkgConfig REQUIRED) 
	pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET libcurl)
	target_link_libraries(coeurl PUBLIC PkgConfig::libcurl)
endif()

# spdlog
if(USE_BUNDLED_SPDLOG)
	hunter_add_package(spdlog)
endif()
find_package(spdlog 1.0.0 CONFIG REQUIRED)
target_link_libraries(coeurl PUBLIC spdlog::spdlog)

IF(MSVC)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
	target_link_libraries(coeurl PUBLIC wsock32 ws2_32)
	target_link_libraries(coeurl PUBLIC iphlpapi)
ENDIF()


# INSTALL MAGIC
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/coeurl)

# Include module with fuction 'write_basic_package_version_file'
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion
	)

configure_package_config_file(
	${CMAKE_CURRENT_LIST_DIR}/cmake/coeurl.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/coeurlConfig.cmake
	INSTALL_DESTINATION "${INSTALL_CONFIGDIR}"
	)

install(
	TARGETS coeurl
	EXPORT "${PROJECT_NAME}Targets"
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
	RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
	INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
	)

export(
	EXPORT "${PROJECT_NAME}Targets"
	FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake
	NAMESPACE coeurl::)
export(PACKAGE coeurl)

install(
	DIRECTORY "include/"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
	FILES_MATCHING PATTERN "*.hpp"
	)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/coeurlConfig.cmake
	DESTINATION "${INSTALL_CONFIGDIR}"
	)

install(
	EXPORT "${PROJECT_NAME}Targets"
	NAMESPACE "${PROJECT_NAME}"
	DESTINATION "${INSTALL_CONFIGDIR}"
	)

