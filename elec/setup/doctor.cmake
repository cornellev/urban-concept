# cmake script to check for required build tools and their versions
# we use cmake because its the only guaranteed cross-platform scripting tool

# pinned versions; match the flake (arm-gcc), cmake/pinned_tools.cmake, and the pico-sdk submodule (picotool)
# update only on a deliberate toolchain bump
set(ARM_GCC_EXPECTED "15.3")
set(PICOTOOL_EXPECTED "2.3")

set(problems 0)

macro(report label names verarg expected)
    unset(exe CACHE)
    # extra args are the only folders to search, so doctor checks the copy the build uses
    if(${ARGC} GREATER 4)
        find_program(exe NAMES ${names} PATHS ${ARGN} NO_DEFAULT_PATH)
    else()
        find_program(exe NAMES ${names})
    endif()
    if(NOT exe)
        message("  MISSING   ${label}  not installed")
        math(EXPR problems "${problems} + 1")
    else()
        execute_process(
            COMMAND "${exe}" ${verarg}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE out
            ERROR_VARIABLE out
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" ver "${out}")
        if(NOT result EQUAL 0)
            message("  BROKEN    ${label}  ${exe} fails to run")
            math(EXPR problems "${problems} + 1")
        elseif(NOT ver)
            message("  ok        ${label}  installed (version unknown)")
        elseif("${expected}" STREQUAL "")
            message("  ok        ${label}  ${ver}")
        else()
            # whole-segment match: 15.3 accepts 15.3.x but not 15.30
            string(REPLACE "." "\\." exp_re "${expected}")
            if(ver MATCHES "^${exp_re}(\\.|$)")
                message("  ok        ${label}  ${ver}")
            else()
                message("  MISMATCH  ${label}  ${ver}  (expected ${expected})")
                math(EXPR problems "${problems} + 1")
            endif()
        endif()
    endif()
endmacro()

set(tools_dir "${CMAKE_CURRENT_LIST_DIR}/../.tools")
set(pending 0)

# look where the build looks: the copies it downloads, or with ELEC_SYSTEM_TOOLS set,
# PICO_TOOLCHAIN_PATH and then PATH
# a windows path's backslashes would be read as escapes inside the macro
if(NOT DEFINED ENV{ELEC_SYSTEM_TOOLS})
    set(arm_dirs "${tools_dir}/arm-gnu-toolchain/bin")
    set(picotool_dirs "${tools_dir}/picotool")
elseif(DEFINED ENV{PICO_TOOLCHAIN_PATH})
    file(TO_CMAKE_PATH "$ENV{PICO_TOOLCHAIN_PATH}/bin" arm_dirs)
endif()

message("checking build tools:")
if(NOT DEFINED ENV{ELEC_SYSTEM_TOOLS} AND NOT EXISTS "${tools_dir}/arm-gnu-toolchain/.pinned")
    message("  later     arm-gcc   downloaded by the first build")
    set(pending 1)
else()
    report("arm-gcc " arm-none-eabi-gcc -dumpversion "${ARM_GCC_EXPECTED}" ${arm_dirs})
endif()
report("cmake   " cmake --version "")
report("ninja   " ninja --version "")
if(NOT DEFINED ENV{ELEC_SYSTEM_TOOLS} AND NOT EXISTS "${tools_dir}/picotool/.pinned")
    message("  later     picotool  downloaded by the first build")
    set(pending 1)
else()
    report("picotool" picotool version "${PICOTOOL_EXPECTED}" ${picotool_dirs})
endif()
# the same lookup pico-sdk's build uses
find_package(Python3 COMPONENTS Interpreter QUIET)
report("python  " "${Python3_EXECUTABLE}" --version "")
message("")

if(problems EQUAL 0 AND pending)
    message("no problems. the first build downloads the arm compiler and picotool")
elseif(problems EQUAL 0)
    message("all build tools present with correct versions")
else()
    message(FATAL_ERROR "${problems} problem(s) above")
endif()
