#ifndef _FONT_H
#define _FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include "../common/esUtil.h"

/**
 * The atlas struct holds a texture that contains the visible US-ASCII characters
 * of a certain font rendered with a certain character height.
 * It also contains an array that contains all the information necessary to
 * generate the appropriate vertex and texture coordinates for each character.
 *
 * After the constructor is run, you don't need to use any FreeType functions anymore.
 */
struct atlas {
    GLuint tex;		// texture object

    unsigned int w;			// width of texture in pixels
    unsigned int h;			// height of texture in pixels

    struct {
        float ax;	// advance.x
        float ay;	// advance.y

        float bw;	// bitmap.width;
        float bh;	// bitmap.height;

        float bl;	// bitmap_left;
        float bt;	// bitmap_top;

        float tx;	// x offset of glyph in texture coordinates
        float ty;	// y offset of glyph in texture coordinates
    } c[128];
};
float font_length(const char *text, struct atlas * a, float sx, float sy);
int font_init(char *fontfilename);
void text_color(GLfloat color[4]);
void font_atlas_init(int height,struct atlas *input);
void font_render(const char *text,struct atlas *a, float x, float y, float sx, float sy);

#endif