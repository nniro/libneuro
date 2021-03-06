
/*    
 * libneuro, a light weight abstraction of high or lower libraries 
 * and toolkit for applications.
 * Copyright (C) 2005-2006  Nicholas Niro, Robert Lemay
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* none.c
 * the source file permitting to compile this library without any lower
 * extern library(this is used for debugging).
 */

#include <global.h>
#include <extlib.h>
#include <stdlib.h>

/*  video constructor destructor  */
int
Lib_VideoInit(v_object **screen, v_object **screen_buf)
{	
	return 0;
}

void
Lib_BlitObject(v_object *source, Rectan *src, v_object *destination, Rectan *dst)
{

}

void
Lib_LoadBMP(const char *path, v_object **img)
{

}

void
Lib_LoadBMPBuffer(void *data, v_object **img)
{

}

u32 
Lib_MapRGB(v_object *vobj, u8 r, u8 g, u8 b)
{
	return 0;
}

void
Lib_SetColorKey(v_object *vobj, u32 key)
{
	
}

void
Lib_SetAlpha(v_object *vobj, u8 alpha)
{
	
}

void 
Lib_PutPixel(v_object *srf, int x, int y, u32 pixel)
{

}

u32 
Lib_GetPixel(v_object *srf, int x, int y)
{
	return 0;
}

v_object *
Lib_CreateVObject(u32 flags, i32 width, i32 height, i32 depth, u32 Rmask, u32 Gmask,
		u32 Bmask, u32 Amask)
{
	return NULL;
}

void
Lib_SetScreenSize(i32 width, i32 height)
{

}

void
Lib_GetScreenSize(i32 *width, i32 *height)
{
	
}

void
Lib_UpdateRect(v_object *source, Rectan *src)
{

}

u32
Lib_GetDefaultDepth(void)
{
	return 0;
}

void
Lib_SyncPixels(v_object *src)
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
Lib_VideoExit(void)
{

}

/*----------------- Input Events -----------------*/

u8
Lib_CheckKeyStatus(u32 key)
{
	return 0;
}

u8
Lib_GetMouseState(i32 *x, i32 *y)
{
	return 0;
}

void
Lib_EventPoll(void)
{

}

i32
Lib_PollEvent(void *event)
{
	return 0;
}

int
Lib_EventInit(void)
{
	return 0;
}

void
Lib_EventExit(void)
{
	
}
