/* x11.c
 * X11 driver(abstraction layer) source.
 */

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/xpm.h>

#include <extlib.h>
#include <ebuf.h>
#include <other.h>


typedef struct V_OBJECT
{
	Display *display;
	i32 screen;
	GC GC;

	GC *cGC; /* pointer to the current GC in use */
	Window *cwin; /* pointer to the current window in use or pixmap */

	Window rwin; /* the root window */
	
	Window win;
	GC wGC;
	XGCValues wValue;
	
	Pixmap data;
	Pixmap shapemask;
	XpmAttributes attrib;
}V_OBJECT;

EBUF *vobjs;

V_OBJECT *dmain;
V_OBJECT *scldmain; /* buffer (double) */

static void
clean_Vobjects(void *src)
{
	V_OBJECT *buf;

	buf = (V_OBJECT*)src;

	/*if (dmain->display == buf->display)
		Info_Print("Cleaning the main display");*/

	/* Info_Print("cleaning process ..."); */
	/* Info_Print("pixmap data"); */
	if (buf->data)
		XFreePixmap(dmain->display, buf->data);

	/* Info_Print("shapemask"); */
	if (buf->shapemask)
		XFreePixmap(dmain->display, buf->shapemask);
	
	/* Info_Print("display main window Graphic context (GC)"); */
	if (buf->wGC)
		XFreeGC(buf->display, buf->wGC);
	
	/* Info_Print("window"); */
	if (buf->win)
		XDestroyWindow(buf->display, buf->win);
	
	/* Info_Print("display"); */
	if (buf->display)
		XCloseDisplay(buf->display);
	/* Info_Print("cleaning process done"); */
}


/*  video constructor destructor  */
/* will need to include the screen width and height also */
int
Lib_VideoInit(v_object **screen, v_object **screen_buf)
{	
	V_OBJECT *tmp;
	i32 width = 800, height = 600;
	
	Neuro_CreateEBuf(&vobjs);
	Neuro_SetcallbEBuf(vobjs, clean_Vobjects);
	
	Neuro_AllocEBuf(vobjs, sizeof(V_OBJECT*), sizeof(V_OBJECT));
	
	tmp = Neuro_GiveCurEBuf(vobjs);
	
	tmp->display = XOpenDisplay(NULL);
	tmp->screen = XDefaultScreen(tmp->display);
	tmp->GC = XDefaultGC(tmp->display, tmp->screen);
	
	tmp->win = XCreateSimpleWindow(tmp->display, XRootWindow(tmp->display, tmp->screen),
			200, 200, width, height, 4,
			BlackPixel(tmp->display, tmp->screen),
			WhitePixel(tmp->display, tmp->screen));

	tmp->wGC = XCreateGC(tmp->display, tmp->win, 0, &tmp->wValue);

	XMapWindow(tmp->display, tmp->win);
	
	tmp->rwin = XRootWindow(tmp->display, tmp->screen);
	
	tmp->cwin = &tmp->win;
	tmp->cGC = &tmp->wGC;
	
	dmain = tmp;
	*screen = tmp;
	if (screen_buf)
	{
		V_OBJECT *tmp2;
		Neuro_AllocEBuf(vobjs, sizeof(V_OBJECT*), sizeof(V_OBJECT));
		
		tmp2 = Neuro_GiveCurEBuf(vobjs);

		/* XCreateImage(tmp->display, tmp->win, 24, ZPixmap, 0, 0, 
				width, height, 16, 0); */
		/* *screen_buf = *screen; */ /* cheap hack that will need to be FIxed TODO TODO */
	}
	
	return 0;
}

u32 
Lib_MapRGB(v_object *vobj, u8 r, u8 g, u8 b)
{
	return 0;
}

void
Lib_SetColorKey(v_object *vobj, u32 flag,u32 key)
{
	
}

void
Lib_BlitObject(v_object *source, Rectan *src, v_object *destination, Rectan *dst)
{
	V_OBJECT *vsrc, *vdst;
	Rectan Rsrc, Rdst;
	i32 h, w;
	int _err = 0;
	
	vsrc = (V_OBJECT*)source;
	vdst = (V_OBJECT*)destination;

	if (src == NULL)
	{
		Neuro_GiveImageSize(source, &w, &h);
		
		Rsrc.x = 0;
		Rsrc.y = 0;
		Rsrc.h = (i16)h;
		Rsrc.w = (i16)w;
	}
	else
	{
		Rsrc.x = src->x;
		Rsrc.y = src->y;
		Rsrc.h = src->h;
		Rsrc.w = src->w;
	}

	if (dst == NULL)
	{
		Rdst.x = 0;
		Rdst.y = 0;
		Rdst.h = 0;
		Rdst.w = 0;
	}
	else
	{
		Rdst.x = dst->x;
		Rdst.y = dst->y;
		Rdst.h = dst->h;
		Rdst.w = dst->w;
	}
		
	
	XSetClipOrigin(dmain->display, dmain->GC, 20, 20);
	XSetClipMask(dmain->display, vdst->GC, vsrc->shapemask);
	/* TODO change vsrc->data and vdst->cwin to pointers which will change 
	 * depending on the type of v_object the v_object is. Either core
	 * or pixmap...
	 */
	_err = XCopyArea(dmain->display, vsrc->data, *vdst->cwin, *dmain->cGC, 
			Rsrc.x, Rsrc.y, Rsrc.w, Rsrc.h,
			Rdst.x, Rdst.y);

	if (_err != 0)
	{
		Error_Print("Blit seems to have failed");
		Debug_Val(0, "Debug value is %d\n", _err);
	}
	XSetClipMask(vdst->display, vdst->GC, None);
}

void
Lib_LoadBMP(const char *path, v_object **img)
{
	EBUF *temp;
	V_OBJECT *tmp;
	char **buffer;
	char **initbuf;
	int i = 0;
	int _err = 0;

	readBitmapFileToPixmap(path, &temp);
	if (!temp)
	{
		Debug_Val(0, "Error loading the file %s, it might not exist or its not a bitmap\n", path);
		return;
	}

	
	Neuro_AllocEBuf(vobjs, sizeof(V_OBJECT*), sizeof(V_OBJECT));

	tmp = Neuro_GiveCurEBuf(vobjs);

	buffer = (char**)Neuro_GiveEBufCore(temp);	
	
	initbuf = buffer;
	while (*buffer)
	{
		Debug_Val(1, "%s\n", *buffer);
		i++;
		buffer++;
	}
	Debug_Val(1, "%s\nreal total == %d\n", *initbuf, i);
	
	_err = XpmCreatePixmapFromData(dmain->display, dmain->win, initbuf, &tmp->data, &tmp->shapemask, &tmp->attrib);
	
	if (_err == 0)
	{
		*img = tmp;
		Debug_Val(0, "Successfully loaded the file %s\n", path);
	}
	else
		Debug_Val(0, "Error loading the file %s with error %d\n", path, _err);
	
	Neuro_CleanEBuf(&temp);

	
	return; /* int needed */
}

v_object *
Lib_CreateVObject(u32 flags, i32 width, i32 height, i32 depth, u32 Rmask, u32 Gmask,
		u32 Bmask, u32 Amask)
{
	return NULL;
}

void
Lib_UpdateRect(v_object *source, Rectan *src)
{

}

void
Lib_FillRect(v_object *source, Rectan *src, u32 color)
{

}

void
Lib_Flip(v_object *source)
{

}

void
Lib_FreeVobject(v_object *source)
{

}

void 
Lib_LockVObject(v_object *vobj)
{
	
}

void 
Lib_UnlockVObject(v_object *vobj)
{
	
}

void
Lib_GiveVobjectProp(v_object *source, Rectan *output)
{

}

void
Lib_GetVObjectData(v_object *vobj, u32 *flags, i32 *h, i32 *w, u32 *pitch, 
		void **pixels, Rectan **clip_rect, u8 *bpp, 
		u32 *Rmask, u32 *Gmask, u32 *Bmask,u32 *Amask)
{

}

void
Lib_VideoExit()
{
	Neuro_CleanEBuf(&vobjs);
}

/*----------------- Input Events -----------------*/

u8 *
Lib_GetKeyState(i32 *numkeys)
{
	return NULL;
}

u8
Lib_GetMouseState(i32 *x, i32 *y)
{
	return 0;
}

void
Lib_EventPoll()
{

}

i32
Lib_PollEvent(void *event)
{
	return 0;
}

int
Lib_EventsInit()
{
	return 0;
}

void
Lib_EventsExit()
{
	
}
