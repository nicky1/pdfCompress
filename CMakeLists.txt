# Windows 下本工程会生成可执行文件 pdf_compress.exe（MSVC 常见路径：build/Release/pdf_compress.exe）。
# CMake 在 Windows 上对 add_executable 的目标名会自动追加 .exe 后缀，无需手写 .exe。
cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
# vcpkg x64-windows-static 使用静态 CRT（/MT）。主程序须同为 /MT。
# 注意：在 project() 之前 MSVC 变量往往尚未置位，不能用 if(MSVC)；本工程仅 Windows。
if(WIN32)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
project(pdf_compress LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
  message(FATAL_ERROR "This project is Windows-only.")
endif()

find_package(qpdf CONFIG REQUIRED)

add_executable(pdf_compress WIN32 src/main.cpp)
if(MSVC)
  set_property(
    TARGET pdf_compress
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
target_link_libraries(
  pdf_compress
  PRIVATE
    qpdf::libqpdf
    comdlg32
    comctl32
    user32
    gdi32
)

target_compile_definitions(
  pdf_compress
  PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
  target_compile_options(pdf_compress PRIVATE /utf-8 /W4 /permissive-)
endif()
