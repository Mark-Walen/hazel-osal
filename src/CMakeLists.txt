add_library(lynx_osal STATIC
    osal_freertos.c
    osal_sem_freertos.c
)

target_include_directories(lynx_osal
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_compile_definitions(lynx_osal PUBLIC CONFIG_FREERTOS_ENABLE=1)
target_link_libraries(lynx_osal PUBLIC freertos_kernel)

target_compile_options(lynx_osal PRIVATE
    -Wall -Wextra -Werror
    -ffunction-sections -fdata-sections
)
