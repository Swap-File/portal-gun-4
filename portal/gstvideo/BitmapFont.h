#ifndef _BITMAPFONTCLASS_H
#define _BITMAPFONTCLASS_H

#include <stdbool.h>

#define BFG_RS_NONE  0x0      // Blend flags
#define BFG_RS_ALPHA 0x1
#define BFG_RS_RGB   0x2
#define BFG_RS_RGBA  0x4

#define BFG_MAXSTRING 255     // Maximum string length

#define WIDTH_DATA_OFFSET  20 // Offset to width data with BFF file
#define MAP_DATA_OFFSET   276 // Offset to texture image data with BFF file

// This definition is missing from some GL libs
#ifndef GL_CLAMP_TO_EDGE 
#define GL_CLAMP_TO_EDGE 0x812F 
#endif

typedef struct {
  unsigned char ID1,ID2;
  unsigned char BPP;
  int ImageWidth,ImageHeight,CellWidth,CellHeight;
  unsigned char StartPoint;
 } FontFileHeader;

bool BitmapFontLoad(char *fname);
void BitmapFontSetScreen(int x, int y); 
void BitmapFontSetCursor(int x, int y); 
void BitmapFontSetColor(float Red, float Green, float Blue);
void BitmapFontReverseYAxis(bool State);
void BitmapFontSelect();                
void BitmapFontBind();                  
void BitmapFontSetBlend();              
void BitmapFontPrint(char *Text);       
void BitmapFontPrintXY(char *Text, int x, int y); 
void BitmapFontezPrint(char *Text, int x, int y);
int  BitmapFontGetWidth(char *Text);

#endif