cmake_minimum_required(VERSION 3.24)
project(WandouPreview LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(assimp CONFIG REQUIRED)

add_executable(WandouModelPreview WIN32 src/ModelViewer.cpp src/ModelViewer.rc)
target_link_libraries(WandouModelPreview PRIVATE assimp::assimp opengl32 comdlg32 shell32)
target_compile_definitions(WandouModelPreview PRIVATE UNICODE _UNICODE NOMINMAX)

add_library(ModelThumbnail SHARED src/ModelThumbnail.cpp src/ModelThumbnail.def)
target_link_libraries(ModelThumbnail PRIVATE ole32 gdi32 user32 advapi32 shell32 uuid)
target_compile_definitions(ModelThumbnail PRIVATE UNICODE _UNICODE NOMINMAX)
set_target_properties(ModelThumbnail PROPERTIES PREFIX "" OUTPUT_NAME "ModelThumbnail")

add_executable(WandouVideoThumbnail WIN32 src/VideoThumbnail.cpp src/ModelViewer.rc)
target_link_libraries(WandouVideoThumbnail PRIVATE mfplat mfreadwrite mfuuid propsys ole32 shell32 user32)
target_compile_definitions(WandouVideoThumbnail PRIVATE UNICODE _UNICODE NOMINMAX)

install(TARGETS WandouModelPreview WandouVideoThumbnail ModelThumbnail RUNTIME DESTINATION . LIBRARY DESTINATION .)
