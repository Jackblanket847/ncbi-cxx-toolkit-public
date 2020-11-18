#############################################################################
# $Id$
#############################################################################
##
##  NCBI CMake wrapper: Test driver
##    Author: Andrei Gourianov, gouriano@ncbi
##

cmake_policy(SET CMP0057 NEW)

string(REPLACE " " ";" NCBITEST_ARGS    "${NCBITEST_ARGS}")
string(REPLACE " " ";" NCBITEST_ASSETS  "${NCBITEST_ASSETS}")

# Include parameters file from a configure
if(NOT IS_ABSOLUTE ${NCBITEST_PARAMS})
    set(NCBITEST_PARAMS ${CMAKE_CURRENT_BINARY_DIR}/${NCBITEST_PARAMS})
endif()
include(${NCBITEST_PARAMS}) 

# Update signature
string(REPLACE "\$<CONFIG>" "${NCBITEST_CONFIG}" _x "${NCBITEST_SIGNATURE}")
set(NCBITEST_SIGNATURE ${_x})

# Extend $FEATURES if set already (NTest sets it, CTest doesn't)
list(APPEND NCBITEST_FEATURES $ENV{FEATURES})
list(APPEND NCBITEST_FEATURES ${NCBITEST_CONFIG})
list(REMOVE_DUPLICATES NCBITEST_FEATURES)
list(SORT NCBITEST_FEATURES)
string(REPLACE ";" " " _x "${NCBITEST_FEATURES}")
set(ENV{FEATURES} ${_x})
    

# ---------------------------------------------------------------------------
# Set directories

if(NOT IS_ABSOLUTE ${NCBITEST_BINDIR})
    set(NCBITEST_BINDIR ${CMAKE_CURRENT_BINARY_DIR}/${NCBITEST_BINDIR})
endif()
if(NOT IS_ABSOLUTE ${NCBITEST_LIBDIR})
    set(NCBITEST_LIBDIR ${CMAKE_CURRENT_BINARY_DIR}/${NCBITEST_LIBDIR})
endif()
if(NOT IS_ABSOLUTE ${NCBITEST_OUTDIR})
    set(NCBITEST_OUTDIR ${CMAKE_CURRENT_BINARY_DIR}/${NCBITEST_OUTDIR})
endif()
if(NOT IS_ABSOLUTE ${NCBITEST_SOURCEDIR})
    set(NCBITEST_SOURCEDIR ${CMAKE_CURRENT_BINARY_DIR}/${NCBITEST_SOURCEDIR})
endif()
if(NOT IS_ABSOLUTE ${NCBITEST_SCRIPTDIR})
    set(NCBITEST_SCRIPTDIR ${CMAKE_CURRENT_BINARY_DIR}/${NCBITEST_SCRIPTDIR})
endif()
set(NCBITEST_CHECK_SCRIPTDIR ${NCBITEST_SCRIPTDIR}/common/check)
if(WIN32 OR XCODE)
    set(NCBITEST_BINDIR ${NCBITEST_BINDIR}/${NCBITEST_CONFIG})
    set(NCBITEST_LIBDIR ${NCBITEST_LIBDIR}/${NCBITEST_CONFIG})
    set(NCBITEST_OUTDIR ${NCBITEST_OUTDIR}/${NCBITEST_CONFIG})
endif()
set(NCBITEST_OUTDIR ${NCBITEST_OUTDIR}/${NCBITEST_XOUTDIR})
set(NCBITEST_SOURCEDIR ${NCBITEST_SOURCEDIR}/${NCBITEST_XOUTDIR})


# ---------------------------------------------------------------------------
# Check and copy assets

if (NOT "${NCBITEST_ASSETS}" STREQUAL "")
    list(REMOVE_DUPLICATES NCBITEST_ASSETS)
    foreach(_res IN LISTS NCBITEST_ASSETS)
        if (NOT EXISTS ${NCBITEST_SOURCEDIR}/${_res})
            message(WARNING "Test ${NCBITEST_NAME} WARNING: test asset ${NCBITEST_SOURCEDIR}/${_res} not found")
        endif()
    endforeach()
endif()
string(RANDOM _subdir)
set(_subdir ${NCBITEST_NAME}_${_subdir})
file(MAKE_DIRECTORY ${NCBITEST_OUTDIR}/${_subdir})
set(_workdir ${NCBITEST_OUTDIR}/${_subdir})
foreach(_res IN LISTS NCBITEST_ASSETS)
    if (EXISTS ${NCBITEST_SOURCEDIR}/${_res})
        file(COPY ${NCBITEST_SOURCEDIR}/${_res} DESTINATION ${_workdir})
    endif()
endforeach()


# ---------------------------------------------------------------------------
# Report files

set(_tmp_out   ${NCBITEST_OUTDIR}/${NCBITEST_NAME}.tmp_out.txt)
set(_test_out  ${NCBITEST_OUTDIR}/${NCBITEST_NAME}.test_out.txt)
file(TO_NATIVE_PATH "${NCBITEST_OUTDIR}/${NCBITEST_NAME}.boost_rep.txt" _boost_rep)
set(_test_rep  ${NCBITEST_OUTDIR}/${NCBITEST_NAME}.test_rep.txt)
if(EXISTS ${_tmp_out})
    file(REMOVE ${_tmp_out})
endif()
if(EXISTS ${_test_out})
    file(REMOVE ${_test_out})
endif()
if(EXISTS ${_boost_rep})
    file(REMOVE ${_boost_rep})
endif()
if(EXISTS ${_test_rep})
    file(REMOVE ${_test_rep})
endif()


# ---------------------------------------------------------------------------
# Set environment variables

set(ENV{CHECK_SIGNATURE} "${NCBITEST_SIGNATURE}")

#  -- Set bin/lib paths before $PATH
 
file(TO_NATIVE_PATH "${NCBITEST_BINDIR}" _cfg_bin)
file(TO_NATIVE_PATH "${NCBITEST_LIBDIR}" _cfg_lib)
set(ENV{CFG_BIN} "${_cfg_bin}")
set(ENV{CFG_LIB} "${_cfg_lib}")


# -- PATH

if(WIN32)
    set(ENV{PATH} "${NCBITEST_SCRIPTDIR}/common/impl;${NCBITEST_CHECK_SCRIPTDIR};${_cfg_bin};${_cfg_lib};$ENV{PATH}")
    if($ENV{NCBI_AUTOMATED_BUILD})
        set(ENV{PATH} "//snowman/win-coremake/Scripts/bin;$ENV{PATH}")
    endif()
else()
    set(ENV{PATH} "${NCBITEST_SCRIPTDIR}/common/impl:${NCBITEST_CHECK_SCRIPTDIR}:.:${_cfg_bin}:${_cfg_lib}:$ENV{PATH}")
    if($ENV{NCBI_AUTOMATED_BUILD})
        # Utility and $HOME/bin (for Ubuntu).
        set(ENV{PATH} "$ENV{NCBI}/bin/_production/CPPCORE:$ENV{HOME}/bin:$ENV{PATH}")
    endif()
endif()


# -- NCBI_TEST_DATA_PATH

if(WIN32)
    set(ENV{NCBI_TEST_DATA_PATH} "//snowman/win-coremake/Scripts/test_data")
else()
    set(ENV{NCBI_TEST_DATA_PATH} "/am/ncbiapdata/test_data")
endif()
set(ENV{NCBI_TEST_DATA} "$ENV{NCBI_TEST_DATA_PATH}")


# -- Misc 

set(ENV{NCBI_APPLOG_SITE} "testcxx")
set(ENV{BOOST_TEST_CATCH_SYSTEM_ERRORS} "no")
set(ENV{NCBI_CONFIG__LOG__FILE} "-")

if("$ENV{DIAG_OLD_POST_FORMAT}" STREQUAL "")
    set(ENV{DIAG_OLD_POST_FORMAT} "false")
endif()

if (DEFINED NCBITEST_LINKERD_BACKUP)
    set(ENV{NCBI_CONFIG__ID2SNP__PTIS_NAME "${NCBITEST_LINKERD_BACKUP}")
endif()


# -- DYLD_BIND_AT_LAUNCH / MALLOC_DEBUG_

set(ENV{DYLD_BIND_AT_LAUNCH} "1")
if(MaxDebug IN_LIST NCBITEST_PROJECT_FEATURES)
    if(Linux IN_LIST NCBITEST_REQUIRES)
        set(ENV{MALLOC_DEBUG_} 2)
    endif()
endif()

# -- Add library directory to runpath

if(UNIX OR APPLE)
    if(DEFINED $ENV{LD_LIBRARY_PATH})
        set(ENV{LD_LIBRARY_PATH} "${NCBITEST_LIBDIR}:$ENV{LD_LIBRARY_PATH}")
    else()
        set(ENV{LD_LIBRARY_PATH} "${NCBITEST_LIBDIR}")
    endif()
    if (${NCBITEST_SYSTEM} MATCHES "Darwin")
        set(ENV{DYLD_LIBRARY_PATH} "$ENV{LD_LIBRARY_PATH}")
    endif()
    # TODO:
    # Add additional path for imported projects to point to local /lib first
endif()

# -- Timeout

set(_timeout_mult 1)
if(NOT "$ENV{NCBI_CHECK_TIMEOUT_MULT}" STREQUAL "")
    set(_timeout_mult $ENV{NCBI_CHECK_TIMEOUT_MULT})
else()
    if(MaxDebug IN_LIST NCBITEST_PROJECT_FEATURES)
        if("${NCBITEST_COMPILER}" STREQUAL "GCC" OR "${NCBITEST_COMPILER}" STREQUAL "ICC")
            set(_timeout_mult 20)
        endif()
    endif()
endif()
math(EXPR NCBITEST_TIMEOUT "${NCBITEST_TIMEOUT} * ${_timeout_mult}")
set(ENV{CHECK_TIMEOUT} "${NCBITEST_TIMEOUT}")


# -- Debug tools to get stack/back trace (except running under memory checkers)

if(DEFINED NCBITEST_BACK_TRACE)
    set(ENV{NCBI_CHECK_BACK_TRACE} "gdb --batch --quiet -ex \"thread apply all bt\" -ex \"quit\"")
endif()
if(DEFINED NCBITEST_STACK_TRACE)
    set(ENV{NCBI_CHECK_STACK_TRACE} "gstack")
endif()


# -- Generate PHID/SID for a test

if($ENV{NCBI_AUTOMATED_BUILD})
    set(ENV{NCBI_LOG_HIT_ID} "")
    set(ENV{NCBI_LOG_SESSION_ID} "")
    find_program(NCBI_APPLOG NAMES ncbi_applog ncbi_applog.bat PATHS ENV PATH)
    if(NCBI_APPLOG)
        execute_process(
            COMMAND          ${NCBI_APPLOG} generate -phid -sid 
            OUTPUT_VARIABLE  NCBI_APPLOG_output
            RESULT_VARIABLE  _result
        )
        if("${_result}" EQUAL "0")
            string(REPLACE "\n" ";" NCBI_APPLOG_output "${NCBI_APPLOG_output}")
            list(GET NCBI_APPLOG_output 0 NCBI_LOG_HIT_ID)
            set(ENV{NCBI_LOG_HIT_ID} "${NCBI_LOG_HIT_ID}.1")
            file(WRITE ${_workdir}/${NCBI_LOG_HIT_ID} "0")
            list(GET NCBI_APPLOG_output 1 NCBI_LOG_SESSION_ID)
            set(ENV{NCBI_LOG_SESSION_ID} "${NCBI_LOG_SESSION_ID}")
        endif()
    endif()
    set(ENV{CHECK_EXEC} "${NCBITEST_CHECK_SCRIPTDIR}/check_exec_test.sh")
    set(ENV{CHECK_EXEC_STDIN} "${NCBITEST_CHECK_SCRIPTDIR}/check_exec_test.sh -stdin")
    math(EXPR NCBITEST_TIMEOUT "${NCBITEST_TIMEOUT} + 60")
    set(ENV{DIAG_OLD_POST_FORMAT}    "FALSE")
    set(ENV{NCBI_BOOST_REPORT_FILE}  "${_boost_rep}")
else()
    set(ENV{CHECK_EXEC} " ")
endif()


# ---------------------------------------------------------------------------
# Compose test arguments, create wrappers

string(REPLACE  ";" " " _test_args "${NCBITEST_ARGS}")
string(REPLACE  ";" " " _test_cmd  "${NCBITEST_COMMAND}")
if(WIN32)
    set(ENV{DIAG_SILENT_ABORT} "Y")
    if(NOT "${NCBITEST_CYGWIN}" STREQUAL "")
        file(TO_NATIVE_PATH "${NCBITEST_CYGWIN}" NCBITEST_CYGWIN)
        set(ENV{PATH}    "${NCBITEST_CYGWIN};.;$ENV{PATH}")
        string(REPLACE  ";" " " NCBITEST_ARGS  "${NCBITEST_ARGS}")
        if($ENV{NCBI_AUTOMATED_BUILD})
            set(NCBITEST_COMMAND "@echo off\r\nsh -c 'set -o igncr\;export SHELLOPTS\;time -p ${NCBITEST_CHECK_SCRIPTDIR}/check_exec.sh ${NCBITEST_COMMAND} ${NCBITEST_ARGS}'")
        else()
            set(NCBITEST_COMMAND "@echo off\r\nsh -c 'set -o igncr\;export SHELLOPTS\;${NCBITEST_COMMAND} ${NCBITEST_ARGS}'")
        endif()
        string(RANDOM _wrapper)
        file(WRITE ${_workdir}/${_wrapper}.bat ${NCBITEST_COMMAND})
        set(NCBITEST_COMMAND ${_wrapper}.bat)
        set(NCBITEST_ARGS "")
    endif()
else()
    if($ENV{NCBI_AUTOMATED_BUILD})
        if(OFF)
            list(INSERT NCBITEST_ARGS 0 -p ${NCBITEST_CHECK_SCRIPTDIR}/check_exec.sh ${NCBITEST_COMMAND})
            set(NCBITEST_COMMAND "time")
        else()
            string(REPLACE  ";" " " NCBITEST_ARGS  "${NCBITEST_ARGS}")
            set(NCBITEST_COMMAND "#!/bin/sh\nexec time -p ${NCBITEST_CHECK_SCRIPTDIR}/check_exec.sh ${NCBITEST_COMMAND} ${NCBITEST_ARGS}")
            string(RANDOM _wrapper)
            file(WRITE ${_workdir}/../${_wrapper} ${NCBITEST_COMMAND})
            file(COPY ${_workdir}/../${_wrapper} DESTINATION ${_workdir} FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ)
            file(REMOVE ${_workdir}/../${_wrapper})
            set(NCBITEST_COMMAND ${_wrapper})
            set(NCBITEST_ARGS "")
        endif()
    endif()
endif()


# ---------------------------------------------------------------------------
# Run command

set(_result "1")
string(TIMESTAMP _test_start "%m/%d/%Y %H:%M:%S")
string(TIMESTAMP _time_start "%s")
execute_process(
    COMMAND           ${NCBITEST_COMMAND} ${NCBITEST_ARGS}
    WORKING_DIRECTORY ${_workdir}
    TIMEOUT           ${NCBITEST_TIMEOUT}
    RESULT_VARIABLE   _result
    OUTPUT_FILE       ${_tmp_out}
    ERROR_FILE        ${_tmp_out}
)
string(TIMESTAMP _time_stop "%s")
string(TIMESTAMP _test_stop "%m/%d/%Y %H:%M:%S")
math(EXPR _test_times "${_time_stop} - ${_time_start}")
set(_test_times "real ${_test_times}")


# ---------------------------------------------------------------------------
# Add header/footer into output

file(READ ${_tmp_out} _contents)
set(_info)
string(APPEND _info "======================================================================\n")
string(APPEND _info "${NCBITEST_NAME}\n")
string(APPEND _info "======================================================================\n")
string(APPEND _info "\nCommand line: ${_test_cmd} ${_test_args}\n\n")
string(APPEND _info ${_contents})
string(APPEND _info "Start time   : ${_test_start}\n")
string(APPEND _info "Stop time    : ${_test_stop}\n")
#string(APPEND _info "Load averages: unavailable\n")
string(APPEND _info "\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n@@@ EXIT CODE: ${_result}\n")
file(WRITE ${_test_out} ${_info})


# ---------------------------------------------------------------------------
# Generate test report

if($ENV{NCBI_AUTOMATED_BUILD})
    set(_info)
if(OFF)
    cmake_host_system_information(RESULT _name QUERY OS_NAME)
else()
    if(APPLE)
        set(_name "Darwin")
    elseif(UNIX)
        set(_name "Linux")
    elseif(WIN32)
        set(_name "Windows")
    else()
        set(_name "Unknown")
    endif()
endif()
    string(APPEND _info "${NCBITEST_SIGNATURE} ${_name}\n")
    string(APPEND _info "${NCBITEST_XOUTDIR}\n")
    string(APPEND _info "${_test_cmd} ${_test_args}\n")
    string(APPEND _info "${NCBITEST_ALIAS}\n")

    if (${_result} EQUAL "0")
        set(_status "OK")
    else()
        set(_status "ERR")
    endif()
    string(FIND "${_contents}" "NCBI_UNITTEST_DISABLED" _pos)
    if(${_pos} GREATER_EQUAL 0)
        set(_status "DIS")
    endif()
    string(FIND "${_contents}" "NCBI_UNITTEST_SKIPPED" _pos)
    if(${_pos} GREATER_EQUAL 0)
        set(_status "SKP")
    endif()
    string(FIND "${_contents}" "NCBI_UNITTEST_TIMEOUTS_BUT_NO_ERRORS" _pos)
    if(${_pos} GREATER_EQUAL 0)
        set(_status "TO")
        set(_result "Process terminated due to timeout")
    endif()
    string(FIND "${_contents}" "Maximum execution time of" _pos)
    if(${_pos} GREATER_EQUAL 0)
        set(_status "TO")
        set(_result "Process terminated due to timeout")
    endif()
    string(FIND "${_result}" "timeout" _pos)
    if(${_pos} GREATER_EQUAL 0)
        set(_status "TO")
    endif()
    string(FIND "${_contents}" "real " _pos REVERSE)
    if(${_pos} GREATER_EQUAL 0)
        string(SUBSTRING "${_contents}" ${_pos} -1 _times)
        string(STRIP "${_times}" _times)
        string(REPLACE "\n" ", " _times  "${_times}")
        if("${_times}" MATCHES "^real [0-9].* user [0-9].* sys [0-9]")
            set(_test_times "${_times}")
        endif()
    endif()

    string(APPEND _info "${_status}\n")
    string(APPEND _info "${_test_start}\n")
    string(APPEND _info "${_result}\n")
    string(APPEND _info "${_test_times}\n")
    string(APPEND _info "${NCBITEST_WATCHER}\n")
    string(APPEND _info "unavailable\n")
    string(TIMESTAMP _test_stop "%y%m%d%H%M%S")
    if(EXISTS ${_workdir}/check_exec.pid)
        file(STRINGS ${_workdir}/check_exec.pid _id)
        list(GET _id 0 _id)
    else()
        string(RANDOM ALPHABET "0123456789" _id)
    endif()
    cmake_host_system_information(RESULT _name  QUERY HOSTNAME)
    string(APPEND _info "${_test_stop}-${_id}-${_name}\n")
    file(WRITE ${_test_rep} ${_info})
endif()


# ---------------------------------------------------------------------------
# Cleanup

file(REMOVE ${_tmp_out})
file(REMOVE_RECURSE ${_workdir})
file(REMOVE ${_tmp_out})


# ---------------------------------------------------------------------------

if (NOT ${_result} EQUAL "0")
    message(SEND_ERROR "Test ${NCBITEST_NAME} failed (error=${_result})")
endif()
