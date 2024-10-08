/*
 * Copyright (c) 2017 Rob Clark <rclark@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> //STDIN_FILENO
#include <sys/select.h>
#include <sys/time.h>
#include "opengl.h"
#include "drm-common.h"
#include <stdlib.h>

static struct drm drm;

static void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	/* suppress 'unused parameter' warnings */
	(void)fd, (void)frame, (void)sec, (void)usec;

	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

static int legacy_run(const struct gbm *gbm, const struct egl *egl)
{
	fd_set fds;
	drmEventContext evctx = {
			.version = 2,
			.page_flip_handler = page_flip_handler,
	};
	struct gbm_bo *bo;
	struct drm_fb *fb;
	uint32_t i = 0;
	int ret;

	eglSwapBuffers(egl->display, egl->surface);	
	bo = gbm_surface_lock_front_buffer(gbm->surface);
	fb = drm_fb_get_from_bo(bo);
	if (!fb) {
		fprintf(stderr, "Failed to get a new framebuffer BO\n");
		return -1;
	}
	
	/* set mode: */
	ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0, &drm.connector_id, 1, drm.mode);
	
	if (ret) {
		printf("failed to set mode: %s\n", strerror(errno));
		return ret;
	}
	
	bool dpms_enabled = true;
	char debug_msg[100];
	
	while (1) {
		struct gbm_bo *next_bo;
		int waiting_for_flip = 1;
		bool dpms_request;
		
		egl->draw(i++,debug_msg,&dpms_request);
		//printf(" %d \n",dpms_request);

		eglSwapBuffers(egl->display, egl->surface);
		next_bo = gbm_surface_lock_front_buffer(gbm->surface);
		fb = drm_fb_get_from_bo(next_bo);
		if (!fb) {
			fprintf(stderr, "Failed to get a new framebuffer BO\n");
			return -1;
		}


	drm_wait_master(drm.fd);
		if (dpms_request == false && dpms_enabled == true){
			
		   int ret = drmModeConnectorSetProperty(drm.fd, drm.connector_id, drm.dpms_idx, DRM_MODE_DPMS_SUSPEND);
		     printf("DPMS Turning off... %d\n",ret);
		     if (ret == 0) dpms_enabled = dpms_request;
		}
		
		if (dpms_request == true && dpms_enabled == false){
			
		   int ret =  drmModeConnectorSetProperty(drm.fd, drm.connector_id, drm.dpms_idx, DRM_MODE_DPMS_ON);
		     printf("DPMS Turning on... %d\n",ret);
		   if (ret == 0) dpms_enabled = dpms_request;
		
		}		
		
		
		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */
		if (dpms_enabled){
		
			ret = drmModePageFlip(drm.fd, drm.crtc_id, fb->fb_id,DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
			drmDropMaster(drm.fd);
			
			debug_msg[0] = '\0';
			if (ret) {
				printf("failed to queue page flip: %s\n", strerror(errno));
				exit(1);
			}else{
				while (waiting_for_flip) {
					FD_ZERO(&fds);
					FD_SET(STDIN_FILENO, &fds);
					FD_SET(drm.fd, &fds);

					ret = select(drm.fd + 1, &fds, NULL, NULL, NULL);
					if (ret < 0) {
						printf("select err: %s\n", strerror(errno));
						return ret;
					} else if (ret == 0) {
						printf("select timeout!\n");
						return -1;
					} else if (FD_ISSET(STDIN_FILENO, &fds)) {
						//stdin is generally line buffered so this works fine for debug input
						int results = read(STDIN_FILENO, debug_msg, 90);
						debug_msg[results] = '\0';
					}
					drmHandleEvent(drm.fd, &evctx);
				}
			}
		}else{
				drmDropMaster(drm.fd);
		}
		
		
		/* release last buffer to render on again: */
		gbm_surface_release_buffer(gbm->surface, bo);
		bo = next_bo;
	}

	return 0;
}

const struct drm * init_drm_legacy(const char *device, const char *mode_str, unsigned int vrefresh, int monitor)
{	
	int ret;

	ret = init_drm(&drm, device, mode_str, vrefresh, monitor);
	if (ret)
		return NULL;

	drm.run = legacy_run;

	return &drm;
}
