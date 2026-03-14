cmake_minimum_required(VERSION 3.16)
project(stuntrace C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── dependencies ──────────────────────────────────────────────
find_package(SDL2 REQUIRED)

set(SNESRECOMP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/snesrecomp)

# ── recompiled game code ──────────────────────────────────────
add_library(srf_game STATIC
    src/recomp/srf_boot.c
    src/recomp/srf_init.c
    src/recomp/srf_nmi.c
    src/recomp/srf_audio.c
    src/recomp/srf_display.c
    src/recomp/srf_title.c
)

target_include_directories(srf_game PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(srf_game PUBLIC snesrecomp)

# ── launcher ──────────────────────────────────────────────────
add_executable(srf_launcher src/main/main.c)
target_link_libraries(srf_launcher PRIVATE srf_game snesrecomp SDL2::SDL2main)
