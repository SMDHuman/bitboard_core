//-----------------------------------------------------------------------------
#ifndef BITBOARD_CORE_H
#define BITBOARD_CORE_H

//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

//-----------------------------------------------------------------------------
uint32_t bb_colors_rgb[8][2] = {
  {0x000000, 0x000000}, // BB_BLANK
  {0x7F0000, 0xFF0000}, // BB_RED
  {0x00007F, 0x0000FF}, // BB_BLUE
  {0x007F00, 0x00FF00}, // BB_GREEN
  {0x7F7F00, 0xFFFF00}, // BB_YELLOW
  {0x007F7F, 0x00FFFF}, // BB_CYAN
  {0x7F007F, 0xFF00FF}, // BB_MAGENTA
  {0x7F7F7F, 0xFFFFFF}, // BB_WHITE
};

typedef enum{
  BB_BLANK = 0,
  BB_RED = 1,
  BB_BLUE = 2,
  BB_GREEN = 3,
  BB_YELLOW = 4,
  BB_CYAN = 5,
  BB_MAGENTA = 6,
  BB_WHITE = 7,
} WIRE_COLORS;

//-----------------------------------------------------------------------------
typedef struct{
  uint8_t wire_colors : 3; // WIRE_COLORS
  uint8_t state       : 1; // 0 = off, 1 = on
  uint8_t _next_state : 2; // For internal use only, do not modify directly. 0: not updating, 1: update to on, 2: update to off
} bb_wire_t;

typedef struct{
  enum{BB_NOT, BB_DIODE} type : 2; // gate type BB_NOT: 0, BB_DIODE: 1
  uint8_t num_inputs          : 3;
  uint8_t num_outputs         : 3;
  bb_wire_t** inputs;              // array of input wires (pointers to wire_t)
  bb_wire_t** outputs;             // array of output wires (pointers to wire_t)
} bb_gate_t;

typedef struct{
  enum{BB_PIX_BLANK, BB_PIX_WIRE, BB_PIX_GATE_OUT, BB_PIX_GATE_IN} type; // pixel type: 0 = blank, 1 = wire, 2 = gate output, 3 = gate input
  union{
    bb_wire_t* wire;          // pointer to wire if type == 1
    bb_gate_t* gate;          // pointer to gate if type == 2
  };
} bb_pixel_t;

typedef struct{
  uint16_t width;
  uint16_t height;
  bb_pixel_t* pixel_map;      // pixel map of the circuit [width * height]
  bb_wire_t* wires;           // array of wires, null terminated
  uint32_t num_wires;         // number of wires
  bb_gate_t* gates;           // array of gates, null terminated
  uint32_t num_gates;         // number of gates
  uint8_t reset_flag : 1;     // flag to reset the circuit
}bitboard_t;

typedef struct {
    uint8_t *data;          // image raw rgb8 data
    int width;              // image base width
    int height;             // image base height
} bb_image_t;

//-----------------------------------------------------------------------------
extern void bb_put_pixel(uint16_t x, uint16_t y, uint32_t rgb8);

//-----------------------------------------------------------------------------
bitboard_t* new_bitboard(bb_image_t* image);
void free_bitboard(bitboard_t* board);
void bb_render(bitboard_t* board);
void bb_tick(bitboard_t* board);

//-----------------------------------------------------------------------------
static void _bb_preprocess_image(bb_image_t* image);
static WIRE_COLORS _bb_get_wire_color(uint32_t rgb8);
static void _bb_append_wire(bitboard_t* board, bb_wire_t* wire);
static void _bb_append_gate(bitboard_t* board, bb_gate_t* gate);
static void _bb_append_input_to_gate(bb_gate_t* gate, bb_wire_t* input_wire);
static void _bb_append_output_to_gate(bb_gate_t* gate, bb_wire_t* output_wire);
static bb_wire_t* _bb_peek_wire(bitboard_t* board, uint16_t x, uint16_t y);
static bb_wire_t* _bb_peek_adjacent_wires(bitboard_t* board, uint16_t x, uint16_t y, WIRE_COLORS color);
static bb_gate_t* _bb_peek_adjacent_gates(bitboard_t* board, uint16_t x, uint16_t y);

#ifdef BITBOARD_CORE_IMPLEMENTATION
//-----------------------------------------------------------------------------
bitboard_t* new_bitboard(bb_image_t* image){
  bitboard_t* board = (bitboard_t*)calloc(1, sizeof(bitboard_t));
  board->width = image->width;
  board->height = image->height;
  board->reset_flag = 1;
  //...
  _bb_preprocess_image(image);
  //... 
  board->pixel_map = (bb_pixel_t*)calloc(image->width * image->height, sizeof(bb_pixel_t));
  for(int y = 0; y < image->height; y++){
    for(int x = 0; x < image->width; x++){
      size_t index = y * image->width + x;
      size_t index_rgb = index * 3; // 3 bytes per pixel (RGB)
      uint32_t rgb = (image->data[index_rgb] << 16) | (image->data[index_rgb + 1] << 8) | image->data[index_rgb + 2];
      WIRE_COLORS color_index = _bb_get_wire_color(rgb);
      switch(color_index){
        case BB_BLANK:
          board->pixel_map[index].type = BB_PIX_BLANK;
          break;
        case BB_RED:
        case BB_BLUE:
        case BB_GREEN:{
          board->pixel_map[index].type = color_index == BB_RED ? BB_PIX_GATE_IN : BB_PIX_GATE_OUT;
          bb_gate_t* gate = _bb_peek_adjacent_gates(board, x, y);
          if(gate == NULL){
            bb_gate_t new_gate = {0};
            _bb_append_gate(board, &new_gate);
            gate = &board->gates[board->num_gates - 1];
          }
          if(color_index == BB_BLUE) gate->type = BB_NOT;
          else if(color_index == BB_GREEN) gate->type = BB_DIODE;
          board->pixel_map[index].gate = gate;
        }break;
        case BB_YELLOW:
        case BB_CYAN:
        case BB_MAGENTA:
        case BB_WHITE: {
          board->pixel_map[index].type = BB_PIX_WIRE;
          bb_wire_t* wire = _bb_peek_adjacent_wires(board, x, y, color_index);
          if(wire != NULL){
            board->pixel_map[index].wire = wire;
          }
          else{
            bb_wire_t new_wire = { .wire_colors = color_index, .state = (rgb == bb_colors_rgb[color_index][1]) ? 1 : 0, ._next_state = 0 };
            _bb_append_wire(board, &new_wire);
            board->pixel_map[index].wire = &board->wires[board->num_wires - 1];
          }
        }break;
      }
    }
  }

  // Link wires to gates
  for(int y = 0; y < image->height; y++){
    for(int x = 0; x < image->width; x++){
      size_t index = y * image->width + x;
      if(board->pixel_map[index].type == BB_PIX_GATE_OUT){
        bb_gate_t* gate = board->pixel_map[index].gate;
        int sides[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // Up, Down, Left, Right
        for(int i = 0; i < 4; i++){
          int adj_x = x + sides[i][0];
          int adj_y = y + sides[i][1];
          bb_wire_t* wire = _bb_peek_wire(board, adj_x, adj_y);
          if(wire != NULL){
            printf("Linking gate output at (%d, %d) to wire at (%d, %d)\n", x, y, adj_x, adj_y);
            _bb_append_output_to_gate(gate, wire);
          }
        }
      }
      else if(board->pixel_map[index].type == BB_PIX_GATE_IN){
        bb_gate_t* gate = board->pixel_map[index].gate;
        int sides[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}}; // Up, Down, Left, Right
        for(int i = 0; i < 4; i++){
          int adj_x = x + sides[i][0];
          int adj_y = y + sides[i][1];
          bb_wire_t* wire = _bb_peek_wire(board, adj_x, adj_y);
          if(wire != NULL){
            printf("Linking gate input at (%d, %d) to wire at (%d, %d)\n", x, y, adj_x, adj_y);
            _bb_append_input_to_gate(gate, wire);
          }
        }
      }
    }
  }
  return board;
}
//-----------------------------------------------------------------------------
void free_bitboard(bitboard_t* board){
  if(board){
    free(board);
  }
}
//-----------------------------------------------------------------------------
void bb_render(bitboard_t* board){
  for(int y = 0; y < board->height; y++){
    for(int x = 0; x < board->width; x++){
      size_t index = y * board->width + x;
      bb_pixel_t* pixel = &board->pixel_map[index];
      if(pixel->type == BB_PIX_WIRE){ // wire
        bb_wire_t* wire = pixel->wire;
        uint32_t color = bb_colors_rgb[wire->wire_colors][wire->state];
        bb_put_pixel(x, y, color);
      }
      else if(pixel->type == BB_PIX_GATE_OUT){ // gate output
          bb_gate_t* gate = pixel->gate;
          if(gate->type == 0){ // BB_NOT
            bb_put_pixel(x, y, bb_colors_rgb[BB_BLUE][1]);
          }
          else if(gate->type == 1){ // BB_DIODE
            bb_put_pixel(x, y, bb_colors_rgb[BB_GREEN][1]);
          }
        } 
      else if(pixel->type == BB_PIX_GATE_IN){ // gate input
        bb_put_pixel(x, y, bb_colors_rgb[BB_RED][1]);
      }
      else { // blank
        bb_put_pixel(x, y, bb_colors_rgb[BB_BLANK][0]);
      }
    }
  }
}

//-----------------------------------------------------------------------------
void bb_tick(bitboard_t* board){
  for(int i = 0; i < board->num_gates; i++){
    bb_gate_t* gate = &board->gates[i];
    uint8_t output_state = 0;
    if(gate->type == 0){ // BB_NOT
      for(int j = 0; j < gate->num_inputs; j++){
        bb_wire_t* input_wire = gate->inputs[j];
        output_state += input_wire->state;
      }
      output_state = (output_state > 0) ? 0 : 1;
    }
    else if(gate->type == 1){ // BB_DIODE
      for(int j = 0; j < gate->num_inputs; j++){
        bb_wire_t* input_wire = gate->inputs[j];
        output_state += input_wire->state;
      }
      output_state = (output_state > 0) ? 1 : 0;
    }
    for(int j = 0; j < gate->num_outputs; j++){
      bb_wire_t* output_wire = gate->outputs[j];
      output_wire->_next_state = output_state == 1 ? 1 : 2; // 1: update to on, 2: update to off
    }
  }
  // Update wire states
  for(int i = 0; i < board->num_wires; i++){
    bb_wire_t* wire = &board->wires[i];
    if(wire->_next_state == 1){
      wire->state = 1;
    }
    else if(wire->_next_state == 2){
      wire->state = 0;
    }
    else if(board->reset_flag){
      wire->state = 0; // Reset all wires to off
    }
    wire->_next_state = 0; // Reset next state
  }
  if(board->reset_flag) board->reset_flag = 0; // Clear reset flag after processing
}
//-----------------------------------------------------------------------------
static void _bb_preprocess_image(bb_image_t* image){
  // Adjust the colors to nearest expected color match.
  for(int y = 0; y < image->height; y++){
    for(int x = 0; x < image->width; x++){
      size_t index = (y * image->width + x) * 3; // 3 bytes per pixel (RGB)s
      uint8_t r = image->data[index];
      uint8_t g = image->data[index + 1];
      uint8_t b = image->data[index + 2];
      // Find the closest rgb values, 0,127,255
      r = (r < 64) ? 0 : (r < 192) ? 127 : 255;
      g = (g < 64) ? 0 : (g < 192) ? 127 : 255;
      b = (b < 64) ? 0 : (b < 192) ? 127 : 255;
      image->data[index] = r;
      image->data[index + 1] = g;
      image->data[index + 2] = b;
    }
  }
}

static WIRE_COLORS _bb_get_wire_color(uint32_t rgb8){
  for(int i = 0; i < 8; i++){
    if(bb_colors_rgb[i][1] == rgb8 || bb_colors_rgb[i][0] == rgb8){
      return (WIRE_COLORS)i;
    }
  }
  return BB_BLANK; // Default to blank if no match
}
//-----------------------------------------------------------------------------
static void _bb_append_wire(bitboard_t* board, bb_wire_t* wire){
  if(board->wires == NULL){
    board->wires = calloc(1, sizeof(bb_wire_t));
    board->wires[0] = *wire;
    board->num_wires = 1;
  }
  else{
    board->wires = realloc(board->wires, (board->num_wires + 1) * sizeof(bb_wire_t));
    if(wire != NULL) board->wires[board->num_wires] = *wire;
    else board->wires[board->num_wires] = (bb_wire_t){0}; // Null terminate the wires array
    board->num_wires++;
  }
}
//-----------------------------------------------------------------------------
static void _bb_append_gate(bitboard_t* board, bb_gate_t* gate){
  if(board->gates == NULL){
    board->gates = calloc(1, sizeof(bb_gate_t));
    board->gates[0] = *gate;
    board->num_gates = 1;
  }
  else{
    board->gates = realloc(board->gates, (board->num_gates + 1) * sizeof(bb_gate_t));
    if(gate != NULL) board->gates[board->num_gates] = *gate;
    else board->gates[board->num_gates] = (bb_gate_t){0}; // Null terminate the gates array
    board->num_gates++;
  }
}
//-----------------------------------------------------------------------------
static void _bb_append_input_to_gate(bb_gate_t* gate, bb_wire_t* input_wire){
  if(gate->inputs == NULL){
    gate->inputs = calloc(1, sizeof(bb_wire_t*));
    gate->inputs[0] = input_wire;
    gate->num_inputs = 1;
  }
  else{
    gate->inputs = realloc(gate->inputs, (gate->num_inputs + 1) * sizeof(bb_wire_t*));
    gate->inputs[gate->num_inputs] = input_wire;
    gate->num_inputs++;
  }
}
//-----------------------------------------------------------------------------
static void _bb_append_output_to_gate(bb_gate_t* gate, bb_wire_t* output_wire){
  if(gate->outputs == NULL){
    gate->outputs = calloc(1, sizeof(bb_wire_t*));
    gate->outputs[0] = output_wire;
    gate->num_outputs = 1;
  }
  else{
    gate->outputs = realloc(gate->outputs, (gate->num_outputs + 1) * sizeof(bb_wire_t*));
    gate->outputs[gate->num_outputs] = output_wire;
    gate->num_outputs++;
  }
}
//-----------------------------------------------------------------------------
static bb_wire_t* _bb_peek_wire(bitboard_t* board, uint16_t x, uint16_t y){
  if(x >= board->width || y >= board->height){
    return NULL; // Out of bounds
  }
  bb_pixel_t* pixel = &board->pixel_map[y * board->width + x];
  if(pixel->type == BB_PIX_WIRE){
    return pixel->wire;
  }
  return NULL; // Not a wire
}
//-----------------------------------------------------------------------------
static bb_wire_t* _bb_peek_adjacent_wires(bitboard_t* board, uint16_t x, uint16_t y, WIRE_COLORS color){
  bb_wire_t* adjacent_wires[4] = {NULL, NULL, NULL, NULL}; // Up, Down, Left, Right
  if(y > 0){ // Up
    bb_pixel_t* pixel = &board->pixel_map[(y - 1) * board->width + x];
    if(pixel->type == 1 && pixel->wire->wire_colors == color){
      adjacent_wires[0] = pixel->wire;
    }
  }
  if(y < board->height - 1){ // Down
    bb_pixel_t* pixel = &board->pixel_map[(y + 1) * board->width + x];
    if(pixel->type == 1 && pixel->wire->wire_colors == color){
      adjacent_wires[1] = pixel->wire;
    }
  }
  if(x > 0){ // Left
    bb_pixel_t* pixel = &board->pixel_map[y * board->width + (x - 1)];
    if(pixel->type == 1 && pixel->wire->wire_colors == color){
      adjacent_wires[2] = pixel->wire;
    }
  }
  if(x < board->width - 1){ // Right
    bb_pixel_t* pixel = &board->pixel_map[y * board->width + (x + 1)];
    if(pixel->type == 1 && pixel->wire->wire_colors == color){
      adjacent_wires[3] = pixel->wire;
    }
  }
  
  for(int i = 0; i < 4; i++){
    if(adjacent_wires[i] != NULL){
      return adjacent_wires[i];
    }
  }
  
  return NULL; // No adjacent wire found

}
//-----------------------------------------------------------------------------
static bb_gate_t* _bb_peek_adjacent_gates(bitboard_t* board, uint16_t x, uint16_t y){
  bb_gate_t* adjacent_gates[4] = {NULL, NULL, NULL, NULL}; // Up, Down, Left, Right
  if(y > 0){ // Up
    bb_pixel_t* pixel = &board->pixel_map[(y - 1) * board->width + x];
    if(pixel->type == BB_PIX_GATE_OUT || pixel->type == BB_PIX_GATE_IN){
      adjacent_gates[0] = pixel->gate;
    }
  }
  if(y < board->height - 1){ // Down
    bb_pixel_t* pixel = &board->pixel_map[(y + 1) * board->width + x];
    if(pixel->type == BB_PIX_GATE_OUT || pixel->type == BB_PIX_GATE_IN){
      adjacent_gates[1] = pixel->gate;
    }
  }
  if(x > 0){ // Left
    bb_pixel_t* pixel = &board->pixel_map[y * board->width + (x - 1)];
    if(pixel->type == BB_PIX_GATE_OUT || pixel->type == BB_PIX_GATE_IN){
      adjacent_gates[2] = pixel->gate;
    }
  }
  if(x < board->width - 1){ // Right
    bb_pixel_t* pixel = &board->pixel_map[y * board->width + (x + 1)];
    if(pixel->type == BB_PIX_GATE_OUT || pixel->type == BB_PIX_GATE_IN){
      adjacent_gates[3] = pixel->gate;
    }
  }
  
  for(int i = 0; i < 4; i++){
    if(adjacent_gates[i] != NULL){
      return adjacent_gates[i];
    }
  }
  
  return NULL; // No adjacent gate found
}
//-----------------------------------------------------------------------------
#endif // BITBOARD_CORE_IMPLEMENTATION
#endif // BITBOARD_CORE_H