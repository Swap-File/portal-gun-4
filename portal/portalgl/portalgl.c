#include <gst/gl/x11/gstgldisplay_x11.h>
#include <math.h> //M_PI
#include <stdio.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "portalgst.h"

Display *dpy;
Window win;
GLXContext ctx;

#ifndef GLX_MESA_swap_control
#define GLX_MESA_swap_control 1
typedef int (*PFNGLXGETSWAPINTERVALMESAPROC)(void);
#endif

/* new window size or exposure */
static void reshape(int screen_width, int screen_height)
{
	float nearp = 1, farp = 100.0f, hht, hwd;

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glViewport(0, 0, (GLsizei)screen_width, (GLsizei)screen_height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	hht = nearp * tan(45.0 / 2.0 / 180.0 * M_PI);
	hwd = hht * screen_width / screen_height;

	glFrustum(-hwd, hwd, -hht, hht, nearp, farp);
}

/*
* Create an RGB, double-buffered window.
* Return the window and context handles.
*/
static void make_window( Display *dpy, const char *name,int x, int y, int width, int height,Window *winRet, GLXContext *ctxRet, VisualID *visRet)
{
	int attribs[64];
	int i = 0;

	int scrnum;
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	Window win;

	XVisualInfo *visinfo;

	/* Singleton attributes. */
	attribs[i++] = GLX_RGBA;
	attribs[i++] = GLX_DOUBLEBUFFER;

	/* Key/value attributes. */
	attribs[i++] = GLX_RED_SIZE;
	attribs[i++] = 1;
	attribs[i++] = GLX_GREEN_SIZE;
	attribs[i++] = 1;
	attribs[i++] = GLX_BLUE_SIZE;
	attribs[i++] = 1;
	attribs[i++] = GLX_DEPTH_SIZE;
	attribs[i++] = 1;

	attribs[i++] = None;

	scrnum = DefaultScreen( dpy );
	root = RootWindow( dpy, scrnum );

	visinfo = glXChooseVisual(dpy, scrnum, attribs);
	if (!visinfo) {
		printf("Error: couldn't get an RGB, Double-buffered");
		exit(1);
	}

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
	attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
	/* XXX this is a bad way to get a borderless window! */
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	win = XCreateWindow( dpy, root, x, y, width, height,0, visinfo->depth, InputOutput,	visinfo->visual, mask, &attr );

	/* set hints and properties */
	{
		XSizeHints sizehints;
		sizehints.x = x;
		sizehints.y = y;
		sizehints.width  = width;
		sizehints.height = height;
		sizehints.flags = USSize | USPosition;
		XSetNormalHints(dpy, win, &sizehints);
		XSetStandardProperties(dpy, win, name, name,
		None, (char **)NULL, 0, &sizehints);
	}

	ctx = glXCreateContext( dpy, visinfo, NULL, True );
	if (!ctx) {
		printf("Error: glXCreateContext failed\n");
		exit(1);
	}

	*winRet = win;
	*ctxRet = ctx;
	*visRet = visinfo->visualid;

	XFree(visinfo);
}

/**
* Determine whether or not a GLX extension is supported.
*/
static int is_glx_extension_supported(Display *dpy, const char *query)
{
	const int scrnum = DefaultScreen(dpy);
	const char *glx_extensions = NULL;
	const size_t len = strlen(query);
	const char *ptr;

	if (glx_extensions == NULL) {
		glx_extensions = glXQueryExtensionsString(dpy, scrnum);
	}

	ptr = strstr(glx_extensions, query);
	return ((ptr != NULL) && ((ptr[len] == ' ') || (ptr[len] == '\0')));
}

/**
* Attempt to determine whether or not the display is synched to vblank.
*/
static void query_vsync(Display *dpy, GLXDrawable drawable)
{
	int interval = 0;

#if defined(GLX_EXT_swap_control)
	if (is_glx_extension_supported(dpy, "GLX_EXT_swap_control")) {
		unsigned int tmp = -1;
		glXQueryDrawable(dpy, drawable, GLX_SWAP_INTERVAL_EXT, &tmp);
		interval = tmp;
	} else
#endif
	if (is_glx_extension_supported(dpy, "GLX_MESA_swap_control")) {
		PFNGLXGETSWAPINTERVALMESAPROC pglXGetSwapIntervalMESA =
		(PFNGLXGETSWAPINTERVALMESAPROC)
		glXGetProcAddressARB((const GLubyte *) "glXGetSwapIntervalMESA");

		interval = (*pglXGetSwapIntervalMESA)();
	} else if (is_glx_extension_supported(dpy, "GLX_SGI_swap_control")) {
	/* The default swap interval with this extension is 1.  Assume that it
	* is set to the default.
	*
	* Many Mesa-based drivers default to 0, but all of these drivers also
	* export GLX_MESA_swap_control.  In that case, this branch will never
	* be taken, and the correct result should be reported.
	*/
		interval = 1;
	}

	if (interval > 0) {
		printf("Running synchronized to the vertical refresh.  The framerate should be\n");
		if (interval == 1) {
			printf("approximately the same as the monitor refresh rate.\n");
		} else if (interval > 1) {
			printf("approximately 1/%d the monitor refresh rate.\n",
			interval);
		}
	}
}

int main(int argc, char *argv[])
{

	
	/* Initialize X11 */
	unsigned int winWidth = 1366 * 2, winHeight = 768;
	
	char *dpyName = NULL;

	VisualID visId;
	
	static Display *dpy;
	
	dpy = XOpenDisplay(dpyName);
	if (!dpy) {
		printf("Error: couldn't open display %s\n",
		dpyName ? dpyName : getenv("DISPLAY"));
		return -1;
	}

	make_window(dpy, "gstvideo", 0, 0, winWidth, winHeight, &win, &ctx, &visId);
	XMapWindow(dpy, win);
	glXMakeCurrent(dpy, win, ctx);
	query_vsync(dpy, win);

	/* Set initial projection/viewing transformation.
	* We can't be sure we'll get a ConfigureNotify event when the window
	* first appears.
	*/
	reshape(winWidth, winHeight);

	portalgst_init();
	
	//cleanup code
	glXMakeCurrent(dpy, None, NULL);
	glXDestroyContext(dpy, ctx);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
	
	return 0;
}
