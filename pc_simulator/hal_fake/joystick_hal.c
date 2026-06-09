#include "joystick_hal.h"
#include <SDL2/SDL.h>

/* Setas do teclado mapeadas para os eixos do joystick.
 * Y positivo = cima (conforme joystick_hal.h). */

esp_err_t joystick_hal_init(void) {
    return ESP_OK;
}

joystick_data_t joystick_hal_get_state(void) {
    joystick_data_t d = { 0, 0 };
    const uint8_t *keys = SDL_GetKeyboardState(NULL);
    if (keys == NULL) return d;

    if (keys[SDL_SCANCODE_LEFT])  d.x = -100;
    if (keys[SDL_SCANCODE_RIGHT]) d.x =  100;
    if (keys[SDL_SCANCODE_UP])    d.y =  100;
    if (keys[SDL_SCANCODE_DOWN])  d.y = -100;

    return d;
}
