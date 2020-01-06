#ifndef _BOARD_MODEL_H
#define _BOARD_MODEL_H

void model_board_init(void);
void model_board_animate(int acceleration[], int frame);
void model_board_redraw(GLuint gst_shared_texture, int frame);
#endif
