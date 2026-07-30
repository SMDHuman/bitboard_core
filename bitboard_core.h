#ifndef BITBOARD_CORE_H
#define BITBOARD_CORE_H

#include <stdint.h>
#include <stddef.h>

uint32_t bb_colors_rgb[8][2] = {
  {0x000000, 0x000000}, // BB_BLANK
  {0x880000, 0xFF0000}, // BB_RED
  {0x000088, 0x0000FF}, // BB_BLUE
  {0x008800, 0x00FF00}, // BB_GREEN
  {0x888800, 0xFFFF00}, // BB_YELLOW
  {0x008888, 0x00FFFF}, // BB_CYAN
  {0x880088, 0xFF00FF}, // BB_MAGENTA
  {0x888888, 0xFFFFFF}, // BB_WHITE
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

typedef struct{
  uint8_t type;         // gate type BB_NOT: 0, BB_DIODE: 1
  wire_t** inputs;      // array of input wires (pointers to wire_t)
  uint8_t num_inputs;
  wire_t** outputs;     // array of output wires (pointers to wire_t)
  uint8_t num_outputs;
} gate_t;

typedef struct{
  uint8_t wire_colors; // WIRE_COLORS
  uint8_t state;       // 0 = off, 1 = on
  uint8_t _next_state; // For internal use only, do not modify directly. 0: not updating, 1: update to on, 2: update to off
} wire_t;

typedef struct{
  uint8_t type;       // pixel type: 0 = blank, 1 = wire, 2 = gate output, 3 = gate input
  union{
    wire_t* wire;     // pointer to wire if type == 1
    gate_t* gate;     // pointer to gate if type == 2
  };
} pixel_t;

typedef struct{
  uint16_t width;
  uint16_t height;
  pixel_t* pixel_map;  // pixel map of the circuit [width * height]
  wire_t* wires;       // array of wires, null terminated
  gate_t* gates;       // array of gates, null terminated
}bitboard_t;

extern void bb_put_pixel(uint16_t x, uint16_t y, uint32_t rgb8);

void bb_render(bitboard_t* board);
void bb_tick(bitboard_t* board);

void bb_render(bitboard_t* board){
  for(int y = 0; y < board->height; y++){
    for(int x = 0; x < board->width; x++){
      size_t index = y * board->width + x;
      pixel_t* pixel = &board->pixel_map[index];
      if(pixel->type == 1){ // wire
        wire_t* wire = pixel->wire;
        uint32_t color = bb_colors_rgb[wire->wire_colors][wire->state];
        bb_put_pixel(x, y, color);
      }
      else if(pixel->type == 2){ // gate output
          gate_t* gate = pixel->gate;
          if(gate->type == 0){ // BB_NOT
            bb_put_pixel(x, y, bb_colors_rgb[BB_BLUE][1]);
          }
          else if(gate->type == 1){ // BB_DIODE
            bb_put_pixel(x, y, bb_colors_rgb[BB_GREEN][1]);
          }
        } 
      else if(pixel->type == 3){ // gate input
        bb_put_pixel(x, y, bb_colors_rgb[BB_RED][1]);
      }
      else { // blank
        bb_put_pixel(x, y, bb_colors_rgb[BB_BLANK][0]);
      }
    }
  }
}

void bb_tick(bitboard_t* board){
  for(gate_t* gate = board->gates; gate != NULL; gate++){
    uint8_t output_state = 0;
    if(gate->type == 0){ // BB_NOT
      for(int i = 0; i < gate->num_inputs; i++){
        wire_t* input_wire = gate->inputs[i];
        output_state += input_wire->state;
      }
      output_state = (output_state > 0) ? 0 : 1;
    }
    else if(gate->type == 1){ // BB_DIODE
      for(int i = 0; i < gate->num_inputs; i++){
        wire_t* input_wire = gate->inputs[i];
        output_state += input_wire->state;
      }
      output_state = (output_state > 0) ? 1 : 0;
    }
    for(int i = 0; i < gate->num_outputs; i++){
      wire_t* output_wire = gate->outputs[i];
      output_wire->_next_state = output_state == 1 ? 1 : 2; // 1: update to on, 2: update to off
    }
  }
  // Update wire states
  for(wire_t* wire = board->wires; wire != NULL; wire++){
    if(wire->_next_state == 1){
      wire->state = 1;
    }
    else if(wire->_next_state == 2){
      wire->state = 0;
    }
    wire->_next_state = 0; // Reset next state
  }
}


#endif // BITBOARD_CORE_H