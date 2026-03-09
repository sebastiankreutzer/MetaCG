option(
  CAGE_USE_METAVIRT
  ON
  "Rely on metavirt for virtual call handling"
)

if(${CAGE_USE_METAVIRT})
  include(metavirt)
endif()

add_subdirectory(src)
add_subdirectory(test)

install(TARGETS cage cage-plugin  cage_metadata
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
        DIRECTORY include/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
        PATTERN "*.h"
)
