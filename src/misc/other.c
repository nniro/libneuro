
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

/* other.c 
 * contains miscellaneous functions 
 * useful not directly associated with 
 * xml handling but most of the time useful.
 * Especially when you want to have more than 1 data per nodes
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef WIN32
	#define WINDOWS_MEAN_AND_LEAN
	#include <windows.h>
#else /* NOT WIN32 */
	#define __USE_BSD
	#include <unistd.h>
#endif /* NOT WIN32 */

#include <other.h>
#include <extlib.h>
#include <sys/time.h>

#include <graphics.h>
#include <ebuf.h>

u8
Chk_bound(Rectan *rec, i16 x, i16 y, i16 w, i16 h)
{
	if (rec->x <= x && (rec->x + rec->w) >= x)
	{
		if (rec->y <= y && (rec->y + rec->h) >= y)
		{
			/* Debug_Print("condition 1"); */
			return 1;
		}
		
		if (rec->y > y)
		{
			
			if ((rec->y + rec->h) < (y + h))
			{
				/* Debug_Print("condition 2"); */
				return 1;
			}
		}

		if ((rec->y + rec->h) < y)
		{
			if (rec->y > (y + h))
			{
				/* Debug_Print("condition 5"); */
				return 1;			
			}
		}
		
		/* Debug_Print("ret condition 1"); */
	}
	
	if (rec->y <= y && (rec->y + rec->h) >= y)
	{
		if (rec->x > x)
		{
			if ((rec->x + rec->w) < (x + w))
			{
				/* Debug_Print("condition 3"); */
				return 1;
			}
		}

		if ((rec->x + rec->w) < x)
		{
			if (rec->x > (x + w))
			{
				/* Debug_Print("condition 4"); */
				return 1;
			}	
			/* Debug_Print("sub ret condition 2"); */
		}
		/* Debug_Print("ret condition 2"); */
	}
	
	/* Debug_Print("condition 0");	 */
	return 0;
}

void
Neuro_Sleep(u32 t)
{
#ifdef WIN32
	Sleep(t);
#else /* NOT WIN32 */
	usleep(t);
#endif /* NOT WIN32 */
}

u32
Neuro_GetTickCount()
{
#ifdef WIN32
	return (GetTickCount() / 10) & 0x7fffffff;
#else /* NOT WIN32 */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 100 + tv.tv_usec / 10000) & 0x7fffffff;
#endif /* NOT WIN32 */
	
}

void
Neuro_CallbackBuf(OBJBUF *eng, void (*callback)(void *src))
{
	eng->callback = callback;
}

void
Neuro_AllocBuf(OBJBUF *eng, size_t sptp)
{
	void ***buf = NULL;
	u32 total = 0;
		
	buf = &eng->buffer;
	total = eng->total;
			
	if (!*buf)
	{
		*buf = calloc(1, sptp);
		total = 0;
	}
	else
		*buf = realloc(*buf, sptp * (total + 1));	
	
	total++;
	eng->total = total;
}

void
Neuro_CleanBuf(OBJBUF *eng)
{
	void ***buf;
	u32 i;
	
	if (!eng)
		return;
		
	buf = &eng->buffer;
	i = eng->total;

	while (i-- > 0)
	{
		if ((*buf)[i])
			(eng->callback)((*buf)[i]);
	}

	if (*buf)
		free(*buf);
	*buf = NULL;
	
	eng->total = 0;
	eng->buffer = NULL;
}

/* this functions was made to separate a given character, 
 * like the character ':' of a string, 
 * example : foo:bar:test:test2  
 * the functions will have the pointers to each words in the string so
 * foo bar test and test2 will be accessable independently.
 */
char **
Neuro_SepChr(const unsigned char chr, char *source, int *items)
{/* separate characters of words */
	char **ending = NULL;
	u32 slen = strlen(source);
	u32 o_total = 0;
	char *object;
	u32 o_ptr = 0;
	u32 last_ptr = 0;

	
	if (slen == 0)
		return 0;
	while ((last_ptr + o_ptr) < slen)
	{
		if (o_ptr == 0)
			last_ptr = o_ptr;
		else
			last_ptr += o_ptr + 1;
		if ((object = memchr(&source[o_ptr], chr, slen)) != NULL)
			object[0] = '\0'; /* reset the ':' to \0 so we can point to it */
		o_ptr = strlen(&source[last_ptr]);
		if (o_total == 0)
			ending = (char**)calloc(1, sizeof(char*));	
		else
		{ 
			ending = (char **)realloc((char*)ending, (sizeof(char*) * (o_total + 1))); 
			ending[o_total] = (char*)calloc(1, sizeof(char*));
		}	
		ending[o_total] = (char*)&source[last_ptr];
		o_total++;
	}			
	*items = o_total;
	return ending;
}

static void
clean_sepchr_ebuf(void *src)
{
	SepChr_Data *tmp;
	
	tmp = (SepChr_Data*)src;
	
	if (tmp->string)
		free(tmp->string);
}

/* new separate character function using the EBUF tech */
EBUF *
Neuro_SepChr2(const unsigned char chr, char *source)
{
	u32 total;
	u32 i = 0, i2 = 0;
	EBUF *temp;
	SepChr_Data *tmp;
	char *buf;

	if (source)
		total = strlen(source);
	else
		return NULL;

	Neuro_CreateEBuf(&temp);
	Neuro_SetcallbEBuf(temp, clean_sepchr_ebuf);
	
	buf = calloc(1, total);
	
	while (i < total)
	{
		if (source[i] == chr)
		{
			Neuro_AllocEBuf(temp, sizeof(SepChr_Data*), sizeof(SepChr_Data));
			tmp = Neuro_GiveCurEBuf(temp);
			
			tmp->string = calloc(1, i2 + 1);
			strncpy(tmp->string, buf, i2);
			tmp->string[i2] = '\0';
			i2 = 0;
		}
		else
		{
			buf[i2] = source[i];
			i2++;
		}
		
		i++;
	}
	
	if (i2 > 0)
	{
		Neuro_AllocEBuf(temp, sizeof(SepChr_Data*), sizeof(SepChr_Data));
		tmp = Neuro_GiveCurEBuf(temp);
			
		tmp->string = calloc(1, i2 + 1);
		strncpy(tmp->string, buf, i2);
		tmp->string[i2] = '\0';
	}

	free(buf);

	return temp;
}

u32
Neuro_GiveRGB8(u8 R, u8 G, u8 B)
{
	/* if the 2 not used bits are at the end
	 * R : 0x00000030
	 * G : 0x0000000C
	 * B : 0x00000003
	 *
	 * if the 2 not used bits are at the beginning
	 * R : 0x000000C0
	 * G : 0x00000030
	 * B : 0x0000000C
	 */
	return 1;
}

u32
Neuro_GiveRGB16(u8 R, u8 G, u8 B)
{
	/* if the not used bit is at the end
	 * R : 0x00007C00 
	 * G : 0x000003E0
	 * B : 0x0000001F
	 *
	 * if the not used bit is at the beginning
	 * R : 0x0000F800
	 * G : 0x000007C0
	 * B : 0x0000003E
	 */
	return 1;
}

u32
Neuro_GiveRGB24(u8 R, u8 G, u8 B)
{
	/* R : 0x00FF0000
	 * G : 0x0000FF00
	 * B : 0x000000FF
	 */
	return 1;
}

u32
Neuro_GiveRGB32(u8 R, u8 G, u8 B)
{
	/* if the 2 not used bits are at the end
	 * R : 0x3FF00000
	 * G : 0x000FFC00
	 * B : 0x000003FF
	 * 
	 * if the 2 not used bits are at the beginning
	 * R : 0xFFC00000
	 * G : 0x003FF000
	 * B : 0x00000FFC
	 */
	return 1;
}

u32
Neuro_GiveRGBA_special(u8 R, u8 G, u8 B)
{
	u32 out;
	
	/* hexadecimal position of the colors
	 * R : 0x000f8000 
	 * G : 0x00007c00 
	 * B : 0x000003e0
	 * A : 0x0000001f
	 *
	 * R : 0x003e0000
	 * G : 0x0001f000
	 * B : 0x00000f80
	 * A : 0x0000007c
	 *
	 * binary :
	 * 0000,0000 00|00,000|0
	 * 0000|,0000 0|000,00|00
	 */
	
	/*
	out = ((R * 31) / 255) << 11;
	out ^= ((G * 31) / 255) << 6;
	out ^= ((B * 31) / 255);
	*/
	
	
	/*out = ((R * 31) / 255);
	out <<= 5;
	out ^= ((G * 31) / 255);
	out <<= 5;
	out ^= ((B * 31) / 255);
	out <<= 5;
       	out <<= 2;
	*/
/*
	out = ((R * 31) / 255);
	out <<= 5;
	out += ((G * 31) / 255);
	out <<= 5;
	out += ((B * 31) / 255);
	out <<= 5;
	out <<= 2;
*/
	
	/* R : 0xff000000
	 * G : 0x00ff0000
	 * B : 0x0000ff00
	 * A : 0x000000ff
	 */
	
	out = R;
	out <<= 8;
	out += G;
	out <<= 8;
	out += B;
	out <<= 8;
	
	return out;
}

u32
Neuro_GiveRGB(u8 R, u8 G, u8 B)
{
	return Neuro_GiveRGBA_special(R, G, B);
}

void
Neuro_GiveImageSize(v_object *image, i32 *width, i32 *height)
{
	if (image && width && height)
	{
		Lib_GetVObjectData(image, NULL, height, width, NULL, NULL, NULL,
				NULL, NULL, NULL, NULL, NULL);
	}
}

/*
 *  * Return the pixel value at (x, y)
 *   * NOTE: The surface must be locked before calling this!
 *    */
u32 
Neuro_RawGetPixel(v_object *srf, int x, int y)
{
#if temp
	u8 bpp;
	u8 *p;
	void *pixels;
	u32 pitch;
	u32 err;
	
	
	Lib_LockVObject(srf);
	
	Lib_GetVObjectData(srf, NULL, NULL, NULL, &pitch, &pixels, NULL, &bpp,
			NULL, NULL, NULL, NULL);
	
	/* bpp = surface->format->BytesPerPixel; */
	/* Here p is the address to the pixel we want to retrieve */
	/* p = (u8 *)surface->pixels + y * surface->pitch + x * bpp; */
	p = (u8 *)pixels + y * pitch + x * bpp;
	err = 0;
	
	switch(bpp) 
	{
		case 1:
		{
			err = *p;
			/* err = *(u16 *)p; */
		}
		break;
		
		case 2:
		{
			err = *(u16 *)p;
		}
		break;
		
		/*case 3:
		{
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
				err = p[0] << 16 | p[1] << 8 | p[2];
			else
				err = p[0] | p[1] << 8 | p[2] << 16;
		}
		break;
		*/
		
		case 4:
		{
			err = *(u32 *)p;
		}
		break;
		
		default:
		{
			err = 0;       /* shouldn't happen, but avoids warnings */
		}
	}

	Lib_UnlockVObject(srf);

	return err;
#endif /* temp */
	return 0;
}


/*
 *  * Set the pixel at (x, y) to the given value
 *   * NOTE: The surface must be locked before calling this!
 *    */
void 
Neuro_RawPutPixel(v_object *srf, int x, int y, u32 pixel)
{
#if temp
	/* SDL_Surface *surface = (SDL_Surface*)srf; */
	/* SDL_LockSurface(surface); */
	u8 bpp;
	u8 *p;
	void *pixels;
	u32 pitch;
	
	Lib_GetVObjectData(srf, NULL, NULL, NULL, &pitch, &pixels, NULL, &bpp,
			NULL, NULL, NULL, NULL);

	/* int bpp = surface->format->BytesPerPixel; */
	/* Here p is the address to the pixel we want to set */
	/* p = (u8*)surface->pixels + y * surface->pitch + x * bpp; */
	p = (u8 *)pixels + y * pitch + x * bpp;

	switch(bpp) 
	{
		case 1:
		{
			*p = pixel;
		}
		break;

		case 2:
		{
			*(u16*)p = pixel;
		}
		break;
		/*
		case 3:
		{
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) 
			{
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			} 
			else 
			{
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
		}
		break;
		*/
		case 4:
		{
			*(u32 *)p = pixel;
		}
		break;
		
	}
	/* SDL_UnlockSurface(surface); */
#endif /* temp */
}

void
Neuro_PrintFPS()
{
	t_tick fps;

	Neuro_GiveFPS(&fps);

	if (fps >= 0)
	{
		printf("current fps : %d\n", fps);
	}
}

/* substract num2 from num1 and returns the answer only if it is positive,
 * if it is not, returns 0 
 */
i16
Neuro_CalcOnlyPos(i16 num1, i16 num2)
{
	i16 result;
	
	result = num1 - num2;

	if (result < 0)
		return 0;
	else 
		return result;
}

/* skeleton rectangle or square bounds check function.
 * return values :
 * 0 = depen is inside indep.
 * 1 = depen and indep are not touching each others(they are far away).
 * 2 = depen is overlaping indep. 
 * this function needs to be converted to a macro so its lightning quick
 */
u8
Neuro_DumbBoundsCheck(Rectan *indep, Rectan *depen)
{		
	register u8 status = 0;

	status = Chk_bound(indep, depen->x, depen->y, depen->w, depen->h); /* up left */
	status += Chk_bound(indep, depen->x + depen->w, depen->y, depen->w * -1, depen->h); /* up right */
	status += Chk_bound(indep, depen->x + depen->w, depen->y + depen->h, (i16)depen->w * -1, (i16)depen->h * -1); /* bottom right */
	status += Chk_bound(indep, depen->x, depen->y + depen->h, depen->w, (i16)depen->h * -1); /* bottom left */

	if (status == 4)
	{
		status = 0; /* obj is inside form */
	}
	else
	{
		if (status > 0)
			status = 2; /* obj overlaps form */
		else
			status = 1; /* obj is not touching form */
	}
	
	return status;
}

/* rectangle or square bounds check function.
 * return values :
 * 0 = depen is inside indep.
 * 1 = depen and indep are not touching each others(they are far away).
 * 2 = depen is overlaping indep. 
 * 3 = depen and indep links between corners are into the other but the corners
 *     are not touching.
 * this function needs to be converted to a macro so its lightning quick
 */
u8
Neuro_BoundsCheck(Rectan *indep, Rectan *depen)
{		
	register u8 status = 0;
	
	status = Neuro_DumbBoundsCheck(indep, depen);

	/* check to see if depen is bigger than indep and indep is inside depen 
	 * if its the case, the status will be 2.
	 */
	if (status == 1)
	{
		if (Neuro_DumbBoundsCheck(depen, indep) != 1)
			status = 2;
	}

	if (status == 0)
	{
		if (Neuro_DumbBoundsCheck(depen, indep) == 0)
			status = 3;
	}
	
	return status;
}

/* only play with the x and width values 
 * the Rectan indep should contain a rectangle.
 * isrc is the Rectan that contains an image source Rectan.
 * idst is the Rectan that contains the destination for the image
 *
 * this function will calibrate isrc and idst so the image represented
 * by those 2 will not go over the rectangle indep.
 * This function only fixes the vertical way of the image.
 */
void
Neuro_VerticalBoundFix(Rectan *indep, Rectan *isrc, Rectan *idst)
{
	/* 2 methods : 1 is using statements and the other is mathematical */

	/* the statement method */
	/*
	if ((indep->x) > idst->x)
	{
		isrc->x += (indep->x) - idst->x;
		isrc->w -= abs((indep->x) - idst->x);
		idst->x = indep->x;
	}
	
	if ((indep->x + indep->w) < (idst->x + isrc->w))
	{
		isrc->w -= (idst->x + isrc->w) - (indep->x + indep->w);
	}
	*/	
	
	/* the mathematical method */
	isrc->x += Neuro_CalcOnlyPos(indep->x, idst->x);
	isrc->w = Neuro_CalcOnlyPos(isrc->w, (Neuro_CalcOnlyPos(indep->x, idst->x) + Neuro_CalcOnlyPos(idst->x + isrc->w, indep->x + indep->w)));
	idst->x += Neuro_CalcOnlyPos(indep->x, idst->x);
}

/* only play with the y and height values 
 * the Rectan indep should contain a rectangle.
 * isrc is the Rectan that contains an image source Rectan.
 * idst is the Rectan that contains the destination for the image
 *
 * this function will calibrate isrc and idst so the image represented
 * by those 2 will not go over the rectangle indep.
 * This function only fixes the horizontal way of the image.
 */
void
Neuro_HorizontalBoundFix(Rectan *indep, Rectan *isrc, Rectan *idst)
{
	/* 2 methods : 1 is using statements and the other is mathematical */
	
	/* the statement method */
	/*	
	if ((indep->y + 1) > idst->y)
	{
		isrc->y += abs((indep->y + 1) - idst->y);
		isrc->h -= abs((indep->y + 1) - idst->y);
		idst->y = (indep->y + 1);
	}
	if ((indep->y + indep->h) < (idst->y + isrc->h))
	{
		isrc->h -= (idst->y + isrc->h) - (indep->y + indep->h);
	}
	*/
	
	/* the mathematical method */
	isrc->y += Neuro_CalcOnlyPos(indep->y, idst->y);
	isrc->h = Neuro_CalcOnlyPos(isrc->h, (Neuro_CalcOnlyPos(indep->y, idst->y) + Neuro_CalcOnlyPos(idst->y + isrc->h, indep->y + indep->h)));
	idst->y += Neuro_CalcOnlyPos(indep->y, idst->y);
}

