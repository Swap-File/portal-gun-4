#ifndef _SCENE_H
#define _SCENE_H

void scene_init(void);
void scene_animate(int acceleration[], int frame);
void scene_redraw(GLuint gst_shared_texture, int frame, bool preview);

#endif
