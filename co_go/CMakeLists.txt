include(GenerateExportHeader)

add_library(co_go INTERFACE continuation.hpp "resource.hpp")
add_library(cogoproject::co_go ALIAS co_go)

target_link_libraries(co_go INTERFACE cogoproject_options cogoproject_warnings)

target_include_directories(co_go ${WARNING_GUARD} INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)

target_compile_features(co_go INTERFACE cxx_std_23)

set_target_properties(
  co_go
  PROPERTIES VERSION ${PROJECT_VERSION}
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES)

# generate_export_header(co_go EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/co_go/co_go_export.hpp)

# if(NOT BUILD_SHARED_LIBS)
#  target_compile_definitions(co_go PUBLIC CO_GO_STATIC_DEFINE)
# endif()
