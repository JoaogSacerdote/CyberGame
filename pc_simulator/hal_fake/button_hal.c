#include "button_hal.h"
#include <SDL2/SDL.h>
#include <assert.h>

/* Mapeamento primario:
 *   Setas      = joystick (movimento)
 *   Z ou Space = BTN_A  (confirmar / carta correta)
 *   X ou Esc   = BTN_B  (cancelar / voltar)
 *   C          = BTN_X  (carta errada no mock)
 *   V          = BTN_Y  (interagir / abrir terminal)
 *   Enter      = BTN_START
 */
static SDL_Scancode s_key_primary[BTN_MAX_COUNT] = {
    [BTN_A]     = SDL_SCANCODE_Z,
    [BTN_B]     = SDL_SCANCODE_X,
    [BTN_X]     = SDL_SCANCODE_C,
    [BTN_Y]     = SDL_SCANCODE_V,
    [BTN_START] = SDL_SCANCODE_RETURN,
};

/* Teclas alternativas (mais intuitivas para teclado): */
static SDL_Scancode s_key_alt[BTN_MAX_COUNT] = {
    [BTN_A]     = SDL_SCANCODE_SPACE,   /* Space = confirmar */
    [BTN_B]     = SDL_SCANCODE_ESCAPE,  /* Esc   = cancelar  */
    [BTN_X]     = SDL_SCANCODE_A,
    [BTN_Y]     = SDL_SCANCODE_F,       /* F     = interagir */
    [BTN_START] = SDL_SCANCODE_P,       /* P     = pause     */
};

esp_err_t button_hal_init(void) {
    return ESP_OK;
}

bool button_hal_get_event(button_event_t *event, uint32_t timeout_ms) {
    (void)event; (void)timeout_ms;
    return false;
}

button_state_t button_hal_peek(button_id_t id) {
    assert(id < BTN_MAX_COUNT);
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    if (keys == NULL) return BTN_RELEASED;
    const bool pressed = keys[s_key_primary[id]] || keys[s_key_alt[id]];
    return pressed ? BTN_PRESSED : BTN_RELEASED;
}
