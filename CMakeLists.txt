cmake_minimum_required(VERSION 3.24)
project(ModelQuickPreview LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(assimp CONFIG REQUIRED)

add_executable(ModelQuickPreview WIN32 src/ModelViewer.cpp src/ModelViewer.rc)
target_link_libraries(ModelQuickPreview PRIVATE assimp::assimp opengl32 comdlg32 shell32)
target_compile_definitions(ModelQuickPreview PRIVATE UNICODE _UNICODE NOMINMAX)

add_library(ModelThumbnail SHARED src/ModelThumbnail.cpp src/ModelThumbnail.def)
target_link_libraries(ModelThumbnail PRIVATE ole32 gdi32 user32 advapi32 shell32 uuid)
target_compile_definitions(ModelThumbnail PRIVATE UNICODE _UNICODE NOMINMAX)
set_target_properties(ModelThumbnail PROPERTIES PREFIX "" OUTPUT_NAME "ModelThumbnail")

install(TARGETS ModelQuickPreview ModelThumbnail RUNTIME DESTINATION . LIBRARY DESTINATION .)

