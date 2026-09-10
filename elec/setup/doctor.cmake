# cmake script to check for required build tools and their versions
# we use cmake because its the only guaranteed cross-platform scripting tool

set(problems 0)

macro(report label names verarg expected)
    unset(exe CACHE)
    # extra args are find hints (bundled windows picotool)
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

message("checking build tools:")
report("arm-gcc " arm-none-eabi-gcc -dumpversion "${ARM_GCC_EXPECTED}")
report("cmake   " cmake --version "")
report("ninja   " ninja --version "")
report("picotool" picotool version "${PICOTOOL_EXPECTED}" "${TOOLS_DIR}/picotool")
message("")

if(problems EQUAL 0)
    message("all build tools present with correct versions")
else()
    message("${problems} problem(s) above")
endif()
