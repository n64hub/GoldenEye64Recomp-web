cmake_minimum_required(VERSION 3.20)
project(GoldenRecomp)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# RecompFrontend builds Objective-C++ on Apple (recompui's support_apple.mm).
if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

# Newer Apple clang, and LLVM's clang-cl on Windows, advertise __cpp_consteval but
# reject fmt's consteval format-string checks; disable them so macOS and Windows
# build cleanly (see the guard in lib/fmt/include/fmt/base.h).
if (APPLE OR WIN32)
    add_compile_definitions(FMT_USE_CONSTEVAL=0)
endif()
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()

# Lets ccache cache clang-cl compiles (see the file).
include(${CMAKE_SOURCE_DIR}/cmake/ccache_clang_cl.cmake)

# Opt out of constexpr mutex constructor on windows to prevent vcredist issues
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()

# SDL 2.30's SDL_endian.h defines _m_prefetch, which clang declares as a builtin in prfchwintrin.h
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions(__PRFCHWINTRIN_H)
endif()

# Emscripten: threads (SharedArrayBuffer), wasm exceptions (RmlUi, the runtime), wasm SIMD (hlslpp's backend, the
# auto-vectorizer) and tail calls (recomp.h's RECOMP_TAIL_CALL in the recompiled code) for every target. xxHash's
# NEON path through Emscripten's arm_neon.h doesn't compile in C++.
if (EMSCRIPTEN)
    add_compile_options(-pthread -fwasm-exceptions -msimd128 -mtail-call)
    add_compile_options("-DXXH_HAS_INCLUDE(x)=0")
    add_link_options(-pthread -fwasm-exceptions -mtail-call)
    # ccache when it is on PATH (N64Recomp-browser's frontend/build_speed.cmake).
    include(${CMAKE_SOURCE_DIR}/lib/N64Recomp-browser/frontend/build_speed.cmake)
    # The shaders the page specializes at build time, built with the game waiting: the menus' before it starts, and
    # each level's when the level first draws (src/rt64_shaders.txt says why and how it was recorded).
    set(RT64_PREBUILT_SHADER_LIST ${CMAKE_SOURCE_DIR}/src/rt64_shaders.txt)
endif()

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(RT64_STATIC TRUE)
# RecompFrontend's renderer hands RT64 the SDL window, which RT64 takes on Linux with this (its Vulkan surface comes
# from SDL).
set(RT64_SDL_WINDOW_VULKAN TRUE)
# RT64's bundled zstd adds /clang:-Wall under clang-cl, for warnings only; ccache won't cache a call with it.
# Pre-answering zstd's flag check skips it.
if (CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache")
    set(C_FLAG_CLANG_WALL OFF CACHE INTERNAL "")
    set(CXX_FLAG_CLANG_WALL OFF CACHE INTERNAL "")
endif()

# lib/rt64, lib/N64ModernRuntime and lib/RecompFrontend are upstream's, at the commits N64Recomp-browser's web forks
# are built on: this game's own changes to them (lib-patches/, what podulator's copies of them change), then
# N64Recomp-browser's patches/, if they aren't in already. The desktop build needs them too: the patched N64Recomp
# reads GoldenEye's jump tables from their own section and renames its strtol and the rest in the patches' calls, and
# the code it writes uses the patched recomp.h (RECOMP_TAIL_CALL).
include(${CMAKE_SOURCE_DIR}/lib-patches/apply.cmake)
goldeneye_apply_lib_patches(${CMAKE_SOURCE_DIR}/lib)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/rt64 ${CMAKE_BINARY_DIR}/rt64)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)

# The page: N64Recomp-browser's frontend/ (the page shell, the file picker, the cross-origin service worker)
if (EMSCRIPTEN)
    add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64Recomp-browser/frontend ${CMAKE_BINARY_DIR}/web_frontend)
    target_compile_options(fmt PRIVATE -include cstdlib)
endif()

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# RecompiledFuncs - Library containing the primary recompiler output
add_library(RecompiledFuncs STATIC)

target_compile_options(RecompiledFuncs PRIVATE
    # -Wno-unused-but-set-variable
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
)

target_include_directories(RecompiledFuncs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)

option(LIVE_GAMECODE "Ship no recompiled game code; generate it at runtime from the user's ROM (clean distributable builds)" OFF)

option(GE_HYBRID_DEBUG "Debug: also build static funcs as a dlopen-able library for live-vs-static bisection" OFF)

if (LIVE_GAMECODE)
    target_sources(RecompiledFuncs PRIVATE
        ${CMAKE_SOURCE_DIR}/LiveGamecode/forwarders.c
        ${CMAKE_SOURCE_DIR}/LiveGamecode/lookup.cpp
    )
    if (GE_HYBRID_DEBUG)
        file(GLOB HYBRID_C_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
        add_library(StaticFuncsDL SHARED ${HYBRID_C_SOURCES})
        target_compile_options(StaticFuncsDL PRIVATE -fno-strict-aliasing -Wno-unused-variable -Wno-implicit-function-declaration)
        target_include_directories(StaticFuncsDL PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
            ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
            ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
        )
        set_target_properties(StaticFuncsDL PROPERTIES LINK_FLAGS "-Wl,--allow-shlib-undefined")
    endif()
else()
    file(GLOB FUNC_C_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.c)
    file(GLOB FUNC_CXX_SOURCES ${CMAKE_SOURCE_DIR}/RecompiledFuncs/*.cpp)
    target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES} ${FUNC_CXX_SOURCES})
endif()

# wasm-ld turns a call through a wrong implicit declaration into a trap, so on the web it is an error instead.
if (EMSCRIPTEN)
    target_compile_options(RecompiledFuncs PRIVATE -Werror=implicit-function-declaration)
endif()

# PatchesLib - Library containing the recompiled output for any custom function patches
add_library(PatchesLib STATIC)

target_compile_options(PatchesLib PRIVATE
    # -Wno-unused-but-set-variable
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
)

target_include_directories(PatchesLib PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)

target_sources(PatchesLib PRIVATE
    ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
    ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c
)

set_source_files_properties(${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c PROPERTIES COMPILE_FLAGS -fno-strict-aliasing)

# The patches call the game's functions, declared in RecompiledFuncs/funcs.h.
if (EMSCRIPTEN)
    target_compile_options(PatchesLib PRIVATE -Werror=implicit-function-declaration
        "SHELL:-include ${CMAKE_SOURCE_DIR}/RecompiledFuncs/funcs.h")
endif()

# Build patches elf
if(NOT DEFINED PATCHES_C_COMPILER)
    set(PATCHES_C_COMPILER clang)
endif()

if(NOT DEFINED PATCHES_LD)
    set(PATCHES_LD ld.lld)
endif()

if(NOT DEFINED PATCHES_OBJCOPY)
    set(PATCHES_OBJCOPY llvm-objcopy)
endif()

add_custom_target(PatchesBin
    COMMAND ${CMAKE_COMMAND} -E env CC=${PATCHES_C_COMPILER} LD=${PATCHES_LD} OBJCOPY=${PATCHES_OBJCOPY} make
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/patches
    BYPRODUCTS ${CMAKE_SOURCE_DIR}/patches/patches.elf
)

# Generate patches_bin.c from patches.bin
add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c
    COMMAND file_to_c ${CMAKE_SOURCE_DIR}/patches/patches.bin mm_patches_bin ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.c ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches_bin.h 
    DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.bin
)

# Recompile patches elf into patches.c and patches.bin
add_custom_command(OUTPUT
                       ${CMAKE_SOURCE_DIR}/patches/patches.bin
                       ${CMAKE_SOURCE_DIR}/RecompiledPatches/patches.c
                       ${CMAKE_SOURCE_DIR}/RecompiledPatches/recomp_overlays.inl
                       ${CMAKE_SOURCE_DIR}/RecompiledPatches/funcs.h
                   # TODO: Look into why modifying patches requires two builds to take
                   COMMAND ./N64Recomp patches.toml
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                   DEPENDS ${CMAKE_SOURCE_DIR}/patches/patches.elf
)

# Download controller db file for controller support via SDL2
set(GAMECONTROLLERDB_COMMIT "b1e4090b3d4266e55feb0793efa35792e05faf66")
set(GAMECONTROLLERDB_URL "https://raw.githubusercontent.com/gabomdq/SDL_GameControllerDB/${GAMECONTROLLERDB_COMMIT}/gamecontrollerdb.txt")

file(DOWNLOAD ${GAMECONTROLLERDB_URL} ${CMAKE_SOURCE_DIR}/gamecontrollerdb.txt
     TLS_VERIFY ON)

add_custom_target(DownloadGameControllerDB
    DEPENDS ${CMAKE_SOURCE_DIR}/gamecontrollerdb.txt)

# Main executable
add_executable(GoldenRecomp)
add_dependencies(GoldenRecomp DownloadGameControllerDB)

# Generate mm_shader_cache.c from the MM shader cache if it exists
if (EXISTS ${CMAKE_SOURCE_DIR}/shadercache/mm_shader_cache.bin)
    set(HAS_MM_SHADER_CACHE TRUE)
    target_compile_definitions(GoldenRecomp PRIVATE HAS_MM_SHADER_CACHE)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mm_shader_cache.c ${CMAKE_CURRENT_BINARY_DIR}/mm_shader_cache.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/shadercache/mm_shader_cache.bin mm_shader_cache_bytes ${CMAKE_CURRENT_BINARY_DIR}/mm_shader_cache.c ${CMAKE_CURRENT_BINARY_DIR}/mm_shader_cache.h
        DEPENDS ${CMAKE_SOURCE_DIR}/shadercache/mm_shader_cache.bin
    )
endif()

if (LIVE_GAMECODE)
    target_compile_definitions(GoldenRecomp PRIVATE GE_LIVE_GAMECODE)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ge_dump_toml.c ${CMAKE_CURRENT_BINARY_DIR}/ge_dump_toml.h
        COMMAND file_to_c ${CMAKE_SOURCE_DIR}/dump.toml ge_dump_toml ${CMAKE_CURRENT_BINARY_DIR}/ge_dump_toml.c ${CMAKE_CURRENT_BINARY_DIR}/ge_dump_toml.h
        DEPENDS ${CMAKE_SOURCE_DIR}/dump.toml file_to_c
    )
    target_sources(GoldenRecomp PRIVATE
        ${CMAKE_SOURCE_DIR}/src/main/live_gamecode.cpp
        ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/src/config.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/ge_dump_toml.c
    )
    # config.cpp provides Context::from_symbol_file (toml symbol context parser)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/src/config.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/lib/tomlplusplus/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/lib/fmt/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/src;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include"
    )
    set_source_files_properties(${CMAKE_SOURCE_DIR}/src/main/live_gamecode.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/lib/rabbitizer/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/lib/rabbitizer/cplusplus/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty;${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty/concurrentqueue;${CMAKE_BINARY_DIR}"
    )
    target_link_libraries(GoldenRecomp PRIVATE LiveRecomp fmt)
    if (GE_HYBRID_DEBUG)
        target_link_options(GoldenRecomp PRIVATE -Wl,--export-dynamic)
    endif()
endif()

# The app icon, as the launcher's thumbnail for the game and the window's icon on Linux.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
    COMMAND file_to_c ${CMAKE_SOURCE_DIR}/icons/512.png icon_bytes ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.h
    DEPENDS ${CMAKE_SOURCE_DIR}/icons/512.png file_to_c
)

# Embed the retail->TLBFREE ROM patch so the app can convert an unmodified retail
# NTSC-U ROM selected in the ROM picker.
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ge_rom_patch.c ${CMAKE_CURRENT_BINARY_DIR}/ge_rom_patch.h
    COMMAND file_to_c ${CMAKE_SOURCE_DIR}/vanilla_to_tlbfree.gep1 ge_rom_patch ${CMAKE_CURRENT_BINARY_DIR}/ge_rom_patch.c ${CMAKE_CURRENT_BINARY_DIR}/ge_rom_patch.h
    DEPENDS ${CMAKE_SOURCE_DIR}/vanilla_to_tlbfree.gep1 file_to_c
)

set (SOURCES
    ${CMAKE_SOURCE_DIR}/src/main/main.cpp
    ${CMAKE_SOURCE_DIR}/src/main/register_overlays.cpp
    ${CMAKE_SOURCE_DIR}/src/main/register_patches.cpp
    ${CMAKE_SOURCE_DIR}/src/main/rom_conversion.cpp
    ${CMAKE_SOURCE_DIR}/src/main/libultra_tlb_stubs.c
    ${CMAKE_SOURCE_DIR}/src/main/launcher.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ge_rom_patch.c
    ${CMAKE_CURRENT_BINARY_DIR}/icon_bytes.c

    ${CMAKE_SOURCE_DIR}/src/game/input.cpp
    ${CMAKE_SOURCE_DIR}/src/game/mouse.cpp
    ${CMAKE_SOURCE_DIR}/src/game/ext_controls.cpp
    ${CMAKE_SOURCE_DIR}/src/game/config.cpp
    ${CMAKE_SOURCE_DIR}/src/game/scene_table.cpp
    ${CMAKE_SOURCE_DIR}/src/game/debug.cpp
    ${CMAKE_SOURCE_DIR}/src/game/quicksaving.cpp
    ${CMAKE_SOURCE_DIR}/src/game/recomp_api.cpp

    ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp
    #${CMAKE_SOURCE_DIR}/rsp/njpgdspMain.cpp
)

if (HAS_MM_SHADER_CACHE)
    list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/mm_shader_cache.c)
endif()

target_include_directories(GoldenRecomp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty/concurrentqueue
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/thirdparty/sse2neon
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompinput/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/include
    ${CMAKE_SOURCE_DIR}/lib/RecompFrontend/recompui/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/hlslpp/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/plume
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/inc
    ${CMAKE_SOURCE_DIR}/lib/rt64/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/rhi
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/render
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/nativefiledialog-extended/src/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    target_compile_options(GoldenRecomp PRIVATE
        #-march=nehalem
        -march=core2
        -fno-strict-aliasing
        -fms-extensions
        -msse4.1 # added for core2
    )
else()
    target_compile_options(GoldenRecomp PRIVATE
        -fno-strict-aliasing
        -fms-extensions
    )
endif()

if (MSVC)
    # Disable identical code folding, since this breaks mod function patching as multiple functions can get merged into one.
    target_link_options(GoldenRecomp PRIVATE /OPT:NOICF)
endif()

if (WIN32)
    include(FetchContent)

    if (DEFINED ENV{SDL2_VERSION})
        set(SDL2_VERSION $ENV{SDL2_VERSION})
    else()
        set(SDL2_VERSION "2.30.3")
    endif()

    # Fetch SDL2 on windows
    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(GoldenRecomp PRIVATE
        ${sdl2_SOURCE_DIR}/include
    )
    target_link_directories(GoldenRecomp PRIVATE
        ${sdl2_SOURCE_DIR}/lib/x64
    )
    # RT64 looks for SDL2 in its mupen64plus-win32-deps submodule on Windows, which
    # lib/rt64 doesn't carry: build it against the same SDL2 the program links.
    target_include_directories(rt64 PRIVATE
        ${sdl2_SOURCE_DIR}/include
    )

    # Copy SDL2 and dxc DLLs to output folder as post build step
    add_custom_command(TARGET GoldenRecomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:GoldenRecomp>)

    set_target_properties(
        GoldenRecomp
        PROPERTIES
            LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
            LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )

    target_sources(GoldenRecomp PRIVATE ${CMAKE_SOURCE_DIR}/icons/app.rc)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(SDL2 REQUIRED)
    add_compile_definitions("PLUME_SDL_VULKAN_ENABLED")
    add_compile_definitions("RT64_SDL_WINDOW_VULKAN")

    message(STATUS "SDL2_FOUND = ${SDL2_FOUND}")
    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")

    target_include_directories(GoldenRecomp PRIVATE ${SDL2_INCLUDE_DIRS})

    find_package(Freetype REQUIRED)

    message(STATUS "FREETYPE_FOUND = ${FREETYPE_FOUND}")
    message(STATUS "FREETYPE_INCLUDE_DIRS = ${FREETYPE_INCLUDE_DIRS}")
    message(STATUS "FREETYPE_LIBRARIES = ${FREETYPE_LIBRARIES}")

    include_directories(${FREETYPE_LIBRARIES})
    target_link_libraries(GoldenRecomp PRIVATE ${FREETYPE_LIBRARIES})

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)

    target_link_libraries(GoldenRecomp PRIVATE "-latomic -static-libstdc++" ${CMAKE_DL_LIBS} Threads::Threads)
endif()

if (APPLE)
    # macOS: dependencies via Homebrew (brew install sdl2 freetype).
    # If devkitPro is installed, its Switch cross-compilation SDL2 can be found
    # first, which produces confusing errors (e.g. SDL_SysWMinfo without a
    # 'cocoa' member). Point explicitly at the Homebrew SDL2 and ignore devkitPro.
    list(APPEND CMAKE_IGNORE_PREFIX_PATH /opt/devkitpro)
    list(APPEND CMAKE_IGNORE_PATH /opt/devkitpro /opt/devkitpro/portlibs/switch
                                  /opt/devkitpro/portlibs/switch/include)
    find_program(BREW_EXECUTABLE brew)
    if (BREW_EXECUTABLE)
        execute_process(COMMAND ${BREW_EXECUTABLE} --prefix sdl2
            OUTPUT_VARIABLE BREW_SDL2_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (BREW_SDL2_PREFIX AND EXISTS "${BREW_SDL2_PREFIX}/lib/cmake/SDL2")
            set(SDL2_DIR "${BREW_SDL2_PREFIX}/lib/cmake/SDL2" CACHE PATH "Homebrew SDL2" FORCE)
            list(PREPEND CMAKE_PREFIX_PATH "${BREW_SDL2_PREFIX}")
        endif()
    endif()

    find_package(SDL2 REQUIRED)
    find_package(Freetype REQUIRED)

    target_include_directories(GoldenRecomp PRIVATE ${SDL2_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
    if (TARGET SDL2::SDL2)
        target_link_libraries(GoldenRecomp PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(GoldenRecomp PRIVATE ${SDL2_LIBRARIES})
    endif()
    target_link_libraries(GoldenRecomp PRIVATE ${FREETYPE_LIBRARIES})

    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    target_link_libraries(GoldenRecomp PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)

    # Frameworks RT64's Metal backend and SDL rely on
    target_link_libraries(GoldenRecomp PRIVATE
        "-framework Metal" "-framework QuartzCore" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo"
    )
endif()

# TODO fix the rt64 CMake script so that this doesn't need to be duplicated here
# For DXC, which builds RecompFrontend's UI shaders
set (DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set (DXC_DXIL_OPTS "-Wno-ignored-attributes")
set (DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set (DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set (DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
set (DXC_CS_OPTS "${DXC_COMMON_OPTS}" "-E" "CSMain" "-T cs_6_0")
set (DXC_GS_OPTS "${DXC_COMMON_OPTS}" "-E" "GSMain" "-T gs_6_0")
set (DXC_RT_OPTS "${DXC_COMMON_OPTS}" "-D" "RT_SHADER" "-T" "lib_6_3" "-fspv-target-env=vulkan1.1spirv1.4" "-fspv-extension=SPV_KHR_ray_tracing" "-fspv-extension=SPV_EXT_descriptor_indexing")

if (WIN32)
    add_compile_definitions(NOMINMAX)
endif()
# DXC runs on the machine that builds, which for the page (Emscripten) isn't the target.
if (CMAKE_HOST_WIN32)
    set (DXC "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc.exe")
else()
    if (CMAKE_HOST_APPLE)
        # Apple's binary is universal, so it'll work on both x86_64 and arm64
        set (DXC "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-macos")
    else()
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
            set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc-linux")
        else()
            set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-linux")
        endif()
    endif()
endif()

# RecompFrontend: the launcher, the settings menus, input and the RT64 render context.
set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)
set(RECOMP_FRONTEND_RT64_PATH ${CMAKE_SOURCE_DIR}/lib/rt64)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RecompFrontend)

target_link_libraries(GoldenRecomp PRIVATE
    PatchesLib
    RecompiledFuncs
    recompui
    recompinput
    librecomp
    ultramodern
    rt64
)

# The page reads the assets and the controller database from its preloaded file system.
if (EMSCRIPTEN)
    n64recomp_browser_frontend_page(GoldenRecomp)
    target_link_options(GoldenRecomp PRIVATE
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/gamecontrollerdb.txt@/gamecontrollerdb.txt")
else()
    target_link_libraries(GoldenRecomp PRIVATE $<$<NOT:$<PLATFORM_ID:Darwin>>:SDL2> nfd)
endif()

target_sources(GoldenRecomp PRIVATE ${SOURCES})

set_property(TARGET GoldenRecomp PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

