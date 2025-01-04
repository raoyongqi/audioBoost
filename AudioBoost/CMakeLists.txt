# 指定 CMake 最低版本
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(AudioBoost)

# 将源代码添加到此项目的可执行文件
add_executable(AudioBoost "AudioBoost.cpp" "AudioBoost.h" )

# 使用 C++20 标准（如果 CMake 版本大于 3.12）
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET AudioBoost PROPERTY CXX_STANDARD 20)
endif()

# 设置 vcpkg 的工具链文件路径（只有在你使用 vcpkg 时需要）
# 如果你已经通过 vcpkg 安装了 FFmpeg，并且没有手动指定 `CMAKE_TOOLCHAIN_FILE`
# 需要保证 vcpkg 配置正确：
# set(CMAKE_TOOLCHAIN_FILE "C:/path_to_vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Toolchain file")

# 设置 FFmpeg 配置文件的路径
# 如果你通过 vcpkg 安装 FFmpeg，那么 vcpkg 会自动为你设置好路径
# 你不需要手动指定 `CMAKE_MODULE_PATH`，`find_package` 会自动查找安装的包
# 但是如果你需要指定路径，可以设置 `CMAKE_MODULE_PATH`
set(CMAKE_MODULE_PATH "C:/vcpkg/packages/ffmpeg_x64-windows/share/ffmpeg" ${CMAKE_MODULE_PATH})

# 查找 FFmpeg 包
find_package(FFMPEG REQUIRED)

# 如果找到了 FFmpeg，则包括它的头文件和库

# 如果找到了 FFmpeg，则包括它的头文件和库
if (FFMPEG_FOUND)
    target_include_directories(AudioBoost PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(AudioBoost PRIVATE ${FFMPEG_LIBRARIES})
    message("FFmpeg found!")
else()
    message(FATAL_ERROR "FFmpeg not found!")
endif()
