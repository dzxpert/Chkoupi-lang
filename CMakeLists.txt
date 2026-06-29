cmake_minimum_required(VERSION 3.20)
project(chkoupi CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Static runtime: no VCRUNTIME/MSVCP DLL dependency for end users ────────────
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ── Find LLVM ──────────────────────────────────────────────────
if(EXISTS "D:/LLVM20/lib/cmake/llvm")
    set(LLVM_DIR "D:/LLVM20/lib/cmake/llvm" CACHE PATH "Path to LLVM cmake config")
else()
    set(LLVM_DIR "D:/LLVM/lib/cmake/llvm" CACHE PATH "Path to LLVM cmake config")
endif()
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── Sources ────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Sema.cpp
    src/Codegen.cpp
)

add_executable(chkoupi ${SOURCES})

# ── Link LLVM ──────────────────────────────────────────────────
llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    irreader
    nativecodegen
    x86codegen
    x86asmparser
    x86info
    orcjit
    executionengine
    mcjit
)

target_link_libraries(chkoupi PRIVATE ${LLVM_LIBS})
target_include_directories(chkoupi PRIVATE ${CMAKE_SOURCE_DIR}/include)

# ── VS 2022 DIA SDK (fixes diaguids.lib path baked into LLVM built w/ VS2019) ─
set(VS2022_DIA "C:/Program Files/Microsoft Visual Studio/2022/Community/DIA SDK/lib/amd64")
if(EXISTS "${VS2022_DIA}/diaguids.lib")
    target_link_directories(chkoupi PRIVATE "${VS2022_DIA}")
    message(STATUS "Using VS2022 DIA SDK: ${VS2022_DIA}")
else()
    message(WARNING "VS2022 DIA SDK not found at: ${VS2022_DIA}")
endif()

# ── VS Code Extension Installer ────────────────────────────────
add_executable(chkoupi-vscode-installer editors/vscode/installer/main.cpp)
