cmake_minimum_required(VERSION 3.24)

project(AegisDesktopGuardTask2 VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =========================================================
# RPC generation
# =========================================================

set(RPC_IDL_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/AegisRpc.idl
)

set(RPC_GENERATED_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/generated/rpc
)

set(RPC_GENERATED_HEADER
    ${RPC_GENERATED_DIR}/AegisRpc.h
)

set(RPC_CLIENT_STUB
    ${RPC_GENERATED_DIR}/AegisRpc_c.c
)

set(RPC_SERVER_STUB
    ${RPC_GENERATED_DIR}/AegisRpc_s.c
)

file(MAKE_DIRECTORY ${RPC_GENERATED_DIR})

add_custom_command(
    OUTPUT
        ${RPC_GENERATED_HEADER}
        ${RPC_CLIENT_STUB}
        ${RPC_SERVER_STUB}

    COMMAND midl
        /nologo
        /app_config
        /env x64
        /h AegisRpc.h
        /cstub AegisRpc_c.c
        /sstub AegisRpc_s.c
        /out ${RPC_GENERATED_DIR}
        ${RPC_IDL_FILE}

    DEPENDS
        ${RPC_IDL_FILE}

    COMMENT "Generating Windows RPC files"
)

add_custom_target(GenerateAegisRpc
    DEPENDS
        ${RPC_GENERATED_HEADER}
        ${RPC_CLIENT_STUB}
        ${RPC_SERVER_STUB}
)

# =========================================================
# Common library
# =========================================================

add_library(AegisCommon STATIC
    src/common/WinError.cpp
    src/common/WinError.h
    src/common/Constants.h
)

target_include_directories(AegisCommon PUBLIC
    src/common
)

target_compile_definitions(AegisCommon PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# =========================================================
# RPC client library
# =========================================================

add_library(AegisRpcClient STATIC
    ${RPC_CLIENT_STUB}
    ${RPC_GENERATED_HEADER}
)

add_dependencies(AegisRpcClient GenerateAegisRpc)

target_include_directories(AegisRpcClient PUBLIC
    ${RPC_GENERATED_DIR}
)

target_link_libraries(AegisRpcClient PUBLIC
    Rpcrt4
)

# =========================================================
# RPC server library
# =========================================================

add_library(AegisRpcServer STATIC
    ${RPC_SERVER_STUB}
    ${RPC_GENERATED_HEADER}
)

add_dependencies(AegisRpcServer GenerateAegisRpc)

target_include_directories(AegisRpcServer PUBLIC
    ${RPC_GENERATED_DIR}
)

target_link_libraries(AegisRpcServer PUBLIC
    Rpcrt4
)

# =========================================================
# GUI application
# =========================================================

add_executable(AegisDesktopGuard WIN32
    src/gui/main.cpp
)

add_dependencies(AegisDesktopGuard GenerateAegisRpc)

target_include_directories(AegisDesktopGuard PRIVATE
    src/common
    ${RPC_GENERATED_DIR}
)

target_link_libraries(AegisDesktopGuard PRIVATE
    AegisCommon
    AegisRpcClient

    user32
    shell32
    advapi32
    wtsapi32
    Rpcrt4
)

# =========================================================
# Windows service
# =========================================================

add_executable(AegisDesktopGuardService
    src/service/main.cpp
)

add_dependencies(AegisDesktopGuardService GenerateAegisRpc)

target_include_directories(AegisDesktopGuardService PRIVATE
    src/common
    ${RPC_GENERATED_DIR}
)

target_link_libraries(AegisDesktopGuardService PRIVATE
    AegisCommon
    AegisRpcServer

    advapi32
    userenv
    wtsapi32
    shell32
    Rpcrt4
)

# =========================================================
# Global compiler settings
# =========================================================

foreach(target
    AegisCommon
    AegisRpcClient
    AegisRpcServer
    AegisDesktopGuard
    AegisDesktopGuardService
)

    target_compile_features(${target} PRIVATE cxx_std_20)

    target_compile_definitions(${target} PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /utf-8
        )
    endif()

endforeach()

# =========================================================
# Install
# =========================================================

install(TARGETS
    AegisDesktopGuard
    AegisDesktopGuardService
    RUNTIME DESTINATION bin
)