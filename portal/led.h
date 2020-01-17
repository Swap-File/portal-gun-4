#ifndef _LED_H
#define _LED_H

#include "shared.h"

uint8_t led_update(const struct gun_struct *this_gun);
void led_init(void);
void led_wipe(void);

//CRGB struct borrowed from FastLED
//Representation of an RGB pixel (Red, Green, Blue)

struct CRGB {
	union {
		struct {
            union {
                uint8_t r;
                uint8_t red;
            };
            union {
                uint8_t g;
                uint8_t green;
            };
            union {
                uint8_t b;
                uint8_t blue;
            };
        };
		uint8_t raw[3];
	};
};

#endif