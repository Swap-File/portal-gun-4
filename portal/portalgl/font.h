#ifndef _FONT_H
#define _FONT_H

#include <stdbool.h>

#define BFG_RS_NONE  0x0      // Blend flags
#define BFG_RS_ALPHA 0x1
#define BFG_RS_RGB   0x2
#define BFG_RS_RGBA  0x4

#define BFG_MAXSTRING 255     // Maximum string length

#define WIDTH_DATA_OFFSET  20 // Offset to width data with BFF file
#define MAP_DATA_OFFSET   276 // Offset to texture image data with BFF file

bool font_Load(char *fname);
void font_SetScreen(int x, int y); 
void font_SetCursor(int x, int y); 
void font_SetColor(float Red, float Green, float Blue);
void font_ReverseYAxis(bool State);
void font_Select();
void font_Bind();
void font_SetBlend();
void font_Print(char *Text);
void font_PrintXY(char *Text, int x, int y);
void font_ezPrint(char *Text, int x, int y);
int font_GetWidth(char *Text);

#endif