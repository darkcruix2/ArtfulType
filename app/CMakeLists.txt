# HOW TO BUILD USING CMAKE as a standalone project:
# mkdir build
# cd build
# cmake .. -DCMAKE_TOOLCHAIN_FILE=$HOME/dev/retro68/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake
# make

cmake_minimum_required(VERSION 3.10)
project(ArtfulType C)

# Explicitly include system headers so clangd picks them up in compile_commands.json
include_directories(SYSTEM "/opt/Retro68-build/toolchain/m68k-apple-macos/include")
include_directories(SYSTEM "/opt/Retro68-build/toolchain/multiversal/CIncludes")
include_directories(SYSTEM "/opt/Retro68-build/toolchain/lib/gcc/m68k-apple-macos/16.1.0/include")
include_directories(SYSTEM "/opt/Retro68-build/toolchain/lib/gcc/m68k-apple-macos/16.1.0/include-fixed")
include_directories(waste)

add_application(ArtfulType
    main.c
    scrolling.c
    markdown.c
    undo.c
    zoom.c
    file.c
    splash.c
    waste/WASTE.c
    main.r
    CREATOR "ArtT"
    )

set_target_properties(ArtfulType PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(ArtfulType PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(ArtfulType PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()

add_application(ArtfulTypePro
    main.c
    scrolling.c
    markdown.c
    undo.c
    zoom.c
    file.c
    splash.c
    waste/WASTE.c
    main_pro.r
    CREATOR "ArtT"
    )

target_compile_definitions(ArtfulTypePro PRIVATE ARTFUL_PRO)

set_target_properties(ArtfulTypePro PROPERTIES COMPILE_OPTIONS -ffunction-sections)
if(CMAKE_SYSTEM_NAME MATCHES Retro68)
    set_target_properties(ArtfulTypePro PROPERTIES LINK_FLAGS "-Wl,-gc-sections -Wl,--mac-strip-macsbug")
else()
    set_target_properties(ArtfulTypePro PROPERTIES LINK_FLAGS "-Wl,-gc-sections")
endif()
