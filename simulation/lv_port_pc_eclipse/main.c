/**
 * @file main
 * CyberSec: Network Defender — PC Simulator entry point
 */

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/lvgl.h"
#include "cybersec_game.h"
#include <SDL2/SDL.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t * hal_init(int32_t w, int32_t h);
static int sdl_key_watch(void *userdata, SDL_Event *event);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *      VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int main(int argc, char **argv)
{
  (void)argc; (void)argv;

  /*Initialize LVGL*/
  lv_init();

  /*Initialize 480x320 display (matches SCR_W/SCR_H constants)*/
  lv_display_t *disp = hal_init(480, 320);

  /*Hook raw SDL keyboard events into the game engine*/
  SDL_AddEventWatch(sdl_key_watch, NULL);

  /*Start the CyberSec game*/
  cybersec_start();

  while(1) {
    uint32_t time_until_next = lv_timer_handler();
    if (time_until_next == LV_NO_TIMER_READY)
      time_until_next = LV_DEF_REFR_PERIOD;
    lv_delay_ms(time_until_next);
  }

  lv_deinit();
  return 0;
}

static int sdl_key_watch(void *userdata, SDL_Event *event)
{
  (void)userdata;
  if (event->type == SDL_KEYDOWN)
    cybersec_sdl_key_event(event->key.keysym.sym, true);
  else if (event->type == SDL_KEYUP)
    cybersec_sdl_key_event(event->key.keysym.sym, false);
  return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the LVGL graphics
 * library
 */
static lv_display_t * hal_init(int32_t w, int32_t h)
{
  lv_group_set_default(lv_group_create());

  lv_display_t * disp = lv_sdl_window_create(w, h);

  lv_indev_t * mouse = lv_sdl_mouse_create();
  lv_indev_set_group(mouse, lv_group_get_default());
  lv_indev_set_display(mouse, disp);
  lv_display_set_default(disp);

  LV_IMAGE_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
  lv_obj_t * cursor_obj;
  cursor_obj = lv_image_create(lv_screen_active()); /*Create an image object for the cursor */
  lv_image_set_src(cursor_obj, &mouse_cursor_icon);           /*Set the image source*/
  lv_indev_set_cursor(mouse, cursor_obj);             /*Connect the image  object to the driver*/

  lv_indev_t * mousewheel = lv_sdl_mousewheel_create();
  lv_indev_set_display(mousewheel, disp);

  lv_indev_t * keyboard = lv_sdl_keyboard_create();
  lv_indev_set_display(keyboard, disp);
  lv_indev_set_group(keyboard, lv_group_get_default());

  return disp;
}
