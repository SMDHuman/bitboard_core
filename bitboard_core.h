//-----------------------------------------------------------------------------
#ifndef BITBOARD_CORE_H
#define BITBOARD_CORE_H

//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
uint32_t bb_colors_rgb[8][2] = {
  {0x000000, 0x000000}, // BB_BLANK
  {0x00007F, 0x0000FF}, // BB_BLUE
  {0x007F00, 0x00FF00}, // BB_GREEN
  {0x007F7F, 0x00FFFF}, // BB_CYAN
  {0x7F0000, 0xFF0000}, // BB_RED
  {0x7F007F, 0xFF00FF}, // BB_MAGENTA
  {0x7F7F00, 0xFFFF00}, // BB_YELLOW
  {0x7F7F7F, 0xFFFFFF}, // BB_WHITE
};

typedef enum{
  BB_BLANK = 0,
  BB_BLUE,
  BB_GREEN,
  BB_CYAN,
  BB_RED,
  BB_MAGENTA,
  BB_YELLOW,
  BB_WHITE,
} BB_COLORS;

#define bb_index_t uint32_t 

typedef struct{
  BB_COLORS  type : 3; // 3 bits for type
  bb_index_t index: sizeof(bb_index_t)*8 - 3; // Remaining bits for index
} bb_pixel_t;

//-----------------------------------------------------------------------------
typedef struct{
  uint16_t width;
  uint16_t height;
  bb_pixel_t* pixel_map;      // pixel map of the circuit [width * height]
  uint8_t* wires_state;       // sizeof (num_wires / 8) bytes
  uint8_t* next_wires_state;  // sizeof (num_wires / 8) bytes
  uint8_t* update_wires_state;  // sizeof (num_wires / 8) bytes
  uint32_t num_wires;         // number of bits used for wires         
  bb_index_t* not_gates[2];   // array of NOT gate input and output wire indices
  uint32_t num_not;           // number of not gates
  bb_index_t* diodes[2];      // array of diode input and output wire indices
  uint32_t num_diodes;        // number of diodes

  // Configuration flags
  void* image;                 // pointer to the original image data
  uint8_t reset_flag : 1;     // flag to reset the circuit
}bitboard_t;

//-----------------------------------------------------------------------------
// This function is for drawing the pixels on the screen while rendering the circuit
extern void bb_draw_pixel(uint16_t x, uint16_t y, uint32_t rgb8, void* image);
// This function is for getting the pixels from circuit image, runs just in initialization
extern uint32_t bb_get_pixel(uint16_t x, uint16_t y, void* image);

//-----------------------------------------------------------------------------
bitboard_t* new_bitboard(void* image, uint16_t width, uint16_t height);
void free_bitboard(bitboard_t* board);
void bb_render(bitboard_t* board, void* image);
void bb_tick(bitboard_t* board);
bool bb_get_wire_at(bitboard_t* board, uint16_t x, uint16_t y);
void bb_set_wire_at(bitboard_t* board, uint16_t x, uint16_t y, bool state);

//-----------------------------------------------------------------------------
static BB_COLORS _bb_get_pixel_color(bitboard_t* board, uint16_t x, uint16_t y);
static bool      _bb_get_pixel_state(bitboard_t* board, uint16_t x, uint16_t y);
static BB_COLORS _bb_get_adjacent_pixel_color(bitboard_t* board, uint16_t x, uint16_t y, uint8_t dir);
static bb_pixel_t* _bb_get_adjacent_pixel_map(bitboard_t* board, uint16_t x, uint16_t y, uint8_t dir);
static void _bb_compile_wires(bitboard_t* board);
static void _bb_weld_crossing_wires(bitboard_t* board);
static void _bb_normalize_wire_indices(bitboard_t* board);
static void _bb_compile_gates(bitboard_t* board);
static void _bb_change_pixels_index(bitboard_t* board, bb_index_t current_index, bb_index_t new_index);
static bool _bb_is_pixel_wire(bb_pixel_t* pixel);
static bool _bb_get_wire_state(bitboard_t* board, bb_index_t index);
// Debug functions
static void _bb_print_pixel_map(bitboard_t* board);
static void _bb_print_gate_connections(bitboard_t* board);

#ifdef BITBOARD_CORE_IMPLEMENTATION
//-----------------------------------------------------------------------------
bitboard_t* new_bitboard(void* image, uint16_t width, uint16_t height){
  bitboard_t* board = (bitboard_t*)calloc(1, sizeof(bitboard_t));
  board->reset_flag = 0;
  board->image = image;
  board->width = width;
  board->height = height;
  //...
  board->pixel_map = (bb_pixel_t*)calloc(width * height, sizeof(bb_pixel_t));
  //...
  _bb_compile_wires(board);
  _bb_weld_crossing_wires(board);
  _bb_normalize_wire_indices(board);
  _bb_compile_gates(board);
  // _bb_print_pixel_map(board);
  // _bb_print_gate_connections(board);
  // printf("Bitboard created with %d wires and %d gates.\n", board->num_wires, board->num_not + board->num_diodes);
  // Allocate memory for wires state
  board->wires_state = (uint8_t*)calloc((board->num_wires + 7) / 8, sizeof(uint8_t));
  board->next_wires_state = (uint8_t*)calloc((board->num_wires + 7) / 8, sizeof(uint8_t));
  board->update_wires_state = (uint8_t*)calloc((board->num_wires + 7) / 8, sizeof(uint8_t));
  // memset(board->wires_state, 1, (board->num_wires + 7) / 8);
  // memset(board->next_wires_state, 1, (board->num_wires + 7) / 8);
  // Initialize the wires state from image pixel values
  for( uint16_t y = 0; y < height; y++){
    for(uint16_t x = 0; x < width; x++){
      if(_bb_is_pixel_wire(&board->pixel_map[y * width + x])){
        bool state = _bb_get_pixel_state(board, x, y);
        bb_set_wire_at(board, x, y, state);
      }
    }
  }
  return board;
}
//-----------------------------------------------------------------------------
void free_bitboard(bitboard_t* board){
  if(board){
    free(board->wires_state);
    free(board->next_wires_state);
    free(board->pixel_map);
    free(board);
  }
}
//-----------------------------------------------------------------------------
void bb_render(bitboard_t* board, void* image){
  for (uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      bb_pixel_t pixel = board->pixel_map[y * board->width + x];
      if(pixel.type != BB_BLANK){
        uint8_t state;
        if(pixel.type == BB_RED || pixel.type == BB_BLUE || pixel.type == BB_GREEN){
          // Gates are always drawn in their original color
          state = 1;
        }else{
          state = _bb_get_wire_state(board, pixel.index);
        }
        uint32_t rgb8 = bb_colors_rgb[pixel.type][state];
        bb_draw_pixel(x, y, rgb8, image);
      }
    }
  }
}

//-----------------------------------------------------------------------------
void bb_tick(bitboard_t* board){
  // Reset the circuit if the reset flag is set
  if(board->reset_flag){
    for (uint32_t i = 0; i < (board->num_wires + 7) / 8; i++){
      board->wires_state[i] = 0;
      board->next_wires_state[i] = 0;
    }
    board->reset_flag = 0;
  }
  for (uint32_t i = 0; i < (board->num_wires + 7) / 8; i++){
    board->next_wires_state[i] = 0;
  }
  // Process NOT gates
  for(uint32_t i = 0; i < board->num_not; i++){
    bb_index_t input_wire = board->not_gates[0][i];
    bb_index_t output_wire = board->not_gates[1][i];
    bool input_state = _bb_get_wire_state(board, input_wire);
    if(!input_state){
      // Input is LOW, output should be HIGH
      board->next_wires_state[(output_wire) / 8] |= (1 << ((output_wire) % 8));
    }
    board->update_wires_state[(output_wire) / 8] |= (1 << ((output_wire) % 8));
  }

  // Process Diodes
  for(uint32_t i = 0; i < board->num_diodes; i++){
    bb_index_t input_wire = board->diodes[0][i];
    bb_index_t output_wire = board->diodes[1][i];
    bool input_state = _bb_get_wire_state(board, input_wire);
    if(input_state){
      // Input is HIGH, output should be HIGH
      board->next_wires_state[(output_wire) / 8] |= (1 << ((output_wire) % 8));
    }
    board->update_wires_state[(output_wire) / 8] |= (1 << ((output_wire) % 8));
  }

  // Update the wires state for the next tick
  for (uint32_t i = 0; i < (board->num_wires + 7) / 8; i++){
    board->wires_state[i] = (board->next_wires_state[i] & board->update_wires_state[i]) | (board->wires_state[i] & ~board->update_wires_state[i]);
  }
}
//-----------------------------------------------------------------------------
bool bb_get_wire_at(bitboard_t* board, uint16_t x, uint16_t y){
  if(x >= board->width || y >= board->height) return false;
  bb_pixel_t pixel = board->pixel_map[y * board->width + x];
  if(_bb_is_pixel_wire(&pixel)){
    return _bb_get_wire_state(board, pixel.index);
  }
  return false;
}
//-----------------------------------------------------------------------------
void bb_set_wire_at(bitboard_t* board, uint16_t x, uint16_t y, bool state){
  if(x >= board->width || y >= board->height) return;
  bb_pixel_t pixel = board->pixel_map[y * board->width + x];
  if(_bb_is_pixel_wire(&pixel)){
    if(state){
      board->wires_state[(pixel.index) / 8] |= (1 << ((pixel.index) % 8));
      board->next_wires_state[(pixel.index) / 8] |= (1 << ((pixel.index) % 8));
    }else{
      board->wires_state[(pixel.index) / 8] &= ~(1 << ((pixel.index) % 8));
      board->next_wires_state[(pixel.index) / 8] &= ~(1 << ((pixel.index) % 8));
    }
  }
}
//-----------------------------------------------------------------------------
static BB_COLORS _bb_get_pixel_color(bitboard_t* board, uint16_t x, uint16_t y){
  uint32_t rgb8 = bb_get_pixel(x, y, board->image);
  uint8_t r = rgb8 >> 16;
  uint8_t g = (rgb8 >> 8) & 0xFF;
  uint8_t b = rgb8 & 0xFF;
  // Find the closest rgb values, 0,127,255
  r = (r < 64) ? 0 : (r < 192) ? 127 : 255;
  g = (g < 64) ? 0 : (g < 192) ? 127 : 255;
  b = (b < 64) ? 0 : (b < 192) ? 127 : 255;
  BB_COLORS color = (BB_COLORS)((b&0x01) | (g&0x02) | (r&0x04));
  return color;
}
//-----------------------------------------------------------------------------
static bool _bb_get_pixel_state(bitboard_t* board, uint16_t x, uint16_t y){
  uint32_t rgb8 = bb_get_pixel(x, y, board->image);
  uint8_t r = rgb8 >> 16;
  uint8_t g = (rgb8 >> 8) & 0xFF;
  uint8_t b = rgb8 & 0xFF;
  if(r == 255 || g == 255 || b == 255){
    return true;
  }
  return false;
}
//-----------------------------------------------------------------------------
static BB_COLORS _bb_get_adjacent_pixel_color(bitboard_t* board, uint16_t x, uint16_t y, uint8_t dir){
  // dir: 0 = up, 1 = right, 2 = down, 3 = left
  if(dir == 0 && y > 0){
    return _bb_get_pixel_color(board, x, y-1);
  }else if(dir == 1 && x < board->width - 1){
    return _bb_get_pixel_color(board, x+1, y);
  }else if(dir == 2 && y < board->height - 1){
    return _bb_get_pixel_color(board, x, y+1);
  }else if(dir == 3 && x > 0){
    return _bb_get_pixel_color(board, x-1, y);
  }
  return BB_BLANK;
}
//-----------------------------------------------------------------------------
static bb_pixel_t* _bb_get_adjacent_pixel_map(bitboard_t* board, uint16_t x, uint16_t y, uint8_t dir){
  // dir: 0 = up, 1 = right, 2 = down, 3 = left
  if(dir == 0 && y > 0){
    return &board->pixel_map[(y-1) * board->width + x];
  }else if(dir == 1 && x < board->width - 1){
    return &board->pixel_map[y * board->width + (x+1)];
  }else if(dir == 2 && y < board->height - 1){
    return &board->pixel_map[(y+1) * board->width + x];
  }else if(dir == 3 && x > 0){
    return &board->pixel_map[y * board->width + (x-1)];
  }
  return NULL;
}
//-----------------------------------------------------------------------------
static void _bb_compile_wires(bitboard_t* board){
  uint32_t wire_count = 1;
  for (uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      BB_COLORS color = _bb_get_pixel_color(board, x, y);
      if(color == BB_WHITE || color == BB_CYAN || color == BB_MAGENTA || color == BB_YELLOW){
        if(board->pixel_map[y * board->width + x].type == 0){
          // This is a wire pixel, assign it a wire index
          bb_pixel_t* pixel = &board->pixel_map[y * board->width + x];
          pixel->type = color; // Wire type
          
          for (uint8_t dir = 0; dir < 4; dir++){
            bb_pixel_t* adj_pixel = _bb_get_adjacent_pixel_map(board, x, y, dir);
            if(adj_pixel){
              if(adj_pixel->type == color){
                if(pixel->index != 0 ){
                  if(pixel->index < adj_pixel->index){
                    _bb_change_pixels_index(board, adj_pixel->index, pixel->index);
                  }else if(pixel->index > adj_pixel->index){
                    _bb_change_pixels_index(board, pixel->index, adj_pixel->index);
                  }
                }else{
                  pixel->index = adj_pixel->index;
                }
              }
            }
          }
          if(pixel->index == 0){
            // This is a new wire, assign it a new index
            pixel->index = wire_count++;
          }
        }
      }
    }
  }
  board->num_wires = wire_count;
}

//-----------------------------------------------------------------------------
static void _bb_weld_crossing_wires(bitboard_t* board){
  for (uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      BB_COLORS color = _bb_get_pixel_color(board, x, y);
      // Wire crossing
      uint8_t is_gate = 0;
      if(color == BB_RED){
        for (uint8_t dir = 0; dir < 4; dir++){
          BB_COLORS adj_color = _bb_get_adjacent_pixel_color(board, x, y, dir);
          if(adj_color == BB_BLUE || adj_color == BB_GREEN){
            is_gate = 1;
            break;
          }
        }
        if(!is_gate){
          for (uint8_t dir = 0; dir < 2; dir++){
            bb_pixel_t* side_a = _bb_get_adjacent_pixel_map(board, x, y, (dir+2)%4);
            bb_pixel_t* side_b = _bb_get_adjacent_pixel_map(board, x, y, dir);
            if(side_a && side_b){
              if(_bb_is_pixel_wire(side_a) && _bb_is_pixel_wire(side_b)){
                if(side_a->index < side_b->index){
                  _bb_change_pixels_index(board, side_b->index, side_a->index);
                }else if(side_a->index > side_b->index){
                  _bb_change_pixels_index(board, side_a->index, side_b->index);
                }
              }
            }
          }
        }
      }
      if(color == BB_BLUE || color == BB_GREEN || (color == BB_RED && is_gate)){
        // Connect all wires around the gate if there is more then one wire
        bb_index_t smallest = (bb_index_t)-1;
        uint8_t wire_count = 0;
        for (uint8_t dir = 0; dir < 4; dir++){
          bb_pixel_t* side = _bb_get_adjacent_pixel_map(board, x, y, dir);
          if(side && _bb_is_pixel_wire(side)){
            if(side->index < smallest){
              smallest = side->index;
            }
            wire_count++;
          }
        }
        if(wire_count > 1){
          for (uint8_t dir = 0; dir < 4; dir++){
            bb_pixel_t* side = _bb_get_adjacent_pixel_map(board, x, y, dir);
            if(side && _bb_is_pixel_wire(side)){
              _bb_change_pixels_index(board, side->index, smallest);
            }
          }
        }
      }
    }
  }
}
//-----------------------------------------------------------------------------
// After welding wires, some indices may be skipped, this function will normalize the indices to be sequential
static void _bb_normalize_wire_indices(bitboard_t* board){
  bb_index_t* index_map = (bb_index_t*)calloc(board->num_wires + 1, sizeof(bb_index_t));
  bb_index_t new_index = 1;
  for (uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      bb_pixel_t* pixel = &board->pixel_map[y * board->width + x];
      if(_bb_is_pixel_wire(pixel)){
        if(index_map[pixel->index] == 0){
          index_map[pixel->index] = new_index++;
        }
        pixel->index = index_map[pixel->index]; 
      }
    }
  }
  free(index_map);
  board->num_wires = new_index;
}
//-----------------------------------------------------------------------------
static void _bb_compile_gates(bitboard_t* board){
  for(uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      BB_COLORS color = _bb_get_pixel_color(board, x, y);
      if(color == BB_BLUE) board->num_not++;
      else if(color == BB_GREEN) board->num_diodes++;
    }
  }
  board->not_gates[0] = (bb_index_t*)calloc(board->num_not, sizeof(bb_index_t));
  board->not_gates[1] = (bb_index_t*)calloc(board->num_not, sizeof(bb_index_t));
  board->diodes[0] = (bb_index_t*)calloc(board->num_diodes, sizeof(bb_index_t));
  board->diodes[1] = (bb_index_t*)calloc(board->num_diodes, sizeof(bb_index_t));

  bb_index_t not_count = 0;
  bb_index_t diode_count = 0;
  for(uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      BB_COLORS color = _bb_get_pixel_color(board, x, y);
      if(color == BB_BLUE || color == BB_GREEN){
        board->pixel_map[y * board->width + x].type = color;
        // Check if its connected to any wire as output
        bb_index_t output_index = 0;
        for (uint8_t dir = 0; dir < 4; dir++){
          bb_pixel_t* adj_pixel = _bb_get_adjacent_pixel_map(board, x, y, dir);
          if(adj_pixel && _bb_is_pixel_wire(adj_pixel)){
            output_index = adj_pixel->index;
            break;
          }
        }
        uint8_t input_found = 0;
        for (uint8_t dir = 0; dir < 4; dir++){
          bb_pixel_t* input_pixel = _bb_get_adjacent_pixel_map(board, x, y, dir);
          // Link to existing input gate
          if(input_pixel->type == BB_RED){
            board->pixel_map[y * board->width + x].index = input_pixel->index;
            if(color == BB_BLUE) board->not_gates[1][input_pixel->index] = output_index;
            else if(color == BB_GREEN) board->diodes[1][input_pixel->index] = output_index;
            input_found = 1;
            break;
          }
        }
        if(!input_found){
          // This is a new NOT gate, assign it a new index
          
          if(color == BB_BLUE) {
            board->pixel_map[y * board->width + x].index = not_count;
            board->not_gates[1][not_count] = output_index; 
            not_count++;
          }
          else if(color == BB_GREEN) {
            board->pixel_map[y * board->width + x].index = diode_count;
            board->diodes[1][diode_count] = output_index;
            diode_count++;
          }
        }
      }
      else if(color == BB_RED){
        board->pixel_map[y * board->width + x].type = BB_RED;
        // Check if this is crossing
        uint8_t gate_found = 0;
        for (uint8_t dir = 0; dir < 4; dir++){
          BB_COLORS adj_color = _bb_get_adjacent_pixel_color(board, x, y, dir);
          if(adj_color == BB_BLUE || adj_color == BB_GREEN) gate_found = 1;
        }
        if(!gate_found) continue; // Not a gate, skip
        // Check if this is connected to any wire as input
        bb_index_t input_index = 0;
        for (uint8_t dir = 0; dir < 4; dir++){
          bb_pixel_t* adj_pixel = _bb_get_adjacent_pixel_map(board, x, y, dir);
          if(adj_pixel && _bb_is_pixel_wire(adj_pixel)){
            input_index = adj_pixel->index;
            break;
          }
        }
        uint8_t output_found = 0;
        for (uint8_t dir = 0; dir < 4; dir++){
          bb_pixel_t* output_pixel = _bb_get_adjacent_pixel_map(board, x, y, dir);
          // Link to existing output gate
          if(output_pixel->type == BB_BLUE){
            board->pixel_map[y * board->width + x].index = output_pixel->index;
            board->not_gates[0][output_pixel->index] = input_index;
            output_found = 1;
            break;
          }
          if(output_pixel->type == BB_GREEN){
            board->pixel_map[y * board->width + x].index = output_pixel->index;
            board->diodes[0][output_pixel->index] = input_index;
            output_found = 1;
            break;
          }
        }
        if(!output_found){
          // This is a new gate input, assign it a new index, check if its a NOT or Diode gate
          uint8_t is_not_gate = 0;
          uint8_t is_diode_gate = 0;
          for (uint8_t dir = 0; dir < 4; dir++){
            BB_COLORS adj_color = _bb_get_adjacent_pixel_color(board, x, y, dir);
            if(adj_color == BB_BLUE){
              is_not_gate = 1;
              break;
            }
            if(adj_color == BB_GREEN){
              is_diode_gate = 1;
              break;
            }
          }
          if(is_not_gate){
            board->pixel_map[y * board->width + x].index = not_count;
            board->not_gates[0][not_count] = input_index;
            not_count++;
          }
          if(is_diode_gate){
            board->pixel_map[y * board->width + x].index = diode_count;
            board->diodes[0][diode_count] = input_index;
            diode_count++;
          }
        }
      }
    }
  }
}
//-----------------------------------------------------------------------------
static void _bb_change_pixels_index(bitboard_t* board, bb_index_t current_index, bb_index_t new_index){
  for (uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      bb_pixel_t* pixel = &board->pixel_map[y * board->width + x];
      if(pixel->index == current_index){
        pixel->index = new_index;
      }
    }
  }
}
//-----------------------------------------------------------------------------
static bool _bb_is_pixel_wire(bb_pixel_t* pixel){
  return (pixel->type == BB_WHITE || pixel->type == BB_CYAN || pixel->type == BB_MAGENTA || pixel->type == BB_YELLOW);
}
//-----------------------------------------------------------------------------
static bool _bb_get_wire_state(bitboard_t* board, bb_index_t index){
  if(index == 0 || index > board->num_wires) return 0;
  return (board->wires_state[(index) / 8] >> ((index) % 8)) & 0x01;
}

//-----------------------------------------------------------------------------
static void _bb_print_pixel_map(bitboard_t* board){
  for (uint16_t y = 0; y < board->height; y++){
    for (uint16_t x = 0; x < board->width; x++){
      bb_pixel_t pixel = board->pixel_map[y * board->width + x];
      if(pixel.type == 0){
        printf("     ");
      } else if(pixel.type == BB_RED) { // Input Gate
        printf("<%3d>", pixel.index);
      } else if(pixel.type == BB_BLUE) { // Not Gate Output
        printf("!%3d!", pixel.index);
      } else if(pixel.type == BB_GREEN) { // Diode Output
        printf("|%3d|", pixel.index);
      }else{ // Wire
        printf(" %3d ", pixel.index);
      } 
    }
    printf("\n");
  }
}
//-----------------------------------------------------------------------------
static void _bb_print_gate_connections(bitboard_t* board){
  printf("NOT Gates:\n");
  for(uint32_t i = 0; i < board->num_not; i++){
    printf("NOT Gate %d: Input Wire %d -> Output Wire %d\n", i, board->not_gates[0][i], board->not_gates[1][i]);
  }
  printf("Diodes:\n");
  for(uint32_t i = 0; i < board->num_diodes; i++){
    printf("Diode %d: Input Wire %d -> Output Wire %d\n", i, board->diodes[0][i], board->diodes[1][i]);
  }
}

//-----------------------------------------------------------------------------
#endif // BITBOARD_CORE_IMPLEMENTATION
#endif // BITBOARD_CORE_H