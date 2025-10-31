include(GenerateExportHeader)


add_library(co_go co_go.cpp co_go.hpp)

add_library(myproject::co_go ALIAS co_go)

target_link_libraries(co_go PRIVATE myproject_options myproject_warnings)

target_include_directories(co_go ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/co_go>
                                                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>/co_go)

target_compile_features(co_go PUBLIC cxx_std_20)

set_target_properties(
  co_go
  PROPERTIES VERSION ${PROJECT_VERSION}
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN YES)

generate_export_header(co_go EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/co_go/co_go/co_go_export.hpp)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(co_go PUBLIC CO_GO_STATIC_DEFINE)
endif()
