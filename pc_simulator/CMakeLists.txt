cmake_minimum_required(VERSION 3.10)
project(cybersim C)

# ============================================================================
# Simulador PC do firmware CyberGame.
#
# Etapa 1 (atual): "Hello LVGL" — janela SDL + LVGL inicializada, tela vazia
# com label "CyberSim — pronto para receber o firmware". Prova de conceito
# que toolchain (MinGW + SDL2) + LVGL 9 estao funcionando.
#
# Etapas futuras (2, 3, 4): adicionar HALs fake, FreeRTOS shim, storage fake,
# engine + FSM, telas. Cada etapa entrega algo testavel.
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

find_package(SDL2 REQUIRED)

# === LVGL ===
# Reusa o mesmo LVGL 9.3.0 que o firmware usa (managed_components/).
# Mantem versao identica entre firmware e simulador.
set(LVGL_DIR "${CMAKE_SOURCE_DIR}/../managed_components/lvgl__lvgl")
add_subdirectory(${LVGL_DIR} lvgl_build)

# lv_conf.h mora no pc_simulator (separado do firmware que tem o proprio
# lv_conf.defaults gerado pelo ESP-IDF Kconfig).
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

# === Executavel principal ===
add_executable(cybersim main.c)
target_compile_definitions(cybersim PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_include_directories(cybersim PRIVATE
    ${SDL2_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${LVGL_DIR}            # raiz do LVGL — habilita #include "lvgl.h" e "src/..."
)
target_link_libraries(cybersim PRIVATE lvgl ${SDL2_LIBRARIES} m pthread)

# === Copia DLLs do MSYS2 pro bin/ apos build (apenas Windows) ===
if(WIN32)
    set(MSYS_BIN "C:/msys64/mingw64/bin")
    foreach(DLL SDL2.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${MSYS_BIN}/${DLL}")
            add_custom_command(TARGET cybersim POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MSYS_BIN}/${DLL}" $<TARGET_FILE_DIR:cybersim>)
        endif()
    endforeach()
endif()
