#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##    Author: Andrei Gourianov, gouriano@ncbi
##
##  Source tree description


string(TIMESTAMP NCBI_TIMESTAMP_START "%s")
string(TIMESTAMP _start)
message("Started: ${_start}")

#############################################################################
if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    if(NOT DEFINED XCODE)
        set(XCODE ON)
    endif()
endif()
get_cmake_property(NCBI_GENERATOR_IS_MULTI_CONFIG GENERATOR_IS_MULTI_CONFIG)
if("$ENV{OSTYPE}" STREQUAL "cygwin")
    if(NOT DEFINED CYGWIN)
        set(CYGWIN ON)
    endif()
endif()

if(DEFINED NCBIPTB.env.NCBI)
    set(ENV{NCBI} ${NCBIPTB.env.NCBI})
endif()
if(DEFINED NCBIPTB.env.PATH)
    if(WIN32)
        set(ENV{PATH} "${NCBIPTB.env.PATH};$ENV{PATH}")
    else()
        set(ENV{PATH} "${NCBIPTB.env.PATH}:$ENV{PATH}")
    endif()
endif()

#############################################################################
set(NCBI_DIRNAME_RUNTIME bin)
set(NCBI_DIRNAME_ARCHIVE lib)
if (WIN32)
    set(NCBI_DIRNAME_SHARED ${NCBI_DIRNAME_RUNTIME})
else()
    set(NCBI_DIRNAME_SHARED ${NCBI_DIRNAME_ARCHIVE})
endif()
set(NCBI_DIRNAME_SRC             src)
set(NCBI_DIRNAME_INCLUDE         include)
set(NCBI_DIRNAME_COMMON_INCLUDE  common)
set(NCBI_DIRNAME_CFGINC          inc)
set(NCBI_DIRNAME_INTERNAL        internal)
set(NCBI_PTBCFG_INSTALL_EXPORT ncbi-cpp-toolkit)
if(NCBI_PTBCFG_PACKAGING OR NCBI_PTBCFG_PACKAGED)
    set(NCBI_DIRNAME_EXPORT          res)
else()
    set(NCBI_DIRNAME_EXPORT          cmake)
endif()
set(NCBI_DIRNAME_TESTING         testing)
set(NCBI_DIRNAME_SCRIPTS         scripts)
set(NCBI_DIRNAME_COMMON_SCRIPTS  scripts/common)
set(NCBI_DIRNAME_BUILDCFG ${NCBI_DIRNAME_SRC}/build-system)
set(NCBI_DIRNAME_CMAKECFG ${NCBI_DIRNAME_SRC}/build-system/cmake)
set(NCBI_DIRNAME_CONANGEN        generators)

set(NCBI_TREE_ROOT     ${CMAKE_SOURCE_DIR})
if(EXISTS "${CMAKE_SOURCE_DIR}/corelib/CMakeLists.corelib.lib.txt")
    get_filename_component(NCBI_TREE_ROOT "${NCBI_TREE_ROOT}/.."   ABSOLUTE)
endif()

# After CMake language processing, \\\\\\1 becomes \\\1.
# The first \\ places a literal \ in the output. The second \1 is the placeholder that gets replaced by the match.
string(REGEX REPLACE "([][^$.\\*+?|()])" "\\\\\\1" _tmp ${CMAKE_SOURCE_DIR})
if (NCBI_PTBCFG_PACKAGED OR (NOT DEFINED NCBI_EXTERNAL_TREE_ROOT AND NOT ${CMAKE_CURRENT_LIST_DIR} MATCHES "^${_tmp}/"))
    get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/../../.."   ABSOLUTE)
    if(EXISTS "${_root}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake")
        set(_files "${_root}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake")
    else()
        file(GLOB_RECURSE _files "${_root}/*/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake")
    endif()
    list(LENGTH _files _count)
    if(${_count} GREATER 0)
        set(NCBI_EXTERNAL_TREE_ROOT "${_root}")
        message(STATUS "Found NCBI C++ Toolkit build: ${NCBI_EXTERNAL_TREE_ROOT}")
    endif()
endif()

if(NOT DEFINED NCBITK_TREE_ROOT)
    if(DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(NCBITK_TREE_ROOT  ${NCBI_EXTERNAL_TREE_ROOT})
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/corelib/CMakeLists.corelib.lib.txt" OR
           EXISTS "${CMAKE_SOURCE_DIR}/src/corelib/CMakeLists.corelib.lib.txt")
        set(NCBITK_TREE_ROOT  ${NCBI_TREE_ROOT})
    else()
        file(GLOB_RECURSE _files "${CMAKE_SOURCE_DIR}/*/CMakeLists.corelib.lib.txt")
        list(LENGTH _files _count)
        if(${_count} GREATER 1)
            if(NOT DEFINED NCBITK_TREE_ROOT)
                message(FATAL_ERROR "Found more than one NCBI C++ toolkit source tree. Please define NCBITK_TREE_ROOT")
            endif()
        elseif(${_count} EQUAL 1)
            get_filename_component(_path ${_files} DIRECTORY)
            get_filename_component(_path "${_path}/../.."   ABSOLUTE)
            set(NCBITK_TREE_ROOT  ${_path})
            message(STATUS "Found NCBI C++ Toolkit sources: ${NCBITK_TREE_ROOT}")
        else()
            get_filename_component(_root "${CMAKE_CURRENT_LIST_DIR}/../../.."   ABSOLUTE)
            if(EXISTS "${_root}/src/corelib/CMakeLists.corelib.lib.txt")
                set(NCBITK_TREE_ROOT  ${_root})
                message(STATUS "Found NCBI C++ Toolkit sources: ${NCBITK_TREE_ROOT}")
            else()
                set(NCBITK_TREE_ROOT  ${NCBI_TREE_ROOT})
            endif()
        endif()
    endif()
elseif(NOT IS_ABSOLUTE "${NCBITK_TREE_ROOT}")
    get_filename_component(NCBITK_TREE_ROOT "${NCBITK_TREE_ROOT}"  ABSOLUTE)
    if(NOT EXISTS "${NCBITK_TREE_ROOT}")
        message(FATAL_ERROR "ERROR: Not found NCBITK_TREE_ROOT: ${NCBITK_TREE_ROOT}")
    endif()
endif()

set(NCBI_SRC_ROOT     ${NCBI_TREE_ROOT}/${NCBI_DIRNAME_SRC})
set(NCBI_INC_ROOT     ${NCBI_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE})
set(NCBITK_SRC_ROOT   ${NCBITK_TREE_ROOT}/${NCBI_DIRNAME_SRC})
set(NCBITK_INC_ROOT   ${NCBITK_TREE_ROOT}/${NCBI_DIRNAME_INCLUDE})
if (NOT EXISTS "${NCBI_SRC_ROOT}")
    set(NCBI_SRC_ROOT   ${NCBI_TREE_ROOT})
endif()
if (NOT EXISTS "${NCBI_INC_ROOT}")
    set(NCBI_INC_ROOT   ${NCBI_TREE_ROOT})
endif()

set(build_root      ${CMAKE_BINARY_DIR})
set(builddir        ${CMAKE_BINARY_DIR})
set(includedir0     ${NCBI_INC_ROOT})
set(includedir      ${NCBI_INC_ROOT})
set(incinternal     ${NCBI_INC_ROOT}/${NCBI_DIRNAME_INTERNAL})

set(NCBI_DIRNAME_BUILD  build)
if(NCBI_PTBCFG_PACKAGED OR NCBI_PTBCFG_PACKAGING)
    get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}" ABSOLUTE)
    set(NCBI_DIRNAME_BUILD .)
elseif (DEFINED NCBI_EXTERNAL_TREE_ROOT)
    string(FIND ${CMAKE_BINARY_DIR} ${NCBI_TREE_ROOT} _pos_root)
    string(FIND ${CMAKE_BINARY_DIR} ${NCBI_SRC_ROOT}  _pos_src)
    if(NOT "${_pos_root}" LESS "0" AND "${_pos_src}" LESS "0" AND NOT "${CMAKE_BINARY_DIR}" STREQUAL "${NCBI_TREE_ROOT}")
        get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
	    get_filename_component(NCBI_DIRNAME_BUILD ${CMAKE_BINARY_DIR} NAME)
    else()
        get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}" ABSOLUTE)
    endif()
else()
    get_filename_component(NCBI_BUILD_ROOT "${CMAKE_BINARY_DIR}/.." ABSOLUTE)
    get_filename_component(NCBI_DIRNAME_BUILD ${CMAKE_BINARY_DIR} NAME)
endif()

set(NCBI_CFGINC_ROOT   ${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CFGINC})

get_filename_component(incdir "${NCBI_BUILD_ROOT}/${NCBI_DIRNAME_CFGINC}" ABSOLUTE)
if(NCBI_GENERATOR_IS_MULTI_CONFIG)
    set(incdir          ${incdir}/$<CONFIG>)
endif()

set(NCBI_TREE_CMAKECFG "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(NCBI_TREE_BUILDCFG "${CMAKE_CURRENT_LIST_DIR}/.."   ABSOLUTE)

if(OFF)
message("CMAKE_SOURCE_DIR    = ${CMAKE_SOURCE_DIR}")
message("NCBI_CURRENT_SOURCE_DIR   = ${NCBI_CURRENT_SOURCE_DIR}")
message("NCBI_TREE_ROOT      = ${NCBI_TREE_ROOT}")
message("NCBI_SRC_ROOT       = ${NCBI_SRC_ROOT}")
message("NCBI_INC_ROOT       = ${NCBI_INC_ROOT}")
message("NCBITK_TREE_ROOT    = ${NCBITK_TREE_ROOT}")
message("NCBITK_SRC_ROOT     = ${NCBITK_SRC_ROOT}")
message("NCBITK_INC_ROOT     = ${NCBITK_INC_ROOT}")
message("NCBI_BUILD_ROOT     = ${NCBI_BUILD_ROOT}")
message("NCBI_CFGINC_ROOT    = ${NCBI_CFGINC_ROOT}")
message("NCBI_TREE_BUILDCFG  = ${NCBI_TREE_BUILDCFG}")
message("NCBI_TREE_CMAKECFG  = ${NCBI_TREE_CMAKECFG}")
message("NCBI_EXTERNAL_TREE_ROOT  = ${NCBI_EXTERNAL_TREE_ROOT}")
endif()
