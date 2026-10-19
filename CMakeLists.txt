cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD a8_gb_board CACHE STRING "A8PicoCart purple RP2040 clone")
list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/boards")
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Set PICO_SDK_PATH to Raspberry Pi Pico SDK (tested: 2.1.1)")
endif()
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(atariboy LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
add_executable(atariboy main.cpp settings.cpp options_ui.cpp app.cpp state_ui.cpp audio.c vendor/minigb_apu/minigb_apu.c engine.c video.cpp controls.cpp disk_optimize.c storage.c browser.cpp storage/flash_fs.c storage/fatfs_disk.c storage/msc_disk.c storage/usb_descriptors.c storage/fatfs/ff.c storage/fatfs/ffunicode.c storage/fatfs/diskio.c usb_mode.c saves.c osd.cpp sha256.c)
set_source_files_properties(main.cpp settings.cpp options_ui.cpp app.cpp state_ui.cpp audio.c vendor/minigb_apu/minigb_apu.c engine.c video.cpp controls.cpp disk_optimize.c storage.c browser.cpp storage/flash_fs.c storage/fatfs_disk.c storage/msc_disk.c storage/usb_descriptors.c storage/fatfs/ff.c storage/fatfs/ffunicode.c storage/fatfs/diskio.c usb_mode.c saves.c osd.cpp sha256.c PROPERTIES COMPILE_OPTIONS "-O3;-Wall;-Wextra;-fstack-usage")
target_link_libraries(atariboy PRIVATE pico_stdlib pico_multicore hardware_clocks hardware_vreg hardware_sync hardware_flash tinyusb_device tinyusb_board pico_unique_id)
target_include_directories(atariboy PRIVATE . storage storage/fatfs)
# This application needs no C++ runtime library. Link with the C driver so an
# embedded libstdc++ archive is not required just to use C++ syntax and uint32_t.
set_target_properties(atariboy PROPERTIES LINKER_LANGUAGE C)
pico_enable_stdio_usb(atariboy 0)
pico_enable_stdio_uart(atariboy 0)
pico_set_program_name(atariboy "AtariBoy")
pico_set_program_version(atariboy "0.90b")
pico_add_extra_outputs(atariboy)

target_compile_options(atariboy PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
target_compile_definitions(atariboy PRIVATE PICO_STACK_SIZE=0x1000 PICO_MALLOC_PANIC=0 PICO_USE_OPTIMISTIC_SBRK=0)
pico_set_binary_type(atariboy copy_to_ram)
# Retain SDK RAM startup but opt selected cold objects into XIP.
pico_set_linker_script(atariboy ${CMAKE_CURRENT_LIST_DIR}/memmap_hybrid.ld)

# Cold UI/state routines execute from XIP. Prefer compact code here; retain
# -O3 for emulation/video and existing explicit COM optimization settings.
set_source_files_properties(state_ui.cpp saves.c sha256.c browser.cpp disk_optimize.c storage.c storage/fatfs/ff.c storage/flash_fs.c storage/fatfs_disk.c storage/msc_disk.c usb_mode.c PROPERTIES COMPILE_OPTIONS "-Os;-Wall;-Wextra;-fstack-usage")

set_source_files_properties(audio.c vendor/minigb_apu/minigb_apu.c PROPERTIES COMPILE_OPTIONS "-Os;-Wall;-Wextra;-fstack-usage")
target_compile_definitions(atariboy PRIVATE MINIGB_APU_AUDIO_FORMAT_S16SYS AUDIO_SAMPLE_RATE=18635)
