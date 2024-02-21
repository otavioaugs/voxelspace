#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 	240
#define SCREEN_HEIGHT	240

bool init_window(void);
void destroy_window(void);

void clear_framebuffer(uint32_t color);
void render_framebuffer(void);

void draw_pixel(uint16_t x, uint16_t y, uint32_t color);

#endif
