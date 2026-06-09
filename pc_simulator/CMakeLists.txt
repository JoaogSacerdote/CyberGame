cmake_minimum_required(VERSION 3.10)
project(cybersim C)

# ============================================================================
# Simulador PC do firmware CyberGame.
#
# Compila os mesmos componentes do firmware (fsm, gamestate, assets, entity,
# ui, engine) substituindo:
#   engine.c      -> sim_engine.c   (sem FreeRTOS tasks)
#   hal_bridge.c  -> lv_sdl_window  (SDL como display LVGL)
#   hardware/*.c  -> hal_fake/*.c   (teclado SDL simula botoes/joystick)
#
# Assets: apontam para sdcard/ na raiz do projeto via caminho relativo
# ao diretorio de execucao (pc_simulator/bin/) definido em RODAR.bat.
# ============================================================================

set(CMAKE_C_STANDARD 99)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

find_package(SDL2 REQUIRED)

# === LVGL ===
set(LVGL_DIR "${CMAKE_SOURCE_DIR}/../managed_components/lvgl__lvgl")
add_subdirectory(${LVGL_DIR} lvgl_build)
target_include_directories(lvgl PUBLIC ${PROJECT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

# === Diretorios dos componentes ===
set(FW_ROOT  "${CMAKE_SOURCE_DIR}/..")
set(CJSON_DIR "${FW_ROOT}/managed_components/espressif__cjson/cJSON")

# === Fontes ===
set(SIM_SOURCES
    main.c
    sim_engine.c

    # HALs fake (teclado SDL)
    hal_fake/button_hal.c
    hal_fake/joystick_hal.c
    hal_fake/ws2812_hal.c
    hal_fake/pmu.c
    hal_fake/nfc_hal.c
    hal_fake/buzzer_hal.c
    hal_fake/hal_common.c

    # cJSON
    ${CJSON_DIR}/cJSON.c

    # FSM
    ${FW_ROOT}/components/fsm/fsm.c

    # Gamestate
    ${FW_ROOT}/components/gamestate/gamestate.c

    # Assets (loader + dados de colisao compilados)
    ${FW_ROOT}/components/assets/asset_loader.c
    ${FW_ROOT}/components/assets/dialog_loader.c
    ${FW_ROOT}/components/assets/collision/collision_recepcao.c
    ${FW_ROOT}/components/assets/collision/collision_empresa.c

    # Entity
    ${FW_ROOT}/components/entity/entity_pool.c
    ${FW_ROOT}/components/entity/movement.c
    ${FW_ROOT}/components/entity/y_sort.c
    ${FW_ROOT}/components/entity/debug_overlay.c
    ${FW_ROOT}/components/entity/room_layout_sd.c

    # Engine — apenas logica (engine.c substituido por sim_engine.c)
    ${FW_ROOT}/components/engine/defense_matrix.c
    ${FW_ROOT}/components/engine/threat.c

    # UI
    ${FW_ROOT}/components/ui/ui_router.c
    ${FW_ROOT}/components/ui/screen_splash.c
    ${FW_ROOT}/components/ui/screen_menu.c
    ${FW_ROOT}/components/ui/screen_pause.c
    ${FW_ROOT}/components/ui/screen_placeholder.c
    ${FW_ROOT}/components/ui/screen_hud.c
    ${FW_ROOT}/components/ui/screen_recepcao.c
    ${FW_ROOT}/components/ui/screen_empresa.c
    ${FW_ROOT}/components/ui/screen_game_over.c
    ${FW_ROOT}/components/ui/screen_room.c
    ${FW_ROOT}/components/ui/room_layout_loader.c
    ${FW_ROOT}/components/ui/screen_tarefa_verde.c
    ${FW_ROOT}/components/ui/screen_tarefa_amarela.c
    ${FW_ROOT}/components/ui/screen_tarefa_vermelha.c
    ${FW_ROOT}/components/ui/screen_servidor_menu.c
    ${FW_ROOT}/components/ui/screen_web_setor.c

    # Stub: asset_chao usa EMBED_FILES do ESP-IDF — substituído por versão sem chão
    hal_fake/asset_chao_sim.c
)

add_executable(cybersim ${SIM_SOURCES})

# Caminhos dos assets relativos a pc_simulator/bin/ (CWD definido em RODAR.bat)
target_compile_definitions(cybersim PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    ASSET_SD_DIR="../../sdcard/assets"
    LAYOUT_SD_DIR="../../sdcard/assets/layout"
    CYBERSIM=1
)

target_include_directories(cybersim PRIVATE
    # shims ESP-IDF PRIMEIRO — garante que esp_err.h etc. venham dos stubs
    ${PROJECT_SOURCE_DIR}/esp_idf_shim
    # lv_conf.h do simulador
    ${PROJECT_SOURCE_DIR}
    # LVGL
    ${LVGL_DIR}
    # SDL2
    ${SDL2_INCLUDE_DIRS}
    # cJSON
    ${CJSON_DIR}
    # Componentes do firmware
    ${FW_ROOT}/components/fsm/include
    ${FW_ROOT}/components/gamestate/include
    ${FW_ROOT}/components/assets/include
    ${FW_ROOT}/components/entity/include
    ${FW_ROOT}/components/engine/include
    ${FW_ROOT}/components/ui/include
    ${FW_ROOT}/components/hardware/include
    ${FW_ROOT}/components/calibracao/include
)

target_link_libraries(cybersim PRIVATE lvgl ${SDL2_LIBRARIES} m pthread)

# Copia DLLs do MSYS2 para bin/ apos build (Windows)
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
