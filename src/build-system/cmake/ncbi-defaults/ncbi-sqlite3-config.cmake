# Config file for NCBI-specific libxml layout
#

#############################################################################
##
## Standard boilerplate capturing NCBI library layout issues
##

if (BUILD_SHARED_LIBS)
    set(_NCBI_LIBRARY_SUFFIX .so)
else()
    set(_NCBI_LIBRARY_SUFFIX -static.a)
endif()


#############################################################################
##
## Module-specific checks
##

if (APPLE)
    set(_SQLITE3_VERSION "sqlite-3.6.14.2-ncbi1")
else (APPLE)
    set(_SQLITE3_VERSION "sqlite-3.7.13-ncbi1")
endif (APPLE)

get_filename_component(SQLITE3_CMAKE_DIR "$ENV{NCBI}/${_SQLITE3_VERSION}" REALPATH)
string(REGEX REPLACE ".*-([0-9].*)" "\\1" SQLITE3_VERSION_STRING "${SQLITE3_CMAKE_DIR}")

set(SQLITE3_FOUND True)
set(SQLITE3_INCLUDE_DIR
    ${SQLITE3_CMAKE_DIR}/include
    )


if (CMAKE_BUILD_TYPE STREQUAL "Debug")

    set(SQLITE3_LIBRARY
        ${SQLITE3_CMAKE_DIR}/${CMAKE_BUILD_TYPE}MT64/lib/libsqlite3${_NCBI_LIBRARY_SUFFIX}
        )

else()
    # release-mode binaries are in /opt

    set(SQLITE3_LIBRARY
        /opt/ncbi/64/${_SQLITE3_VERSION}/${CMAKE_BUILD_TYPE}MT64/lib/libsqlite3${_NCBI_LIBRARY_SUFFIX}
        )

endif()

set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})

# These are for compatibility with case-sensitive versions
set(Sqlite3_FOUND ${SQLITE3_FOUND})
set(Sqlite3_INCLUDE_DIR ${SQLITE3_INCLUDE_DIR})
set(Sqlite3_LIBRARY ${SQLITE3_LIBRARY})
set(Sqlite3_LIBRARIES ${SQLITE3_LIBRARIES})


#############################################################################
##
## Logging
##

if (_NCBI_MODULE_DEBUG)
    message(STATUS "SQLite3 (NCBI): FOUND = ${SQLITE3_FOUND}")
    message(STATUS "SQLite3 (NCBI): VERSION = ${SQLITE3_VERSION_STRING}")
    message(STATUS "SQLite3 (NCBI): INCLUDE = ${SQLITE3_INCLUDE_DIR}")
    message(STATUS "SQLite3 (NCBI): LIBRARIES = ${SQLITE3_LIBRARY}")
endif()

