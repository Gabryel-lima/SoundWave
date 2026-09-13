cmake_minimum_required(VERSION 3.16)

project(SoundWave
    VERSION 2.0.0
    DESCRIPTION "Laboratorio de mapeamento entre o espectro sonoro e o eletromagnetico"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de build" FORCE)
endif()

option(SOUNDWAVE_BUILD_TESTS "Compilar a suite de testes" ON)
option(SOUNDWAVE_BUILD_EXAMPLES "Compilar os exemplos" ON)
option(SOUNDWAVE_WERROR "Tratar avisos como erros" OFF)

# -----------------------------------------------------------------------------
# Nucleo: ZERO dependencias externas.
#
# Isto e uma decisao de arquitetura, nao uma conveniencia. O nucleo matematico
# precisa ser compilavel e testavel em qualquer maquina, sem instalar nada --
# caso contrario a verificacao do projeto fica refem do ambiente. FFT, leitura
# de WAV, escrita de PNG e o harness de teste sao todos proprios.
#
# Bibliotecas externas entram apenas como backends OPCIONAIS, sempre atras de
# uma interface que o nucleo ja define.
# -----------------------------------------------------------------------------
add_library(soundwave_core
    src/core/spectrum.cpp
    src/core/config.cpp
    src/core/version.cpp
    src/audio/pcm_buffer.cpp
    src/audio/wav_reader.cpp
    src/audio/signal_generator.cpp
    src/dsp/window.cpp
    src/dsp/fft.cpp
    src/dsp/analyzer.cpp
    src/mapping/frequency_mapper.cpp
    src/mapping/mappers.cpp
    src/color/spectral.cpp
    src/color/cie_tables.cpp
    src/color/srgb.cpp
    src/color/em_band.cpp
    src/color/color_engine.cpp
    src/render/image.cpp
    src/render/interpretation.cpp
    src/render/plots.cpp
)

target_include_directories(soundwave_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(MSVC)
    target_compile_options(soundwave_core PRIVATE /W4)
    if(SOUNDWAVE_WERROR)
        target_compile_options(soundwave_core PRIVATE /WX)
    endif()
else()
    target_compile_options(soundwave_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow
        -Wconversion -Wsign-conversion)
    if(SOUNDWAVE_WERROR)
        target_compile_options(soundwave_core PRIVATE -Werror)
    endif()
endif()

# -----------------------------------------------------------------------------
# Dependencias opcionais.
# -----------------------------------------------------------------------------
find_package(PkgConfig QUIET)

set(SOUNDWAVE_HAS_SDL2 OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
    if(SDL2_FOUND)
        set(SOUNDWAVE_HAS_SDL2 ON)
    endif()
endif()

find_package(ZLIB QUIET)
set(SOUNDWAVE_HAS_ZLIB OFF)
if(ZLIB_FOUND)
    set(SOUNDWAVE_HAS_ZLIB ON)
    target_compile_definitions(soundwave_core PRIVATE SOUNDWAVE_WITH_ZLIB)
    target_link_libraries(soundwave_core PRIVATE ZLIB::ZLIB)
endif()

set(SOUNDWAVE_HAS_FFTW OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(FFTW3 QUIET fftw3)
    if(FFTW3_FOUND)
        set(SOUNDWAVE_HAS_FFTW ON)
    endif()
endif()

# -----------------------------------------------------------------------------
# Executavel.
# -----------------------------------------------------------------------------
add_executable(soundwave
    src/cli/main.cpp
    src/cli/commands.cpp
    src/viz/realtime.cpp
)
target_link_libraries(soundwave PRIVATE soundwave_core)
target_include_directories(soundwave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/cli)

if(SOUNDWAVE_HAS_SDL2)
    target_compile_definitions(soundwave PRIVATE SOUNDWAVE_WITH_SDL2)
    target_include_directories(soundwave PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(soundwave PRIVATE ${SDL2_LIBRARIES})
    target_link_directories(soundwave PRIVATE ${SDL2_LIBRARY_DIRS})
endif()

if(SOUNDWAVE_HAS_FFTW)
    target_compile_definitions(soundwave_core PRIVATE SOUNDWAVE_WITH_FFTW)
endif()

# -----------------------------------------------------------------------------
# Testes.
#
# Harness proprio (tests/test_framework.hpp), pelo mesmo motivo do nucleo: os
# testes sao a unica evidencia de que a matematica esta certa, e precisam rodar
# em qualquer lugar sem baixar nada.
# -----------------------------------------------------------------------------
if(SOUNDWAVE_BUILD_TESTS)
    enable_testing()
    add_executable(soundwave_tests
        tests/main.cpp
        tests/test_fft.cpp
        tests/test_window.cpp
        tests/test_analyzer.cpp
        tests/test_mapping.cpp
        tests/test_color.cpp
        tests/test_cie.cpp
        tests/test_audio.cpp
        tests/test_config.cpp
        tests/test_pipeline.cpp
    )
    target_link_libraries(soundwave_tests PRIVATE soundwave_core)
    target_include_directories(soundwave_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_test(NAME soundwave_tests COMMAND soundwave_tests)
endif()

if(SOUNDWAVE_BUILD_EXAMPLES)
    add_executable(example_mapping_comparison examples/mapping_comparison.cpp)
    target_link_libraries(example_mapping_comparison PRIVATE soundwave_core)

    add_executable(example_minimal_pipeline examples/minimal_pipeline.cpp)
    target_link_libraries(example_minimal_pipeline PRIVATE soundwave_core)
endif()

# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "SoundWave ${PROJECT_VERSION}")
message(STATUS "  build            : ${CMAKE_BUILD_TYPE}")
message(STATUS "  testes           : ${SOUNDWAVE_BUILD_TESTS}")
message(STATUS "  exemplos         : ${SOUNDWAVE_BUILD_EXAMPLES}")
message(STATUS "  zlib (PNG menor) : ${SOUNDWAVE_HAS_ZLIB}")
message(STATUS "  SDL2 (tempo real): ${SOUNDWAVE_HAS_SDL2}")
message(STATUS "  FFTW3 (opcional) : ${SOUNDWAVE_HAS_FFTW}")
message(STATUS "")
message(STATUS "  O nucleo e os testes nao dependem de nenhuma delas.")
message(STATUS "")
