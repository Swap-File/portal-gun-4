#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <sys/select.h>
#include "../common/opengl.h"
#include "console_scene.h"

static const struct egl *egl;
static const struct gbm *gbm;
static const struct drm *drm;

int main(int argc, char *argv[])
{
	printf("Starting Console...\n");

    const char *device = "/dev/dri/card1";
    char mode_str[DRM_DISPLAY_MODE_LEN] = "640x480";
    uint32_t format = DRM_FORMAT_XRGB8888;
    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    int samples = 0;
    unsigned int vrefresh = 0;

	drm = init_drm_legacy(device, mode_str, vrefresh, 1);
	
    if (!drm ) {
        printf("failed to initialize DRM\n");
        system("sudo reboot");
        return -1;
    }

    gbm = init_gbm(drm->fd, drm->mode->hdisplay, drm->mode->vdisplay,format, modifier);
    if (!gbm) {
        printf("failed to initialize GBM\n");
        return -1;
    }

    egl = scene_init(gbm, samples);
    if (!egl) {
        printf("failed to initialize EGL\n");
        return -1;
    }

    return drm->run(gbm, egl);
}
