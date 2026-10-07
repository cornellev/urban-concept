# fail the build when a firmware image links the heap allocator
execute_process(COMMAND ${NM} -C ${ELF} OUTPUT_VARIABLE symbols COMMAND_ERROR_IS_FATAL ANY)
string(REGEX MATCHALL " (malloc|calloc|realloc|free|_sbrk|operator new|operator delete)[\n(]" hits "${symbols}")
if(hits)
    list(TRANSFORM hits REPLACE "^ (.*).$" "\\1")
    list(JOIN hits ", " hits)
    message(FATAL_ERROR "${ELF} links the heap: ${hits}")
endif()
