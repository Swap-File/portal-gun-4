#include "BitmapFontClass.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>

int CellX,CellY,YOffset,RowPitch;
char Base;
char Width[256];   
GLuint TexID;
int CurX = 0;
int CurY = 0;
float RowFactor,ColFactor;
int RenderStyle;
float Rd =0;
float Gr = 0;
float Bl =0;
bool InvertYAxis =false;

bool CBitmapFontLoad(char *fname)
 {
  int fileSize;
  char bpp;
  int ImgX,ImgY;

  FILE *f = fopen(fname, "rb");
  fseek(f, 0, SEEK_END);
  fileSize = ftell(f);
  fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

  char *dat = malloc(fileSize);
  fread(dat, 1, fileSize, f);
  fclose(f);
	
  // Check ID is 'BFF2'
  if((unsigned char)dat[0]!=0xBF || (unsigned char)dat[1]!=0xF2)
  {
  free(dat);
  return false;
  }

  // Grab the rest of the header
  memcpy(&ImgX,&dat[2],sizeof(int));
  memcpy(&ImgY,&dat[6],sizeof(int));
  memcpy(&CellX,&dat[10],sizeof(int));
  memcpy(&CellY,&dat[14],sizeof(int));
  bpp=dat[18];
  Base=dat[19];

   // Check filesize
   if (fileSize != ((MAP_DATA_OFFSET)+((ImgX*ImgY)*(bpp/8))) )
	   return false;

  // Calculate font params
  RowPitch=ImgX/CellX;
  ColFactor=(float)CellX/(float)ImgX;
  RowFactor=(float)CellY/(float)ImgY;
  YOffset=CellY;

  // Determine blending options based on BPP
   switch(bpp)
    {
     case 8: // Greyscale
      RenderStyle=BFG_RS_ALPHA;
      break;

     case 24: // RGB
      RenderStyle=BFG_RS_RGB;
      break;

     case 32: // RGBA
      RenderStyle=BFG_RS_RGBA;
      break;

     default: // Unsupported BPP
      free(dat);
      return false;
      break;
    }

  // Allocate space for image
  char * img=malloc((ImgX*ImgY)*(bpp/8));

   if(!img)
    {
     free(dat);
     return false;
    }

  // Grab char widths
  memcpy(Width,&dat[WIDTH_DATA_OFFSET],256);

  // Grab image data
  memcpy(img,&dat[MAP_DATA_OFFSET],(ImgX*ImgY)*(bpp/8));

  // Create Texture
  glGenTextures(1,&TexID);
  glBindTexture(GL_TEXTURE_2D,TexID);
  // Fonts should be rendered at native resolution so no need for texture filtering
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); 
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  // Stop chararcters from bleeding over edges
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

  // Tex creation params are dependent on BPP
   switch(RenderStyle)
    {
     case BFG_RS_ALPHA:
      glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,ImgX,ImgY,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,img);
      break;

     case BFG_RS_RGB:
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,ImgX,ImgY,0,GL_RGB,GL_UNSIGNED_BYTE,img);
      break;

     case BFG_RS_RGBA:
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,ImgX,ImgY,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
      break;
    }

  // Clean up
  free(img);
  free(dat);
  
  return true;
 }

// Set the position for text output, this will be updated as text is printed
void CBitmapFontSetCursor(int x, int y)
 { 
  CurX=x;
  CurY=y;
 }

// The texture ID is a private member, so this function performs the texture bind
void CBitmapFontBind()
 {
  glBindTexture(GL_TEXTURE_2D,TexID);
 }

// Set the color and blending options based on the Renderstyle member
void CBitmapFontSetBlend()
 {
  glColor3f(Rd,Gr,Bl);

    switch(RenderStyle)
     {
      case BFG_RS_ALPHA: // 8Bit
       glBlendFunc(GL_SRC_ALPHA,GL_SRC_ALPHA);
       glEnable(GL_BLEND);
       break;

      case BFG_RS_RGB:   // 24Bit
       glDisable(GL_BLEND);
       break;

      case BFG_RS_RGBA:  // 32Bit
       glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
       glEnable(GL_BLEND);
       break;
     }
 }

// Shortcut, Enables Texturing and performs Bind and SetBlend
void CBitmapFontSelect()
 {
  glEnable(GL_TEXTURE_2D);
  CBitmapFontBind();
  CBitmapFontSetBlend();
 }

// Set the font color NOTE this only sets the polygon color, the texture colors are fixed
void CBitmapFontSetColor(float Red, float Green, float Blue)
 {
  Rd=Red;
  Gr=Green;
  Bl=Blue;
 }

//
void CBitmapFontReverseYAxis(bool State)
 {
   if(State)
    YOffset=-CellY;
   else
    YOffset=CellY;

  InvertYAxis=State;
 }

// Sets up an Ortho screen based on the supplied values 
void CBitmapFontSetScreen(int x, int y)
 {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
   if(InvertYAxis)
    glOrtho(0,x,y,0,-1,1);
   else
    glOrtho(0,x,0,y,-1,1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
 }

// Prints text at the cursor position, cursor is moved to end of text
void CBitmapFontPrint(char* Text)
 {
  int sLen,Loop;
  int Row,Col;
  float U,V,U1,V1;
    
  sLen=(int)strnlen(Text,BFG_MAXSTRING);

  glBegin(GL_QUADS);

   for(Loop=0;Loop!=sLen;++Loop)
    {
     Row=(Text[Loop]-Base)/RowPitch;
     Col=(Text[Loop]-Base)-Row*RowPitch;

     U=Col*ColFactor;
     V=Row*RowFactor;
     U1=U+ColFactor;
     V1=V+RowFactor;

     glTexCoord2f(U, V1); glVertex2i(CurX,      CurY);
     glTexCoord2f(U1,V1); glVertex2i(CurX+CellX,CurY);
     glTexCoord2f(U1,V ); glVertex2i(CurX+CellX,CurY+YOffset);
     glTexCoord2f(U, V ); glVertex2i(CurX,      CurY+YOffset);

     CurX+=Width[(int)Text[Loop]];
    }
   glEnd();

 }

// Prints text at a specifed position, again cursor is updated
void CBitmapFontPrintXY(char* Text, int x, int y)
 {
  int sLen,Loop;
  int Row,Col;
  float U,V,U1,V1;

  CurX=x;
  CurY=y;
    
  sLen=(int)strnlen(Text,BFG_MAXSTRING);

  glBegin(GL_QUADS);

   for(Loop=0;Loop!=sLen;++Loop)
    {
     Row=(Text[Loop]-Base)/RowPitch;
     Col=(Text[Loop]-Base)-Row*RowPitch;

     U=Col*ColFactor;
     V=Row*RowFactor;
     U1=U+ColFactor;
     V1=V+RowFactor;

     glTexCoord2f(U, V1);  glVertex2i(CurX,      CurY);
     glTexCoord2f(U1,V1);  glVertex2i(CurX+CellX,CurY);
     glTexCoord2f(U1,V ); glVertex2i(CurX+CellX,CurY+CellY);
     glTexCoord2f(U, V ); glVertex2i(CurX,      CurY+CellY);

     CurX+=Width[(int)Text[Loop]];
    }
   glEnd();
 }

// Lazy way to draw text.
// Preserves all GL attributes and does everything for you.
// Performance could be an issue.
void CBitmapFontezPrint(char *Text, int x, int y)
 {
  GLint CurMatrixMode;
  GLint ViewPort[4];

  // Save current setup
  glGetIntegerv(GL_MATRIX_MODE,&CurMatrixMode);
  glGetIntegerv(GL_VIEWPORT,ViewPort);
  glPushAttrib(GL_ALL_ATTRIB_BITS);

  // Setup projection
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
   if(InvertYAxis)
    glOrtho(0,ViewPort[2],ViewPort[3],0,-1,1);
   else
    glOrtho(0,ViewPort[2],0,ViewPort[3],-1,1);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(false);

  // Setup Modelview
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // Setup Texture, color and blend options
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,TexID);
  CBitmapFontSetBlend();

  // Render text
  CBitmapFontPrintXY(Text,x,y);

  // Restore previous state
  glPopAttrib();

  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  glMatrixMode(CurMatrixMode);
 }

// Returns the width in pixels of the specified text
int CBitmapFontGetWidth(char* Text)
 {
  int Loop,sLen,Size;

  // How many chars in string?
  sLen=(int)strnlen(Text,BFG_MAXSTRING);

  // Add up all width values
   for(Loop=0,Size=0;Loop!=sLen;++Loop)
    {
     Size+=Width[(int)Text[Loop]];
    }

  return Size;
 }