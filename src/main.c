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

// Vector object
typedef struct {
  float x;
  float y;
} vec2;

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

// Load heightmap & colormap
void load_map(void) {
  // Load the maps
  uint8_t gif_palette[256 * 3];
  int pal_count;

  colormap  = load_gif("maps/gif/map0.color.gif", &pal_count, gif_palette);
  heightmap = load_gif("maps/gif/map0.height.gif", NULL, NULL);

  // Load the palette
  for (int i = 0; i < pal_count; i++) {
    // Get RGB
    int r = gif_palette[3 * i + 0];
    int g = gif_palette[3 * i + 1];
    int b = gif_palette[3 * i + 2];

    r = (r & 63) << 2;
    g = (g & 63) << 2;
    b = (b & 63) << 2;

    palette[i] = (b << 16) | (g << 8) | r;
  }
}

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

  // WASD or cursor keys
  if (keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_W]) {
    camera.x += cos(camera.angle);
    camera.y += sin(camera.angle);
  }
  if (keystate[SDL_SCANCODE_DOWN] || keystate[SDL_SCANCODE_S]) {
    camera.x -= cos(camera.angle);
    camera.y -= sin(camera.angle);
  }
  if (keystate[SDL_SCANCODE_LEFT] || keystate[SDL_SCANCODE_A]) {
    camera.angle -= 0.01;
  }
  if (keystate[SDL_SCANCODE_RIGHT] || keystate[SDL_SCANCODE_D]) {
    camera.angle += 0.01;
  }

  // If W (UP) and S (DOWN) key are pressed at the same time, execute the S (DOWN) key
  if ((keystate[SDL_SCANCODE_UP] || keystate[SDL_SCANCODE_W]) && (keystate[SDL_SCANCODE_DOWN] || keystate[SDL_SCANCODE_S])) {
    camera.x -= cos(camera.angle);
    camera.y -= sin(camera.angle);
  }

  // Up & Down
  if (keystate[SDL_SCANCODE_R]) {
    camera.height++;
  }
  if (keystate[SDL_SCANCODE_F]) {
    camera.height--;
  }

  // Pitch
  if (keystate[SDL_SCANCODE_Q]) {
    camera.horizon += 1.5;
  }
  if (keystate[SDL_SCANCODE_E]) {
    camera.horizon -= 1.5;
  }

  // Collision detection
  // Don't fly below the surface
  int map_offset = ((((int)camera.y) & (MAP_SIZE - 1)) << 10) + (((int)camera.x) & (MAP_SIZE - 1)) | 0;
  if ((heightmap[map_offset] + 10) > camera.height) camera.height = heightmap[map_offset] + 10; 
}

// Render things on screen
void render(void) {
 	clear_framebuffer(0xFFE0BB36);

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

  // Pre-calc division
  float inv_screen_width = 1.0 / SCREEN_WIDTH;
  float inv_zfar = 1.0 / camera.zfar;

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
  vec2 pl = {
   .x = cos_angle * camera.zfar + sin_angle * camera.zfar, 
   .y = sin_angle * camera.zfar - cos_angle * camera.zfar
  };

  // Most right point of the camera
  vec2 pr = {
   .x = cos_angle * camera.zfar - sin_angle * camera.zfar,
   .y = sin_angle * camera.zfar + cos_angle * camera.zfar 
  };

  // Loop 320 rays from left to right
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    // Getting "slope" of each ray to cast correctly
    // (pr.x - pl.x) / SCREEN_WIDTH: Length of each small segment
    // * i: Offset by the current ray/column
    // / camera.zfar: Divide by the zfar length to adjust for perpesctive
    vec2 delta = {
     .x = (pl.x + (pr.x - pl.x) * inv_screen_width * i) * inv_zfar,
     .y = (pl.y + (pr.y - pl.y) * inv_screen_width * i) * inv_zfar
    };

    // Position of each ray
    vec2 ray = {
     .x = camera.x,
     .y = camera.y
    };

    // Get max voxel height 
    float max_height = SCREEN_HEIGHT;

    // Depth loop (how far is from camera) 
    for (int z = 1; z < camera.zfar; z++) {
      // "Sloping" each ray
      ray.x += delta.x;
      ray.y += delta.y;

      // Find the offset that we have to go and fetch values from the heightmap and colormap
      int map_offset = ((MAP_SIZE * ((int)ray.y & (MAP_SIZE - 1)) + ((int)ray.x & (MAP_SIZE - 1))));

      // Project heights and find the height on-screen
      int proj_height = (int)((camera.height - heightmap[map_offset]) / z * SCALE_FACTOR + camera.horizon);

      // Only render the terrain pixels if the new projected height is taller than the previous max-height
      if (proj_height < max_height) {
        // Draw pixels from previous max-height until the new projected height
        for (int y = proj_height; y < max_height; y++) {
          // Only draw pixels on the boundareis of map
          if (y >= 0) {
            draw_pixel(i, y, palette[colormap[map_offset]]);
          }
        }
        max_height = proj_height;
      }
    }
  }

  // Render framebuffer to SDL Texture to be displayed
  render_framebuffer();
}

int main(void) {
	is_running = init_window();

  load_map();

  // Game Loop
	while (is_running) {
    process_input();
    render();
  }

	destroy_window();

	return EXIT_SUCCESS;
}
