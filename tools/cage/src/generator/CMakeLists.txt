add_library(cage STATIC CallGraphGenerator.cpp FileExporter.cpp CallGraphEmbedder.cpp StaticLinkagePolicy.cpp DynamicLinkagePolicy.cpp CallAnalysis.cpp VCallAnalysis.cpp)
set_target_properties(cage PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(cage PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tools/cage/include>)
target_include_directories(cage PUBLIC $<BUILD_INTERFACE:${LLVM_INCLUDE_DIRS}>)

option(CAGE_USE_DEVIRT_MD "Use devirt metadata to resolve virtual calls" ON)

# if (CAGE_USE_DEVIRT_MD)
#   target_compile_definitions(cage PRIVATE "USE_DEVIRT_MD")
# elseif(${CAGE_USE_METAVIRT})
#   target_link_libraries(cage PRIVATE metavirt::Types)
#   target_compile_definitions(cage PRIVATE "HAVE_METAVIRT")
# endif()


if(${CAGE_USE_METAVIRT})
  target_link_libraries(cage PRIVATE metavirt::Types)
  target_compile_definitions(cage PRIVATE "HAVE_METAVIRT")
endif()

target_link_libraries(cage PUBLIC metacg::metacg)
add_config_include(cage)

# TODO: For prototyping, think about where to put MD eventually
add_library(cage_metadata SHARED CageMD.cpp )
target_include_directories(cage_metadata PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/tools/cage/include>)
add_metacg(cage_metadata)
