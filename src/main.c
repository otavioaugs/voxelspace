#include <stdio.h>
#include <stdlib.h>

#include "display.h"

#include <SDL2/SDL.h>
#include "../libs/gif/gifload.h"

#define MAP_SIZE 1024
#define SCALE_FACTOR 100.0

bool is_running = false;

uint32_t palette[256 * 3];

// Buffers for heightmap & colormap
uint8_t *heightmap = NULL; 
uint8_t *colormap  = NULL;

// Camera object
typedef struct {
  float x;       // X position on the map
  float y;       // Y position on the map
  float height;  // Height of the camera
  float angle;   // Camera angle (radians, clockwise)
  float horizon; // Offset of the horizon position(looking up-down)
  float zfar;    // Distance of the camera looking forward
} camera_t;

// Camera object init
camera_t camera = {
  .x       = 512.0,
  .y       = 512.0,
  .height  = 150.0,
  .angle   = 1.5 * 3.14, // -> the same as 270deg
  .horizon = 100.0,
  .zfar    = 600.0
};

// Process input of user
void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        is_running = false;
        break;
    }
  }

  const uint8_t *keystate = SDL_GetKeyboardState(NULL);

  // WASD
  if (keystate[SDL_SCANCODE_UP]) {
    camera.x += cos(camera.angle);
    camera.y += sin(camera.angle);
  }
  if (keystate[SDL_SCANCODE_DOWN]) {
    camera.x -= cos(camera.angle);
    camera.y -= sin(camera.angle);
  }
  if (keystate[SDL_SCANCODE_LEFT]) {
    camera.angle -= 0.01;
  }
  if (keystate[SDL_SCANCODE_RIGHT]) {
    camera.angle += 0.01;
  }

  if (keystate[SDL_SCANCODE_E]) {
    camera.height++;
  }
  if (keystate[SDL_SCANCODE_D]) {
    camera.height--;
  }

  if (keystate[SDL_SCANCODE_Q]) {
    camera.horizon += 1.5;
  }
  if (keystate[SDL_SCANCODE_W]) {
    camera.horizon -= 1.5;
  }
}

int main(int argc, char *argv[]) {
	is_running = init_window();

	// Load the maps
	uint8_t gif_palette[256 * 3];
	int pal_count;

	colormap  = load_gif("maps/gif/map0.color.gif", &pal_count, gif_palette);
	heightmap = load_gif("maps/gif/map0.height.gif", NULL, NULL);

  // Load the palette
	for (int i = 0; i < pal_count; i++) {
		int r = gif_palette[3 * i + 0];
		int g = gif_palette[3 * i + 1];
		int b = gif_palette[3 * i + 2];
		r = (r & 63) << 2;
		g = (g & 63) << 2;
		b = (b & 63) << 2;
		palette[i] = (b << 16) | (g << 8) | r;
	}

	while (is_running) {
		clear_framebuffer(0xFFE0BB36);

    process_input();

    // cos and sine
    /* \     |
        \    |
         \   |
          \  | sin -delta y
           \ |
            \|
       ------|
        cos
         | 
       delta x
    */


    float sin_angle = sin(camera.angle);
    float cos_angle = cos(camera.angle);

    /* pl           pr
        * zfar| zfar*
         \   z|    /
          \  f|   /
           \ a|  /
            \r| /
             \|/
       -------*--------
           camera
    */ 

    // Most left point of the camera
    float plx = cos_angle * camera.zfar + sin_angle * camera.zfar; 
    float ply = sin_angle * camera.zfar - cos_angle * camera.zfar;

    // Most right point of the camera
    float prx = cos_angle * camera.zfar - sin_angle * camera.zfar;
    float pry = sin_angle * camera.zfar + cos_angle * camera.zfar; 

    // Loop 320 rays from left to right
    for (int i = 0; i < SCREEN_WIDTH; i++) {
      
      // Getting "slope" of each ray to cast correctly
      // (pr.x - pl.x) / SCREEN_WIDTH: Length of each small segment
      // * i: Offset by the current ray/column
      // / camera.zfar: Divide by the zfar length to adjust for perpesctive
      float deltax = (plx + (prx - plx) / SCREEN_WIDTH * i) / camera.zfar;
      float deltay = (ply + (pry - ply) / SCREEN_WIDTH * i) / camera.zfar;

      // Position of each ray
      float rx = camera.x;
      float ry = camera.y;

      // Get max voxel height 
      float max_height = SCREEN_HEIGHT;

      // Depth loop (how far is from camera) 
      for (int z = 1; z < camera.zfar; z++) {
        // "Sloping" each ray
        rx += deltax;
        ry += deltay;

        // Find the offset that we have to go and fetch values from the heightmap and colormap
        int map_offset = ((MAP_SIZE * ((int)ry & (MAP_SIZE - 1)) + ((int)rx & (MAP_SIZE - 1))));

        // Project heights and find the height on-screen
        int proj_height = (int)((camera.height - heightmap[map_offset]) / z * SCALE_FACTOR + camera.horizon);

        // Only render the terrain pixels if the new projected height is taller than the previous max-height
        if (proj_height < max_height) {
          // Draw pixels from previous max-height until the new projected height
          for (int y = proj_height; y < max_height; y++) {
            if (y >= 0) {
              draw_pixel(i, y, palette[colormap[map_offset]]);
            }
          }
          max_height = proj_height;
        }
      }
    }

    render_framebuffer();
	}

	destroy_window();

	return EXIT_SUCCESS;
}
