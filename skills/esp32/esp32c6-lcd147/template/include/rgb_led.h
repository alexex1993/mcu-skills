// The single WS2812-family RGB LED on GPIO8, driven over RMT.
//
// GPIO8 is a strapping pin, but only for "chip boot mode" (consulted only when
// GPIO9 is low, i.e. while the BOOT button is held) and for UART0 ROM message
// printing (consulted only when EFUSE_UART_PRINT_CONTROL is non-zero, and its
// factory default is 0). At factory eFuse settings this LED is free to use.
#pragma once

#include <stdint.h>

// Claims an RMT TX channel and its WS2812 byte encoder. Call once.
void rgb_led_init(void);

// Sets the LED and blocks until the bits are on the wire. Values are 0-255 in
// ordinary RGB order; the GRB wire order is handled internally.
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);
