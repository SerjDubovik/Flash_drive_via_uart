cmake_minimum_required(VERSION 3.22)

option(LCD_PLAYBACK_DIAGNOSTICS "Compare LCD updates from RAM and SD" OFF)

#
# User is free to modify the file as much as necessary
#

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(gcc-arm-none-eabi)
message("Build CMAKE_MODULE_PATH: " ${CMAKE_MODULE_PATH})

# Core project settings
set(CMAKE_PROJECT_NAME STM32F407_cubeLed)
project(${CMAKE_PROJECT_NAME})
enable_language(C CXX ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})
 
# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


# Core MCU flags, CPU type, instruction set and FPU setup
set(cpu_PARAMS 
    -mthumb

    # Other parameters
    # -mcpu, -mfloat, -mfloat-abi, ...
    -mcpu=cortex-m4
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)
    
file(GLOB_RECURSE sources_SRCS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/FATFS/App/*.c
    ${CMAKE_CURRENT_LIST_DIR}/FATFS/Target/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/Third_Party/FatFs/src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/App/*.c
    ${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/Target/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32f407zetx.s
	${CMAKE_CURRENT_SOURCE_DIR}/startup/system_stm32f4xx.c
)

# Include directories for all compilers
set(include_DIRS
${CMAKE_CURRENT_LIST_DIR}/Core/Inc
${CMAKE_CURRENT_LIST_DIR}/FATFS/App
${CMAKE_CURRENT_LIST_DIR}/FATFS/Target
${CMAKE_CURRENT_LIST_DIR}/Middlewares/Third_Party/FatFs/src
${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/App
${CMAKE_CURRENT_LIST_DIR}/USB_DEVICE/Target
${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
${CMAKE_CURRENT_LIST_DIR}/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc

)
# Include directories for each compiler
set(include_c_DIRS

)
set(include_cxx_DIRS
)
set(include_asm_DIRS)
    
# Symbols definition for all compilers
set(symbols_SYMB
STM32F407xx
)

# Symbols definition for each compiler
set(symbols_c_SYMB)
set(symbols_cxx_SYMB)
set(symbols_asm_SYMB)

# Link directories and names of libraries
set(link_DIRS)
set(link_LIBS
# ll_stm32f4xx
cmsis_stm32f4xx
hal_stm32f4xx
)

add_subdirectory(drivers/STM32F4xx_HAL_Driver)
add_subdirectory(drivers/CMSIS)

# Linker script
set(linker_script_SRC ${linker_script_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VGTX_FLASH.ld
)

set(compiler_OPTS)
# Compiler options

# Linker options
set(linker_OPTS)


# Link directories setup
# Must be before executable is added
link_directories(${CMAKE_PROJECT_NAME} ${link_DIRS})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PUBLIC ${sources_SRCS})

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${include_DIRS}
    $<$<COMPILE_LANGUAGE:C>: ${include_c_DIRS}>
    $<$<COMPILE_LANGUAGE:CXX>: ${include_cxx_DIRS}>
    $<$<COMPILE_LANGUAGE:ASM>: ${include_asm_DIRS}>
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    ${symbols_SYMB}
    $<$<BOOL:${LCD_PLAYBACK_DIAGNOSTICS}>:LCD_PLAYBACK_DIAGNOSTICS>
    $<$<COMPILE_LANGUAGE:C>: ${symbols_c_SYMB}>
    $<$<COMPILE_LANGUAGE:CXX>: ${symbols_cxx_SYMB}>
    $<$<COMPILE_LANGUAGE:ASM>: ${symbols_asm_SYMB}>

    # Configuration specific
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>: >
)



# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} ${link_LIBS})

set(lvgl_dir "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/lvgl")
file(GLOB_RECURSE lvgl_sources CONFIGURE_DEPENDS "${lvgl_dir}/src/*.c")
add_library(stm32_lvgl STATIC ${lvgl_sources})
target_include_directories(stm32_lvgl PUBLIC "${lvgl_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc")
target_compile_definitions(stm32_lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(stm32_lvgl PRIVATE ${cpu_PARAMS} -Os)
target_link_libraries(${CMAKE_PROJECT_NAME} stm32_lvgl)

# Compiler options
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    ${cpu_PARAMS}
    ${compiler_OPTS}
    # -save-temps
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -Wdouble-promotion
    -u _printf_float
    $<$<COMPILE_LANGUAGE:C>: >
    $<$<COMPILE_LANGUAGE:CXX>:

    # -Wno-volatile
    # -Wold-style-cast
    # -Wuseless-cast
    # -Wsuggest-override
    >
    $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp -MMD -MP>
    $<$<CONFIG:Debug>:-Og -g3 -ggdb>
    $<$<CONFIG:Release>:-Og -g0>
)

# Linker options
set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS ${linker_script_SRC})
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${linker_script_SRC}
    ${cpu_PARAMS}
    ${linker_OPTS}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
    -u _printf_float # STDIO float formatting support (remove if not used)
    --specs=nosys.specs
    -Wl,--start-group
    -lc
    -lm
    -lstdc++
    -lsupc++
    -Wl,--end-group
    -Wl,-z,max-page-size=8 # Allow good software remapping across address space (with proper GCC section making)
    -Wl,--print-memory-usage
)

# Execute post-build to print size, generate hex and bin
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    # COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
)
