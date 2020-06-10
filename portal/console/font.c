#include <ft2build.h>
#include FT_FREETYPE_H
#include "font.h"
#include "../common/opengl.h"
#include "../common/esUtil.h"

// Maximum texture width
#define MAXWIDTH 1024

static FT_Library ft;
static FT_Face face;

static GLuint text_program;
static GLuint text_vbo;
static GLint text_attribute_coord,rotatematrix;
static GLint text_uniform_tex;
static GLint text_uniform_color;
ESMatrix text_rotation;

struct point {
    GLfloat x;
    GLfloat y;
    GLfloat s;
    GLfloat t;
};

void text_color(GLfloat color[4])
{
    glUseProgram(text_program);
    glUniform4fv(text_uniform_color, 1, color);
}

float font_length(const char *text, struct atlas * a, float sx, float sy)
{
    float x = 0;
    const uint8_t *p;

    /* Loop through all characters */
    for (p = (const uint8_t *)text; *p; p++) {
        /* Calculate the vertex and texture coordinates */
        float w = a->c[*p].bw * sx;
        float h = a->c[*p].bh * sy;
        /* Advance the cursor to the start of the next character */
        x += a->c[*p].ax * sx;
        /* Skip glyphs that have no pixels */
        if (!w || !h)
            continue;
    }
    return x;
}

void font_render(const char *text, struct atlas * a, float x, float y, float sx, float sy)
{
    const uint8_t *p;

    /* Use the texture containing the atlas */
    glBindTexture(GL_TEXTURE_2D, a->tex);
    glUniform1i(text_uniform_tex, 0);

    /* Set up the text_vbo for our vertex data */
    glEnableVertexAttribArray(text_attribute_coord);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glVertexAttribPointer(text_attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glUniformMatrix4fv(rotatematrix, 1, GL_FALSE, &text_rotation.m[0][0]);

    struct point coords[6 * strlen(text)];
    int c = 0;

    /* Loop through all characters */
    for (p = (const uint8_t *)text; *p; p++) {
        /* Calculate the vertex and texture coordinates */
        float x2 = x + a->c[*p].bl * sx;
        float y2 = -y - a->c[*p].bt * sy;
        float w = a->c[*p].bw * sx;
        float h = a->c[*p].bh * sy;

        /* Advance the cursor to the start of the next character */
        x += a->c[*p].ax * sx;
        y += a->c[*p].ay * sy;

        /* Skip glyphs that have no pixels */
        if (!w || !h)
            continue;

        coords[c++] = (struct point) {
            x2, -y2, a->c[*p].tx, a->c[*p].ty
        };
        coords[c++] = (struct point) {
            x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty
        };
        coords[c++] = (struct point) {
            x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h
        };
        coords[c++] = (struct point) {
            x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty
        };
        coords[c++] = (struct point) {
            x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h
        };
        coords[c++] = (struct point) {
            x2 + w, -y2 - h, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty + a->c[*p].bh / a->h
        };
    }

    /* Draw all the character on the screen in one go */
    glBufferData(GL_ARRAY_BUFFER, sizeof coords, coords, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, c);

    glDisableVertexAttribArray(text_attribute_coord);
}


void font_atlas_init(int height,struct atlas *input)
{
    FT_Set_Pixel_Sizes(face, 0, height);
    FT_GlyphSlot g = face->glyph;
    unsigned int roww = 0;
    unsigned int rowh = 0;
    input->w = 0;
    input->h = 0;

    memset(input->c, 0, sizeof (input->c));

    /* Find minimum size for a texture holding all visible ASCII characters */
    for (int i = 32; i < 128; i++) {
        if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
            fprintf(stderr, "Loading character %c failed!\n", i);
            continue;
        }
        if (roww + g->bitmap.width + 1 >= MAXWIDTH) {
            input->w = MAX2(input->w, roww);
            input->h += rowh;
            roww = 0;
            rowh = 0;
        }
        roww += g->bitmap.width + 1;
        rowh = MAX2(rowh, g->bitmap.rows);
    }

    input->w = MAX2(input->w, roww);
    input->h += rowh;

    /* Create a texture that will be used to hold all ASCII glyphs */
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &(input->tex));
    glBindTexture(GL_TEXTURE_2D, input->tex);
    glUniform1i(text_uniform_tex, 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, input->w,input->h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);

    /* We require 1 byte alignment when uploading texture data */
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /* Clamping to edges is important to prevent artifacts when scaling */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Linear filtering usually looks best for text */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Paste all glyph bitmaps into the texture, remembering the offset */
    int ox = 0;
    int oy = 0;

    rowh = 0;

    for (int i = 32; i < 128; i++) {
        if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
            fprintf(stderr, "Loading character %c failed!\n", i);
            continue;
        }

        if (ox + g->bitmap.width + 1 >= MAXWIDTH) {
            oy += rowh;
            rowh = 0;
            ox = 0;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, g->bitmap.width, g->bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
        input->c[i].ax = g->advance.x >> 6;
        input->c[i].ay = g->advance.y >> 6;

        input->c[i].bw = g->bitmap.width;
        input->c[i].bh = g->bitmap.rows;

        input->c[i].bl = g->bitmap_left;
        input->c[i].bt = g->bitmap_top;

        input->c[i].tx = ox / (float)(input->w);
        input->c[i].ty = oy / (float)(input->h);

        rowh = MAX2(rowh, g->bitmap.rows);
        ox += g->bitmap.width + 1;
    }

    printf("Generated a %d x %d (%d kb) texture atlas\n", input->w, input->h, input->w * input->h / 1024);
};


int font_init(char *fontfilename ) {
    /* Initialize the FreeType2 library */
    if (FT_Init_FreeType(&ft)) {
        fprintf(stderr, "Could not init freetype library\n");
        return 0;
    }

    /* Load a font */
    if (FT_New_Face(ft, fontfilename, 0, &face)) {
        fprintf(stderr, "Could not open font %s\n", fontfilename);
        return 0;
    }

    text_program = create_program_from_disk("/home/pi/portal/console/font.vert","/home/pi/portal/console/font.frag");
    link_program(text_program);

    rotatematrix = glGetUniformLocation(text_program, "rotatematrix");
    text_attribute_coord = glGetAttribLocation(text_program, "coord");
    text_uniform_tex = glGetUniformLocation(text_program, "tex");
    text_uniform_color = glGetUniformLocation(text_program, "color");


    if(rotatematrix == -1 || text_attribute_coord == -1 || text_uniform_tex == -1 || text_uniform_color == -1)
        return 0;

    // Create the vertex buffer object
    glGenBuffers(1, &text_vbo);

    esMatrixLoadIdentity(&text_rotation);
    esRotate(&text_rotation, 270.0f, 0.0f, 0.0f,1.0f);

    return 1;
}