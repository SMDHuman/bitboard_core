#include <stdio.h>
#include <raylib.h>

#define BITBOARD_CORE_IMPLEMENTATION
#include "bitboard_core.h"

void bb_put_pixel(uint16_t x, uint16_t y, uint32_t rgb8){
  // For testing purposes, just print the pixel coordinates and color
  DrawRectangle(x*10, y*10, 10, 10, (Color){(rgb8 >> 16) & 0xFF, (rgb8 >> 8) & 0xFF, rgb8 & 0xFF, 255});
}


int main(){
  InitWindow(320, 320, "Bitboard Core Test");

  Image ray_image = LoadImage("circuits/test.png");
  bb_image_t image = { .width = ray_image.width, .height = ray_image.height };
  image.data = (uint8_t*)malloc(ray_image.width * ray_image.height * 3); // 3 bytes per pixel (RGB)
  for(int y = 0; y < ray_image.height; y++){
    for(int x = 0; x < ray_image.width; x++){
      Color pixel_color = GetImageColor(ray_image, x, y);
      size_t index = (y * ray_image.width + x) * 3;
      image.data[index] = pixel_color.r;
      image.data[index + 1] = pixel_color.g;
      image.data[index + 2] = pixel_color.b;
    }
  }
  bitboard_t* board = new_bitboard(&image);
  printf("Bitboard created with %d wires and %d gates.\n", board->num_wires, board->num_gates);
  SetTargetFPS(30);
  
  while (!WindowShouldClose()){
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    bb_render(board);
    bb_tick(board);
    
    EndDrawing();
  }

  free_bitboard(board);
  return 0;
}

