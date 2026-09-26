# cmake script to check for required build tools and their versions
# we use cmake because its the only guaranteed cross-platform scripting tool

# pinned versions; match the flake (arm-gcc) and the pico-sdk submodule (picotool)
# update only on a deliberate toolchain bump
set(ARM_GCC_EXPECTED "15.3")
set(PICOTOOL_EXPECTED "2.3")

set(problems 0)

macro(report label names verarg expected)
    unset(exe CACHE)
    # extra args are find hints (bundled windows tools)
    find_program(exe NAMES ${names} HINTS ${ARGN})
    if(NOT exe)
        message("  MISSING   ${label}  not installed")
        math(EXPR problems "${problems} + 1")
    else()
        execute_process(
            COMMAND "${exe}" ${verarg}
            OUTPUT_VARIABLE out
            ERROR_VARIABLE out
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" ver "${out}")
        if(NOT ver)
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

# a windows path's backslashes would be read as escapes inside the macro
file(TO_CMAKE_PATH "$ENV{PICO_TOOLCHAIN_PATH}" toolchain_path)

message("checking build tools:")
report("arm-gcc " arm-none-eabi-gcc -dumpversion "${ARM_GCC_EXPECTED}" "${toolchain_path}/bin")
report("cmake   " cmake --version "")
report("ninja   " ninja --version "")
report("picotool" picotool version "${PICOTOOL_EXPECTED}" "${CMAKE_CURRENT_LIST_DIR}/../.tools/picotool")
# the same lookup pico-sdk's build uses
find_package(Python3 COMPONENTS Interpreter QUIET)
report("python  " "${Python3_EXECUTABLE}" --version "")
message("")

if(problems EQUAL 0)
    message("all build tools present with correct versions")
else()
    message(FATAL_ERROR "${problems} problem(s) above")
endif()
