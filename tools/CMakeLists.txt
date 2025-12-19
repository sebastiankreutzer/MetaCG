add_subdirectory(cgcollector2)
add_subdirectory(cgmerge2)
add_subdirectory(cgconvert)
add_subdirectory(cgformat)
add_subdirectory(cgdiff)

if(${LLVM_PACKAGE_VERSION}
        GREATER
        14
)
    add_subdirectory(cage)
else()
    message(STATUS "Skipping CaGe: requires LLVM version 15 or higher")
endif()

