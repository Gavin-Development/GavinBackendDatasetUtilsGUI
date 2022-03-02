cmake_minimum_required(VERSION 3.21)
project(GavinBackendDatasetUtilsGUI)

set(CMAKE_CXX_STANDARD 17)
find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development NumPy)

configure_file(CMakeLists.txt.in imgui-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/imgui-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/imgui-download)

configure_file(CMakeLists.txt.in GavinBackendDatasetUtils-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/GavinBackendDatasetUtils-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/GavinBackendDatasetUtils-download)

configure_file(CMakeLists.txt.in pybind11-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pybind11-download)
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pybind11-download)

# GLFW
find_package(glfw3 REQUIRED)
include_directories(${GLFW_INCLUDE_DIRS})
link_libraries(${GLFW_LIBRARY_DIRS})

# opengl
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})

# glew
find_package(GLEW REQUIRED)
include_directories(${GLEW_INCLUDE_DIRS})

# pybind11
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/pybind11-src
        ${CMAKE_CURRENT_BINARY_DIR}/pybind11-build
        EXCLUDE_FROM_ALL)


# Find ImGUI files
file(GLOB IMGUI_STANDARD_SRC_FILES ${CMAKE_CURRENT_BINARY_DIR}/imgui-src/*.cpp ${CMAKE_CURRENT_BINARY_DIR}/imgui-src/*.h)

# Find Gavin Files
file(GLOB GAVIN_BACKEND_SRC ${CMAKE_CURRENT_BINARY_DIR}/GavinBackendDatasetUtils-src/GavinBackendDatasetUtils/*.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/GavinBackendDatasetUtils-src/GavinBackendDatasetUtils/*.hpp)

# ImGUI Addons
file(GLOB IMGUI_ADDONS_SRC ${CMAKE_CURRENT_BINARY_DIR}/imgui-addons-src/FileBrowser/*.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/imgui-addons-src/FileBrowser/*.h)


# Build ImGUI Lib
add_library(imgui STATIC
        ${CMAKE_CURRENT_BINARY_DIR}/imgui-src/backends/imgui_impl_glfw.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/imgui-src/backends/imgui_impl_glfw.h
        ${CMAKE_CURRENT_BINARY_DIR}/imgui-src/backends/imgui_impl_opengl3.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/imgui-src/backends/imgui_impl_opengl3.h
        ${IMGUI_STANDARD_SRC_FILES}
        ${IMGUI_ADDONS_SRC}
        )

# Build Gavin Lib
# pybind11_add_module(GavinBackendUtils MODULE ${GAVIN_BACKEND_SRC})

# Include Src Files
include_directories(${CMAKE_CURRENT_BINARY_DIR}/imgui-src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/GavinBackendDatasetUtils-src/GavinBackendDatasetUtils/)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/pybind11-src/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/imgui-addons-src/FileBrowser/)
include_directories(${Python_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} main.cpp ${GAVIN_BACKEND_SRC})
target_link_libraries(${PROJECT_NAME} imgui)
target_link_libraries(
        ${PROJECT_NAME}
        glfw
        ${OPENGL_LIBRARIES}
        ${GLEW_LIBRARIES}
        ${EXTRA_LIBS}
        ${PYTHON_LIBRARIES}
        pybind11::embed
        ${CMAKE_DL_LIBS}
)
