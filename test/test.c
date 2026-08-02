//-----------------------------------------------------------------------------
// This is a test file for the bitboard_core library. It uses the raylib library
// to create a window and render the bitboard. The user can click on the wires
// to toggle their state. 
// Key bindings:
//   - Space: Tick the circuit (update the wires state)
//   - R: Reset the circuit to its initial state
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <raylib.h>

#define BITBOARD_CORE_IMPLEMENTATION
#include "../bitboard_core.h"

//-----------------------------------------------------------------------------
void bb_draw_pixel(uint16_t x, uint16_t y, uint32_t rgb8, void* image){
  // For testing purposes, just print the pixel coordinates and color
  Color color = GetColor(rgb8 << 8 | 0xFF); // Shift color to include alpha channel
  DrawRectangle(x*10, y*10, 10, 10, color); 
}
uint32_t bb_get_pixel(uint16_t x, uint16_t y, void* image){
  Image* ray_image = (Image*)image;
  Color pixel_color = GetImageColor(*ray_image, x, y);
  return (pixel_color.r << 16) | (pixel_color.g << 8) | pixel_color.b;
}

//-----------------------------------------------------------------------------
int main(){

  Image ray_image = LoadImage("circuits/4BitClock.png");
  InitWindow(ray_image.width * 10, ray_image.height * 10, "Bitboard Core Test");
  bitboard_t* board = new_bitboard(&ray_image, ray_image.width, ray_image.height);
  SetTargetFPS(30);
  
  while (!WindowShouldClose()){
    BeginDrawing();
    ClearBackground(BLACK);
    
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
      int mouse_x = GetMouseX() / 10;
      int mouse_y = GetMouseY() / 10;
      if(mouse_x < board->width && mouse_y < board->height){
        bool current_state = bb_get_wire_at(board, mouse_x, mouse_y);
        bb_set_wire_at(board, mouse_x, mouse_y, !current_state);
      }
    }

    // if(IsKeyPressed(KEY_SPACE)){
    //   bb_tick(board);
    // }
    if(IsKeyPressed(KEY_R)){
      board->reset_flag = 1;
    }

    bb_render(board, 0);
    bb_tick(board);
    
    EndDrawing();
  }

  free_bitboard(board);
  return 0;
}
//-----------------------------------------------------------------------------
