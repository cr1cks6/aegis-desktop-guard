cmake_minimum_required(VERSION 3.24)

project(AegisDesktopGuard VERSION 0.1.0 LANGUAGES CXX)

add_executable(AegisDesktopGuard WIN32
    src/main.cpp
)

target_compile_features(AegisDesktopGuard PRIVATE cxx_std_20)

target_compile_definitions(AegisDesktopGuard PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(AegisDesktopGuard PRIVATE /W4 /permissive- /utf-8)
endif()

target_link_libraries(AegisDesktopGuard PRIVATE
    user32
    shell32
    gdi32
)

install(TARGETS AegisDesktopGuard RUNTIME DESTINATION bin)