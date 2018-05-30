#############################################################################
# $Id$
#############################################################################

##
## NCBI CMake components description - UNIX/Linux
##
##
## As a result, the following variables should be defined for component XXX
##  NCBI_COMPONENT_XXX_FOUND
##  NCBI_COMPONENT_XXX_INCLUDE
##  NCBI_COMPONENT_XXX_DEFINES
##  NCBI_COMPONENT_XXX_LIBS
##  HAVE_LIBXXX


# NOTE:
#  for the time being, macros are defined at the end of the file
# ---------------------------------------------------------------------------

set(NCBI_ALL_COMPONENTS "")
#############################################################################


include(CheckLibraryExists)

check_library_exists(dl dlopen "" HAVE_LIBDL)
if(HAVE_LIBDL)
    set(DL_LIBS -ldl)
else(HAVE_LIBDL)
    if (UNIX)
        message(FATAL_ERROR "dl library not found")
    endif(UNIX)
endif(HAVE_LIBDL)

check_library_exists(jpeg jpeg_start_decompress "" HAVE_LIBJPEG)
check_library_exists(gif EGifCloseFile "" HAVE_LIBGIF)
check_library_exists(tiff TIFFClientOpen "" HAVE_LIBTIFF)
check_library_exists(png png_create_read_struct "" HAVE_LIBPNG)

option(USE_LOCAL_BZLIB "Use a local copy of libbz2")
option(USE_LOCAL_PCRE "Use a local copy of libpcre")



############################################################################
# NOTE:
# We conditionally set a package config path
# The existence of files in this directory switches find_package()
# to automatically switch between module mode vs. config mode
#
set(NCBI_TOOLS_ROOT $ENV{NCBI})
if (EXISTS ${NCBI_TOOLS_ROOT})
    set(_NCBI_DEFAULT_PACKAGE_SEARCH_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-system/cmake/ncbi-defaults")
    set(CMAKE_PREFIX_PATH
        ${CMAKE_PREFIX_PATH}
        ${_NCBI_DEFAULT_PACKAGE_SEARCH_PATH}
        )
    message(STATUS "NCBI Root: ${NCBI_TOOLS_ROOT}")
    message(STATUS "CMake Prefix Path: ${CMAKE_PREFIX_PATH}")
else()
    message(STATUS "NCBI Root: <not found>")
endif()

if(WIN32)
    # Specific location overrides for Windows packages
    # Note: this variable must be set in the CMake GUI!!
    #set(WIN32_PACKAGE_ROOT "C:/Users/dicuccio/dev/packages")
    set(GIF_ROOT "${WIN32_PACKAGE_ROOT}/giflib-4.1.4-1-lib")
    set (ENV{GIF_DIR} "${GIF_ROOT}")

    set(PCRE_PKG_ROOT "${WIN32_PACKAGE_ROOT}/pcre-7.9-lib")
    set(PCRE_PKG_INCLUDE_DIRS "${PCRE_PKG_ROOT}/include")
    set(PCRE_PKG_LIBRARY_DIRS "${PCRE_PKG_ROOT}/lib")

    set(GNUTLS_ROOT "${WIN32_PACKAGE_ROOT}/gnutls-3.4.9")
    set(PC_GNUTLS_INCLUDEDIR "${GNUTLS_ROOT}/include")
    set(PC_GNUTLS_LIBDIR "${GNUTLS_ROOT}/lib")

    set(FREETYPE_ROOT "${WIN32_PACKAGE_ROOT}/freetype-2.3.5-1-lib")
    set(ENV{FREETYPE_DIR} "${FREETYPE_ROOT}")

    set(FTGL_ROOT "${WIN32_PACKAGE_ROOT}/ftgl-2.1.3_rc5")
    set(LZO_ROOT "${WIN32_PACKAGE_ROOT}/lzo-2.05-lib")

    set(CMAKE_PREFIX_PATH
        ${CMAKE_PREFIX_PATH}
        "${PCRE_PKG_ROOT}"
        "${GNUTLS_ROOT}"
        "${LZO_ROOT}"
        "${GIF_ROOT}"
        "${WIN32_PACKAGE_ROOT}/libxml2-2.7.8.win32"
        "${WIN32_PACKAGE_ROOT}/libxslt-1.1.26.win32"
        "${WIN32_PACKAGE_ROOT}/iconv-1.9.2.win32"
        "${FREETYPE_ROOT}"
        "${WIN32_PACKAGE_ROOT}/glew-1.5.8"
        "${FTGL_ROOT}"
        "${WIN32_PACKAGE_ROOT}/sqlite3-3.8.10.1"
        "${WIN32_PACKAGE_ROOT}/db-4.6.21"
		"${WIN32_PACKAGE_ROOT}/lmdb-0.9.18"
        )
endif()

#
# Framework for dealing with external libraries
#
include(${top_src_dir}/src/build-system/cmake/FindExternalLibrary.cmake)

############################################################################
#
# PCRE additions
#
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.pcre.cmake)

############################################################################
#
# Compression libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.compress.cmake)



############################################################################
#
# OS-specific settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.os.cmake)

#################################
# Some platform-specific system libs that can be linked eventually
set(THREAD_LIBS   ${CMAKE_THREAD_LIBS_INIT})
if (WIN32)
    set(KSTAT_LIBS    "")
    set(RPCSVC_LIBS   "")
    set(DEMANGLE_LIBS "")
    set(UUID_LIBS      "")
    set(NETWORK_LIBS  "ws2_32.lib")
    set(RT_LIBS          "")
    set(MATH_LIBS      "")
    set(CURL_LIBS      "")
    set(MYSQL_INCLUDE_DIR      "")
endif()

find_library(UUID_LIBS NAMES uuid)
find_library(CRYPT_LIBS NAMES crypt)
find_library(MATH_LIBS NAMES m)

if (APPLE)
  find_library(NETWORK_LIBS c)
  find_library(RT_LIBS c)
else (APPLE)
  find_library(NETWORK_LIBS nsl)
  find_library(RT_LIBS        rt)
endif (APPLE)

#
# Basic Library Definitions
# FIXME: get rid of these
#

set(ORIG_LIBS   ${RT_LIBS} ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set(ORIG_C_LIBS            ${MATH_LIBS} ${CMAKE_THREAD_LIBS_INIT})
set(C_LIBS      ${ORIG_C_LIBS})


# ############################################################
# Specialized libs settings
# Mostly, from Makefile.mk
# ############################################################
#

set(LIBS ${LIBS} ${DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})


#
# NCBI-specific library subsets
# NOTE:
# these should be eliminated or simplified; they're not needed
#
set(LOCAL_LBSM ncbi_lbsm ncbi_lbsm_ipc ncbi_lbsmd)

############################################################################
#
# OS-specific settings
if (UNIX)
    find_library(GMP_LIB LIBS gmp HINTS "$ENV{NCBI}/gmp-6.0.0a/lib64/")
    find_library(IDN_LIB LIBS idn HINTS "/lib64")
    find_library(NETTLE_LIB LIBS nettle HINTS "$ENV{NCBI}/nettle-3.1.1/lib64")
    find_library(HOGWEED_LIB LIBS hogweed HINTS "$ENV{NCBI}/nettle-3.1.1/lib64")
elseif (WIN32)
    set(GMP_LIB "")
    set(IDN_LIB "")
    set(NETTLE_LIB "")
    set(HOGWEED_LIB "")
endif()

find_package(GnuTLS)
if (GnuTLS_FOUND)
    set(GNUTLS_LIBRARIES ${GNUTLS_LIBRARIES} ${ZLIB_LIBRARIES} ${IDN_LIB} ${RT_LIBS} ${HOGWEED_LIB} ${NETTLE_LIB} ${GMP_LIB})
    set(GNUTLS_LIBS ${GNUTLS_LIBRARIES})
endif()

############################################################################
#
# Kerberos 5 (via GSSAPI)
# FIXME: replace with native CMake check
#
find_external_library(KRB5 INCLUDES gssapi/gssapi_krb5.h LIBS gssapi_krb5 krb5 k5crypto com_err)


############################################################################
#
# Sybase stuff
#find_package(Sybase)

if (WIN32)
	find_external_library(Sybase
    DYNAMIC_ONLY
    INCLUDES sybdb.h
    #LIBS libsybblk
	LIBS libsybblk libsybdb64 libsybct libsybcs
    INCLUDE_HINTS "${WIN32_PACKAGE_ROOT}\\sybase-15.5\\include"
	LIBS_HINTS "${WIN32_PACKAGE_ROOT}\\sybase-15.5\\lib")
else (WIN32)
	find_external_library(Sybase
    DYNAMIC_ONLY
    INCLUDES sybdb.h
    LIBS sybblk_r64 sybdb64 sybct_r64 sybcs_r64 sybtcl_r64 sybcomn_r64 sybintl_r64 sybunic64
    HINTS "/opt/sybase/clients/15.7-64bit/OCS-15_0/")
endif (WIN32)

if (NOT SYBASE_FOUND)
	message(FATAL "no sybase found." )
endif (NOT SYBASE_FOUND)

set(SYBASE_DBLIBS "${SYBASE_LIBS}")

############################################################################
#
# FreeTDS
# FIXME: do we need these anymore?
#
set(ftds64   ftds64)
set(FTDS64_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds64/freetds)
set(FTDS64_INCLUDE    ${FTDS64_CTLIB_INCLUDE})

set(ftds95   ftds95)
set(FTDS95_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS95_CTLIB_LIB   ct_ftds95 tds_ftds95)
set(FTDS95_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds95/freetds)
set(FTDS95_LIBS        ${FTDS95_CTLIB_LIBS})
set(FTDS95_LIB        ${FTDS95_CTLIB_LIB})
set(FTDS95_INCLUDE    ${FTDS95_CTLIB_INCLUDE})

set(ftds100   ftds100)
set(FTDS100_CTLIB_LIBS  ${ICONV_LIBS} ${KRB5_LIBS})
set(FTDS100_CTLIB_LIB   ct_ftds100 tds_ftds100)
set(FTDS100_CTLIB_INCLUDE ${includedir}/dbapi/driver/ftds100/freetds)
set(FTDS100_LIBS        ${FTDS100_CTLIB_LIBS})
set(FTDS100_LIB        ${FTDS100_CTLIB_LIB})
set(FTDS100_INCLUDE    ${FTDS100_CTLIB_INCLUDE})

set(ftds          ftds95)
set(FTDS_LIBS     ${FTDS95_LIBS})
set(FTDS_LIB      ${FTDS95_LIB})
set(FTDS_INCLUDE  ${FTDS95_INCLUDE})

#OpenSSL
find_package(OpenSSL)
if (OpenSSL_FOUND)
    set(OpenSSL_LIBRARIES ${OpenSSL_LIBRARIES} ${Z_LIBS} ${DL_LIBS})
    set(OPENSSL_LIBS ${OpenSSL_LIBRARIES})
endif()

#EXTRALIBS were taken from mysql_config --libs
find_external_library(Mysql INCLUDES mysql/mysql.h LIBS mysqlclient EXTRALIBS ${Z_LIBS} ${CRYPT_LIBS} ${NETWORK_LIBS} ${MATH_LIBS} ${OPENSSL_LIBS})

############################################################################
#
# BerkeleyDB
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.BerkeleyDB.cmake)

# ODBC
# FIXME: replace with native CMake check
#
set(ODBC_INCLUDE  ${includedir}/dbapi/driver/odbc/unix_odbc 
                  ${includedir0}/dbapi/driver/odbc/unix_odbc)
set(ODBC_LIBS)

# Python
find_external_library(Python
    INCLUDES Python.h
    LIBS python2.7
    INCLUDE_HINTS "/opt/python-2.7env/include/python2.7/"
    LIBS_HINTS "/opt/python-2.7env/lib/")

############################################################################
#
# Boost settings
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.boost.cmake)

############################################################################
#
# NCBI C Toolkit:  headers and libs
string(REGEX MATCH "DNCBI_INT8_GI|NCBI_STRICT_GI" INT8GI_FOUND "${CMAKE_CXX_FLAGS}")
if (NOT "${INT8GI_FOUND}" STREQUAL "")
    if (EXISTS "${NCBI_TOOLS_ROOT}/ncbi/ncbi.gi64/")
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/ncbi.gi64/")
    elseif (EXISTS "${NCBI_TOOLS_ROOT}/ncbi.gi64")
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi.gi64/")
    else ()
        set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/")
    endif ()
else ()
    set(NCBI_CTOOLKIT_PATH "${NCBI_TOOLS_ROOT}/ncbi/")
endif ()

if (EXISTS "${NCBI_CTOOLKIT_PATH}/include64" AND EXISTS "${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_INCLUDE  "${NCBI_CTOOLKIT_PATH}/include64")
    set(NCBI_C_LIBPATH  "${NCBI_CTOOLKIT_PATH}/lib64")
    set(NCBI_C_ncbi     "ncbi")
    if (APPLE)
        set(NCBI_C_ncbi ${NCBI_C_ncbi} -Wl,-framework,ApplicationServices)
    endif ()
    set(HAVE_NCBI_C true)
elseif (WIN32)
    set(NCBI_CTOOLKIT_WIN32 "//snowman/win-coremake/Lib/Ncbi/C_Toolkit/vs2015.64/c.current")
    if (EXISTS "${NCBI_CTOOLKIT_WIN32}")
        set(NCBI_C_INCLUDE "${NCBI_CTOOLKIT_WIN32}/include")
        set(NCBI_C_LIBPATH "${NCBI_CTOOLKIT_WIN32}/lib")
        set(NCBI_C_ncbi    "ncbi.lib")
        set(HAVE_NCBI_C true)
    else()
        set(HAVE_NCBI_C false)
    endif()
else ()
    set(HAVE_NCBI_C false)
endif ()

message(STATUS "HAVE_NCBI_C = ${HAVE_NCBI_C}")
message(STATUS "NCBI_C_INCLUDE = ${NCBI_C_INCLUDE}")
message(STATUS "NCBI_C_LIBPATH = ${NCBI_C_LIBPATH}")

############################################################################
#
# OpenGL: headers and libs (including core X dependencies) for code
# not using other toolkits.  (The wxWidgets variables already include
# these as appropriate.)

find_package(OpenGL)
if (WIN32)
    set(GLEW_INCLUDE ${WIN32_PACKAGE_ROOT}/glew-1.5.8/include)
    set(GLEW_LIBRARIES ${WIN32_PACKAGE_ROOT}/glew-1.5.8/lib/glew32mx.lib)
else()
    find_package(GLEW)
endif()
find_package(OSMesa)
if (${OPENGL_FOUND})
    set(OPENGL_INCLUDE "${OPENGL_INCLUDE_DIR}")
    set(OPENGL_LIBS "${OPENGL_LIBRARIES}")

    set(OPENGL_INCLUDE ${OPENGL_INCLUDE_DIRS})
    set(OPENGL_LIBS ${OPENGL_LIBRARIES})
    set(GLEW_INCLUDE ${GLEW_INCLUDE_DIRS})
    set(GLEW_LIBS ${GLEW_LIBRARIES})
    set(OSMESA_INCLUDE ${OSMesa_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIRS})
    set(OSMESA_LIBS ${OSMesa_LIBRARIES} ${OPENGL_LIBRARIES})

endif()

############################################################################
#
# wxWidgets
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.wxwidgets.cmake)

# Fast-CGI
if (APPLE)
  find_external_library(FastCGI
      INCLUDES fastcgi.h
      LIBS fcgi
      HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0")
else ()
    if ("${BUILD_SHARED_LIBS}" STREQUAL "OFF")
        find_external_library(FastCGI
            INCLUDES fastcgi.h
            LIBS fcgi
            INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include"
            LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/lib"
            EXTRALIBS ${NETWORK_LIBS})
    else ()
    find_external_library(FastCGI
        INCLUDES fastcgi.h
        LIBS fcgi
        INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/include"
        LIBS_HINTS "${NCBI_TOOLS_ROOT}/fcgi-2.4.0/shlib"
        EXTRALIBS ${NETWORK_LIBS})
  endif()
endif()

# Fast-CGI lib:  (module to add to the "xcgi" library)
set(FASTCGI_OBJS    fcgibuf)

# NCBI SSS:  headers, library path, libraries
set(NCBI_SSS_INCLUDE ${incdir}/sss)
set(NCBI_SSS_LIBPATH )
set(LIBSSSUTILS      -lsssutils)
set(LIBSSSDB         -lsssdb -lssssys)
set(sssutils         sssutils)
set(NCBILS2_LIB      ncbils2_cli ncbils2_asn ncbils2_cmn)
set(NCBILS_LIB       ${NCBILS2_LIB})


# SP:  headers, libraries
set(SP_INCLUDE )
set(SP_LIBS    )

# ORBacus CORBA headers, library path, libraries
set(ORBACUS_INCLUDE )
set(ORBACUS_LIBPATH )
set(LIBOB           )
# LIBIMR should be empty for single-threaded builds
set(LIBIMR          )

# IBM's International Components for Unicode
find_external_library(ICU
    INCLUDES unicode/ucnv.h
    LIBS icui18n icuuc icudata
    HINTS "${NCBI_TOOLS_ROOT}/icu-49.1.1")


##############################################################################
## LibXml2 / LibXsl
##
find_library(GCRYPT_LIBS NAMES gcrypt HINTS "$ENV{NCBI}/libgcrypt/${CMAKE_BUILD_TYPE}/lib")
find_library(GPG_ERROR_LIBS NAMES gpg-error HINTS "$ENV{NCBI}/libgpg-error/${CMAKE_BUILD_TYPE}/lib")
if (NOT GCRYPT_LIBS STREQUAL "GCRYPT_LIBS-NOTFOUND")
    set(GCRYPT_FOUND True)
    set(GCRYPT_LIBS ${GCRYPT_LIBS} ${GPG_ERROR_LIBS})
else()
    set(GCRYPT_FOUND False)
endif()

# ICONV
# Windows requires special handling for this
if (WIN32)
    find_external_library(iconv
        INCLUDES iconv.h
        LIBS iconv)
endif()

find_package(LibXml2)
if (LIBXML2_FOUND)
    set(LIBXML_INCLUDE ${LIBXML2_INCLUDE_DIR})
    set(LIBXML_LIBS    ${LIBXML2_LIBRARIES})
    if (WIN32)
        set(LIBXML_INCLUDE ${LIBXML_INCLUDE} ${ICONV_INCLUDE})
        set(LIBXML_LIBS ${LIBXML_LIBS} ${ICONV_LIBS})
    endif()
endif()

find_package(LibXslt)
if (LIBXSLT_FOUND)
    set(LIBXSLT_INCLUDE  ${LIBXSLT_INCLUDE_DIR})
    set(LIBXSLT_LIBS     ${LIBXSLT_EXSLT_LIBRARIES} ${LIBXSLT_LIBRARIES} ${LIBXML_LIBS})
    set(LIBEXSLT_INCLUDE ${LIBXSLT_INCLUDE_DIR})
    set(LIBEXSLT_LIBS    ${LIBXSLT_EXSLT_LIBRARIES})
    if (GCRYPT_FOUND)
        set(LIBXSLT_LIBS     ${LIBXSLT_LIBS} ${GCRYPT_LIBS})
        set(LIBEXSLT_LIBS    ${LIBEXSLT_LIBS} ${GCRYPT_LIBS})
    endif()
endif()


find_external_library(xerces
    INCLUDES xercesc/dom/DOM.hpp
    LIBS xerces-c
    HINTS "${NCBI_TOOLS_ROOT}/xerces-3.1.1/GCC442-DebugMT64")

find_external_library(xalan
    INCLUDES xalanc/XalanTransformer/XalanTransformer.hpp
    LIBS xalan-c xalanMsg
    HINTS "${NCBI_TOOLS_ROOT}/xalan-1.11~r1302529/GCC442-DebugMT64")

# Sun Grid Engine (libdrmaa):
# libpath - /netmnt/uge/lib/lx-amd64/
find_external_library(SGE
    INCLUDES drmaa.h
    LIBS drmaa
    INCLUDE_HINTS "/netmnt/gridengine/current/include"
    LIBS_HINTS "/netmnt/gridengine/current/lib/lx-amd64/")

# muParser
find_external_library(muparser
    INCLUDES muParser.h
    LIBS muparser
    INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/include"
    LIBS_HINTS "${NCBI_TOOLS_ROOT}/muParser-1.30/GCC-Debug64/lib/")

# HDF5
find_external_library(hdf5
    INCLUDES hdf5.h
    LIBS hdf5
    HINTS "${NCBI_TOOLS_ROOT}/hdf5-1.8.3/GCC401-Debug64/")

############################################################################
#
# SQLite3
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.sqlite3.cmake)

############################################################################
#
# Various image-format libraries
include(${top_src_dir}/src/build-system/cmake/CMakeChecks.image.cmake)

#############################################################################
## MongoDB

find_library(SASL2_LIBS sasl2)

if (NOT WIN32)
    find_package(MongoCXX)
endif()
if (MONGOCXX_FOUND)
    set(MONGOCXX_INCLUDE ${MONGOCXX_INCLUDE_DIRS} ${BSONCXX_INCLUDE_DIRS})
    set(MONGOCXX_LIB ${MONGOCXX_LIBPATH} ${MONGOCXX_LIBRARIES} ${BSONCXX_LIBRARIES} ${OPENSSL_LIBS} ${SASL2_LIBS})
endif()
message(STATUS "MongoCXX Includes: ${MONGOCXX_INCLUDE}")

##############################################################################
##
## LevelDB

find_package(LEVELDB
             PATHS
             /panfs/pan1/gpipe/ThirdParty/leveldb-1.20
             /usr/local
             /usr
             NO_DEFAULT_PATH
)
############################################################################
##
## wgMLST

if (NOT WIN32)
    find_package(SKESA)
endif()
if (WGMLST_FOUND)
    set(WGMLST_INCLUDE ${WGMLST_INCLUDE_DIRS})
    set(WGMLST_LIB ${WGMLST_LIBPATH} ${WGMLST_LIBRARIES})
endif()
message(STATUS "wgMLST Includes: ${WGMLST_INCLUDE}")

# libmagic (file-format identification)
find_library(MAGIC_LIBS magic)

# libcurl (URL retrieval)
find_library(CURL_LIBS curl)

# libmimetic (MIME handling)
find_external_library(mimetic
    INCLUDES mimetic/mimetic.h
    LIBS mimetic
    HINTS "${NCBI_TOOLS_ROOT}/mimetic-0.9.7-ncbi1/")

# libgSOAP++
find_external_library(gsoap
    INCLUDES stdsoap2.h
    LIBS gsoapssl++
    INCLUDE_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/include"
    LIBS_HINTS "${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/lib/"
    EXTRALIBS ${Z_LIBS})

set(GSOAP_SOAPCPP2 ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/soapcpp2)
set(GSOAP_WSDL2H   ${NCBI_TOOLS_ROOT}/gsoap-2.8.15/GCC442-DebugMT64/bin/wsdl2h)

# Compress
set(COMPRESS_LDEP ${CMPRS_LIB})
set(COMPRESS_LIBS xcompress ${COMPRESS_LDEP})


#################################
# Useful sets of object libraries
#################################


set(SOBJMGR_LIBS xobjmgr)
set(ncbi_xreader_pubseqos ncbi_xreader_pubseqos)
set(ncbi_xreader_pubseqos2 ncbi_xreader_pubseqos2)
set(OBJMGR_LIBS ncbi_xloader_genbank)


# Overlapping with qall is poor, so we have a second macro to make it
# easier to stay out of trouble.
set(QOBJMGR_ONLY_LIBS xobjmgr id2 seqsplit id1 genome_collection seqset
    ${SEQ_LIBS} pub medline biblio general-lib xcompress ${CMPRS_LIB})
set(QOBJMGR_LIBS ${QOBJMGR_ONLY_LIBS} qall)
set(QOBJMGR_STATIC_LIBS ${QOBJMGR_ONLY_LIBS} qall)

# EUtils
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout
              einfo uilist ehistory)


#
# SRA/VDB stuff
if (WIN32)
	find_external_library(VDB
		INCLUDES sra/sradb.h
		LIBS ncbi-vdb
		INCLUDE_HINTS "\\\\snowman\\trace_software\\vdb\\vdb-versions\\2.8.0\\interfaces"
		LIBS_HINTS "\\\\snowman\\trace_software\\vdb\\vdb-versions\\2.8.0\\win\\release\\x86_64\\lib")
else (WIN32)
	find_external_library(VDB
		INCLUDES sra/sradb.h
		LIBS ncbi-vdb
		INCLUDE_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/2.8.0/interfaces"
		LIBS_HINTS "/opt/ncbi/64/trace_software/vdb/vdb-versions/2.8.0/linux/release/x86_64/lib")
endif (WIN32)

if (${VDB_FOUND})
	if (WIN32)
		set(VDB_INCLUDE "${VDB_INCLUDE}" "${VDB_INCLUDE}\\os\\win" "${VDB_INCLUDE}\\cc\\vc++\\x86_64" "${VDB_INCLUDE}\\cc\\vc++")
	else (WIN32)
		set(VDB_INCLUDE "${VDB_INCLUDE}" "${VDB_INCLUDE}/os/linux" "${VDB_INCLUDE}/os/unix" "${VDB_INCLUDE}/cc/gcc/x86_64" "${VDB_INCLUDE}/cc/gcc")
	endif(WIN32)
    set(SRA_INCLUDE ${VDB_INCLUDE})
    set(SRA_SDK_SYSLIBS ${VDB_LIBS})
    set(SRA_SDK_LIBS ${VDB_LIBS})
    set(SRAXF_LIBS ${SRA_SDK_LIBS})
    set(SRA_LIBS ${SRA_SDK_LIBS})
    set(BAM_LIBS ${SRA_SDK_LIBS})
    set(SRAREAD_LDEP ${SRA_SDK_LIBS})
    set(SRAREAD_LIBS sraread ${SRA_SDK_LIBS})
    set(HAVE_NCBI_VDB 1)
endif ()

# Makefile.blast_macros.mk
set(BLAST_DB_DATA_LOADER_LIBS ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt)
set(BLAST_FORMATTER_MINIMAL_LIBS xblastformat align_format taxon1 blastdb_format
    gene_info xalnmgr blastxml xcgi xhtml)
set(BLAST_INPUT_LIBS blastinput
    ${BLAST_DB_DATA_LOADER_LIBS} ${BLAST_FORMATTER_MINIMAL_LIBS})

set(BLAST_FORMATTER_LIBS ${BLAST_INPUT_LIBS})

# Libraries required to link against the internal BLAST SRA library
set(BLAST_SRA_LIBS blast_sra ${SRAXF_LIBS} vxf ${SRA_LIBS})

# BLAST_FORMATTER_LIBS and BLAST_INPUT_LIBS need $BLAST_LIBS
set(BLAST_LIBS xblast xalgoblastdbindex composition_adjustment
xalgodustmask xalgowinmask seqmasks_io seqdb blast_services xobjutil
${OBJREAD_LIBS} xnetblastcli xnetblast blastdb scoremat tables xalnmgr)



# SDBAPI stuff
set(SDBAPI_LIB sdbapi) # ncbi_xdbapi_ftds ${FTDS_LIB} dbapi dbapi_driver ${XCONNEXT})


set(VARSVC_LIBS varsvcutil varsvccli varsvcobj)


# Entrez Libs
set(ENTREZ_LIBS entrez2cli entrez2)
set(EUTILS_LIBS eutils egquery elink epost esearch espell esummary linkout einfo uilist ehistory)

#GLPK
find_external_library(glpk
    INCLUDES glpk.h
    LIBS glpk
    HINTS "/usr/local/glpk/4.45")

find_external_library(samtools
    INCLUDES bam.h
    LIBS bam
    HINTS "${NCBI_TOOLS_ROOT}/samtools")

# libbackward
find_path(LIBBACKWARD_INCLUDE_DIR
    backward.hpp
    HINTS $ENV{NCBI}/backward-cpp-1.3/include)
if (LIBBACKWARD_INCLUDE_DIR)
    message(STATUS "Found libbackward: ${LIBBACKWARD_INCLUDE_DIR}")
    set(HAVE_LIBBACKWARD_CPP 1)
endif()

# libdw
find_library(LIBDW_LIBRARIES NAMES libdw.so HINTS /usr/lib64)
if (LIBDW_LIBRARIES)
    set(HAVE_LIBDW 1)
    message(STATUS "Found libdw: ${LIBDW_LIBRARIES}")
endif()


#LAPACK
check_include_file(lapacke.h HAVE_LAPACKE_H)
check_include_file(lapacke/lapacke.h HAVE_LAPACKE_LAPACKE_H)
check_include_file(Accelerate/Accelerate.h HAVE_ACCELERATE_ACCELERATE_H)
#find_external_library(LAPACK LIBS lapack blas)
find_package(LAPACK)
if (LAPACK_FOUND)
    set(LAPACK_INCLUDE ${LAPACK_INCLUDE_DIRS})
    set(LAPACK_LIBS ${LAPACK_LIBRARIES})
else ()
    find_library(LAPACK_LIBS lapack blas)
endif ()

#LMDB
if (WIN32)
	find_external_library(LMDB
		STATIC_ONLY
		INCLUDES lmdb.h 
		LIBS liblmdb 
		INCLUDE_HINTS "${WIN32_PACKAGE_ROOT}\\lmdb-0.9.18\\include" 
		LIB_HINTS "${WIN32_PACKAGE_ROOT}\\lmdb-0.9.18\\lib")
else (WIN32)
	find_external_library(LMDB INCLUDES lmdb.h LIBS lmdb HINTS "${NCBI_TOOLS_ROOT}/lmdb-0.9.18" EXTRALIBS pthread)
endif (WIN32)

if (LMDB_INCLUDE)
    set(HAVE_LIBLMDB 1)
endif()

find_external_library(libxlsxwriter
    INCLUDES xlsxwriter.h
    LIBS xlsxwriter
    HINTS "${NCBI_TOOLS_ROOT}/libxlsxwriter-0.6.9"
    EXTRALIBS ${Z_LIBS})

find_external_library(LIBUNWIND INCLUDES libunwind.h LIBS unwind HINTS "${NCBI_TOOLS_ROOT}/libunwind-1.1")
set(HAVE_LIBUNWIND ${LIBUNWIND_FOUND})

if (EXISTS "${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94/include/gsl/gsl")
    set(MSGSL_INCLUDE "${NCBI_TOOLS_ROOT}/msgsl-0.0.20171114-1c95f94/include/")
else()
    find_path(MSGSL_INCLUDE NAMES gsl/gsl)
endif()

if (NOT "${MSGSL_INCLUDE}" STREQUAL "")
    set(HAVE_MSGSL true)
    message(STATUS "Found MSGSL: ${MSGSL_INCLUDE}")
else()
    message(STATUS "Could NOT find MSGSL")
endif()

# temporarily include the standard python path when searching for Python3
set(CMAKE_PREFIX_PATH_ORIG ${CMAKE_PREFIX_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/python-all/)

find_package(PythonInterp 3)
if (PYTHONINTERP_FOUND)
    if (NOT "$ENV{TEAMCITY_VERSION}" STREQUAL "")
        message(STATUS "Generating ${build_root}/run_with_cd_reporter.py...")
        message(STATUS "Python3 path: ${PYTHON_EXECUTABLE}")

        set(PYTHON3 ${PYTHON_EXECUTABLE})
        set(CD_REPORTER "/am/ncbiapdata/bin/cd_reporter")
        set(abs_top_srcdir ${abs_top_src_dir})
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build-system/run_with_cd_reporter.py.in ${build_root}/build-system/run_with_cd_reporter.py)

        # copy to build_root and set executable permissions (workaround because configure_file doesn't set permissions)
        file(COPY ${build_root}/build-system/run_with_cd_reporter.py DESTINATION ${build_root}
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${build_root}/run_with_cd_reporter.py)
    else()
        message(STATUS "Detected development build, cd_reporter disabled")
    endif()
else(PYTHONINTERP_FOUND)
    message(STATUS "Could not find Python3. Disabling cd_reporter.")
endif(PYTHONINTERP_FOUND)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH_ORIG})


if (WIN32)
	set(win_include_directories 
		"${PCRE_PKG_INCLUDE_DIRS}" 
		"${PC_GNUTLS_INCLUDEDIR}"
        "${WIN32_PACKAGE_ROOT}/sqlite3-3.8.10.1/include"
		"${WIN32_PACKAGE_ROOT}/db-4.6.21/include"
        "${WIN32_PACKAGE_ROOT}/libxml2-2.7.8.win32/include"
        "${WIN32_PACKAGE_ROOT}/libxslt-1.1.26.win32/include"
        "${WIN32_PACKAGE_ROOT}/iconv-1.9.2.win32/include"
		"${WIN32_PACKAGE_ROOT}/lmdb-0.9.18/include" )
	include_directories(${win_include_directories})
endif (WIN32)

#############################################################################
#############################################################################
#############################################################################
##
##          NCBI CMake wrapper adapter
##
#############################################################################
#############################################################################

set(NCBI_COMPONENT_unix_FOUND YES)
set(NCBI_COMPONENT_Linux_FOUND YES)

#############################################################################
# NCBI_C
if(HAVE_NCBI_C)
  message("NCBI_C found at ${NCBI_C_INCLUDE}")
  set(NCBI_COMPONENT_NCBI_C_FOUND YES)
  set(NCBI_COMPONENT_NCBI_C_INCLUDE ${NCBI_C_INCLUDE})

  set(_c_libs  ncbi)
  foreach( _lib IN LISTS _c_libs)
    set(NCBI_COMPONENT_NCBI_C_LIBS ${NCBI_COMPONENT_NCBI_C_LIBS} "${NCBI_C_LIBPATH}/lib${_lib}.a")
  endforeach()
  set(NCBI_COMPONENT_NCBI_C_DEFINES HAVE_NCBI_C=1)
else()
  message("Component NCBI_C ERROR: ${NCBI_C_INCLUDE} not found")
  set(NCBI_COMPONENT_NCBI_C_FOUND NO)
endif()

#############################################################################
# local_lbsm
if (EXISTS ${NCBI_SRC_ROOT}/connect/ncbi_lbsm.c)
  message("local_lbsm found at ${NCBI_SRC_ROOT}/connect")
  set(NCBI_COMPONENT_local_lbsm_FOUND YES)
  set(HAVE_LOCAL_LBSM 1)
else()
  message("Component local_lbsm ERROR: not found")
  set(NCBI_COMPONENT_local_lbsm_FOUND NO)
endif()

#############################################################################
# TLS
if(GnuTLS_FOUND)
  message("TLS found at ${GNUTLS_INCLUDE}")
  set(NCBI_COMPONENT_TLS_FOUND YES)
  set(NCBI_COMPONENT_TLS_INCLUDE ${GNUTLS_INCLUDE})
  set(NCBI_COMPONENT_TLS_LIBS ${GNUTLS_LIBS})
  set(HAVE_LIBGNUTLS 1)
else()
  set(NCBI_COMPONENT_TLS_FOUND NO)
endif()

#############################################################################
# Boost.Test.Included
if(Boost_FOUND)
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND YES)
  set(NCBI_COMPONENT_Boost.Test.Included_INCLUDE ${Boost_INCLUDE_DIRS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Boost.Test.Included")
else()
  set(NCBI_COMPONENT_Boost.Test.Included_FOUND NO)
endif()

#############################################################################
#LocalPCRE
if (EXISTS ${includedir}/util/regexp)
  set(NCBI_COMPONENT_LocalPCRE_FOUND YES)
  set(NCBI_COMPONENT_LocalPCRE_INCLUDE ${includedir}/util/regexp)
  set(NCBI_COMPONENT_LocalPCRE_LIBS regexp)
else()
  set(NCBI_COMPONENT_LocalPCRE_FOUND NO)
endif()

#############################################################################
# PCRE
if(PCRE_FOUND)
  set(NCBI_COMPONENT_PCRE_FOUND YES)
  set(NCBI_COMPONENT_PCRE_INCLUDE ${PCRE_INCLUDE_DIR})
  set(NCBI_COMPONENT_PCRE_LIBS ${PCRE_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} PCRE")
else()
  set(NCBI_COMPONENT_PCRE_FOUND ${NCBI_COMPONENT_LocalPCRE_FOUND})
  set(NCBI_COMPONENT_PCRE_INCLUDE ${NCBI_COMPONENT_LocalPCRE_INCLUDE})
  set(NCBI_COMPONENT_PCRE_LIBS ${NCBI_COMPONENT_LocalPCRE_LIBS})
endif()

#############################################################################
#LocalZ
if (EXISTS ${includedir}/util/compress/zlib)
  set(NCBI_COMPONENT_LocalZ_FOUND YES)
  set(NCBI_COMPONENT_LocalZ_INCLUDE ${includedir}/util/compress/zlib)
  set(NCBI_COMPONENT_LocalZ_LIBS z)
else()
  set(NCBI_COMPONENT_LocalZ_FOUND NO)
endif()

#############################################################################
# Z
if(ZLIB_FOUND)
  set(NCBI_COMPONENT_Z_FOUND YES)
  set(NCBI_COMPONENT_Z_INCLUDE ${ZLIB_INCLUDE_DIR})
  set(NCBI_COMPONENT_Z_LIBS ${ZLIB_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} Z")
else()
  set(NCBI_COMPONENT_Z_FOUND ${NCBI_COMPONENT_LocalZ_FOUND})
  set(NCBI_COMPONENT_Z_INCLUDE ${NCBI_COMPONENT_LocalZ_INCLUDE})
  set(NCBI_COMPONENT_Z_LIBS ${NCBI_COMPONENT_LocalZ_LIBS})
endif()

#############################################################################
#LocalBZ2
if (EXISTS ${includedir}/util/compress/bzip2)
  set(NCBI_COMPONENT_LocalBZ2_FOUND YES)
  set(NCBI_COMPONENT_LocalBZ2_INCLUDE ${includedir}/util/compress/bzip2)
  set(NCBI_COMPONENT_LocalBZ2_LIBS bz2)
else()
  set(NCBI_COMPONENT_LocalBZ2_FOUND NO)
endif()

#############################################################################
# BZ2
if(BZIP2_FOUND)
  set(NCBI_COMPONENT_BZ2_FOUND YES)
  set(NCBI_COMPONENT_BZ2_INCLUDE ${BZIP2_INCLUDE_DIR})
  set(NCBI_COMPONENT_BZ2_LIBS ${BZIP2_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} BZ2")
else()
  set(NCBI_COMPONENT_BZ2_FOUND ${NCBI_COMPONENT_LocalBZ2_FOUND})
  set(NCBI_COMPONENT_BZ2_INCLUDE ${NCBI_COMPONENT_LocalBZ2_INCLUDE})
  set(NCBI_COMPONENT_BZ2_LIBS ${NCBI_COMPONENT_LocalBZ2_LIBS})
endif()

#############################################################################
#LZO
if (LZO_FOUND)
  set(NCBI_COMPONENT_LZO_FOUND YES)
  set(NCBI_COMPONENT_LZO_INCLUDE ${LZO_INCLUDE_DIR})
  set(NCBI_COMPONENT_LZO_LIBS ${LZO_LIBRARIES})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LZO")
else()
  set(NCBI_COMPONENT_LZO_FOUND YES)
endif()

#############################################################################
#LocalLMDB
if (EXISTS ${includedir}/util/lmdb)
  set(NCBI_COMPONENT_LocalLMDB_FOUND YES)
  set(NCBI_COMPONENT_LocalLMDB_INCLUDE ${includedir}//util/lmdb)
  set(NCBI_COMPONENT_LocalLMDB_LIBS lmdb)
else()
  set(NCBI_COMPONENT_LocalLMDB_FOUND NO)
endif()

#############################################################################
#LMDB
if(LMDB_FOUND)
  set(NCBI_COMPONENT_LMDB_FOUND YES)
  set(NCBI_COMPONENT_LMDB_INCLUDE ${LMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_LIBS ${LMDB_LIBS})
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} LMDB")
else()
  set(NCBI_COMPONENT_LMDB_FOUND ${NCBI_COMPONENT_LocalLMDB_FOUND})
  set(NCBI_COMPONENT_LMDB_INCLUDE ${NCBI_COMPONENT_LocalLMDB_INCLUDE})
  set(NCBI_COMPONENT_LMDB_LIBS ${NCBI_COMPONENT_LocalLMDB_LIBS})
endif()

#############################################################################
# JPEG
if(HAVE_LIBJPEG)
  set(NCBI_COMPONENT_JPEG_FOUND YES)
  set(NCBI_COMPONENT_JPEG_LIBS -ljpeg)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} JPEG")
else()
  set(NCBI_COMPONENT_JPEG_FOUND NO)
endif()

#############################################################################
# PNG
if(HAVE_LIBPNG)
  set(NCBI_COMPONENT_PNG_FOUND YES)
  set(NCBI_COMPONENT_PNG_LIBS -lpng)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} PNG")
else()
  set(NCBI_COMPONENT_PNG_FOUND NO)
endif()

#############################################################################
# GIF
set(NCBI_COMPONENT_GIF_FOUND YES)
  set(NCBI_COMPONENT_GIF_LIBS -lgif)
set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} GIF")

#############################################################################
# TIFF
if(HAVE_LIBTIFF)
  set(NCBI_COMPONENT_TIFF_FOUND YES)
  set(NCBI_COMPONENT_TIFF_LIBS -ltiff)
  set(NCBI_ALL_COMPONENTS "${NCBI_ALL_COMPONENTS} TIFF")
else()
  set(NCBI_COMPONENT_TIFF_FOUND NO)
endif()


