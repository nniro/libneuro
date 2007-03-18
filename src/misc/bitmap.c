
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

/* bitmap.c
 * Module : Bitmap
 *
 * bitmap process module
 */

/*-------------------- Extern Headers Including --------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#if USE_ZLIB
/* this is used to open the bitmaps, 
 * the beauty of this is it works 
 * for compressed and uncompressed
 * transparently, meaning no extra code
 * for both!
 */
#include <zlib.h>
typedef gzFile nFILE;
#else /* NOT USE_ZLIB */
typedef FILE nFILE;
#endif /* USE_ZLIB */

/*-------------------- Local Headers Including ---------------------*/
#include <ebuf.h>
#include <other.h>
#include <extlib.h> /* Lib_GetDefaultDepth() */

#include <graphics.h> /* Neuro_PutPixel */

/*-------------------- Main Module Header --------------------------*/
#include <bitmap.h>


/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("bitmap");

typedef struct BITMAP_HEADER
{
	u16 type __attribute__((packed));			/* Magic identifier            */
	u32 size __attribute__((packed));			/* File size in bytes          */
	u16 reserved1, reserved2;	/* reserved */
	u32 offset __attribute__((packed));			/* Offset to image data, bytes */
}BITMAP_HEADER;

typedef struct BITMAP_INFOHEADER
{
	u32 size;			/* Header size in bytes      */
	i32 width, height;		/* Width and height of image */
	u16 planes;			/* Number of colour planes   */
	u16 bits;			/* Bits per pixel            */
	u32 compression;		/* Compression type          */
	u32 imagesize;			/* Image size in bytes       */
	i32 xresolution, yresolution;	/* Pixels per meter          */
	u32 ncolors;			/* Number of colours         */
	u32 importantcolours;		/* Important colours         */
}BITMAP_INFOHEADER;

typedef struct BITMAP_HDATA
{
	BITMAP_HEADER header __attribute__((packed));
	BITMAP_INFOHEADER infoheader __attribute__((packed));
}BITMAP_HDATA;

typedef struct BITMAP_COLOR
{
	u8 r, g, b /*, a*/ ; /* red green blue */
	u8 *symbol; /* unique symbol associated with this color */
}BITMAP_COLOR;

typedef struct BITMAP_MAP
{
	BITMAP_COLOR *color;
}BITMAP_MAP;

struct BMP_CTX
{
	nFILE *f_bitmap;

	/* major (buffers) */
	EBUF *bmap_colors; /* the colors */
	u8 *buf; /* the buffer that will contain the content of the file */
	
	/* minor (mostly pointers and temporary variables) */
	i32 i; /* incremental variable */
	u32 skip_i, x, y;
	
	u32 psize; /* the full size of the pixels data */
	u8 *palette; /* the pointer to the palette if theres one */
	BITMAP_HDATA *bmap; /* this is how we will get informations about the bitmap */
	int aux_var; /* auxiliary variable that can be used by external functions */
	char *aux_buf; /* same as aux_var but a buffer */
	double msize;
	double calc;
	double tmp;
	u32 wmult;
	double pixellen;
	u32 increm;
	u8 DATA;
	v_object *output; /* the image into which we will load the bitmap */
};


/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/

static void print_bitmap_infos(BITMAP_HDATA *bmap) __attribute__((unused));
static int fpdata8(nFILE *input, u8 *output) __attribute__((unused));
static int fpdata16(nFILE *input, u16 *output) __attribute__((unused));
static int fpdata32(nFILE *input, u32 *output) __attribute__((unused));


/*-------------------- Static Functions ----------------------------*/

static void 
clean_bmap_color(void *eng)
{
	BITMAP_COLOR *buf;

	buf = (BITMAP_COLOR*)eng;

	if (buf->symbol)
		free(buf->symbol);
}


/* returns 0 on success and puts the data in *output
 * 1 on error dont touch output
 */
static int
fpdata8(nFILE *input, u8 *output)
{
	if (input == NULL || output == NULL)
		return 1;
#if USE_ZLIB
	*output = gzgetc(input);
#else /* NOT USE_ZLIB */
	*output = fgetc(input);
#endif /* NOT USE_ZLIB */


	return 0;
}

/* returns 0 on success and puts the data in *output
 * 1 on error dont touch output
 */
static int
fpdata16(nFILE *input, u16 *output)
{
	u8 feed[2];
	u16 *buf;
	
	if (input == NULL || output == NULL)
		return 1;

#if USE_ZLIB
	feed[0] = gzgetc(input);
	feed[1] = gzgetc(input);
#else /* NOT USE_ZLIB */
	feed[0] = fgetc(input);
	feed[1] = fgetc(input);
#endif /* NOT USE_ZLIB */

	buf = (u16*)&feed;
	
	*output = *buf;

	return 0;
}

/* returns 0 on success and puts the data in *output
 * 1 on error dont touch output
 */
static int
fpdata32(nFILE *input, u32 *output)
{
	/* register int feed; */
	u8 feed[4];
	u32 *buf;
	
	
	if (input == NULL || output == NULL)
		return 1;

#if USE_ZLIB
	feed[0] = gzgetc(input);
	feed[1] = gzgetc(input);
	feed[2] = gzgetc(input);
	feed[3] = gzgetc(input);
#else /* NOT USE_ZLIB */
	feed[0] = fgetc(input);
	feed[1] = fgetc(input);
	feed[2] = fgetc(input);
	feed[3] = fgetc(input);
#endif /* NOT USE_ZLIB */

	buf = (u32*)&feed;
	
	*output = *buf;

	return 0;
}

static BITMAP_HDATA *
parse_bitmap_header(nFILE *input)
{
	BITMAP_HDATA *buf;
	BITMAP_INFOHEADER *tmp;

	if (input == NULL)
		return NULL;
	
	buf = calloc(1, sizeof(BITMAP_HDATA));

	tmp = &buf->infoheader;
	
	fpdata16(input, &buf->header.type);
	fpdata32(input, &buf->header.size);
	fpdata16(input, &buf->header.reserved1);
	fpdata16(input, &buf->header.reserved2);
	fpdata32(input, &buf->header.offset);
	
	fpdata32(input, &tmp->size);
	fpdata32(input, (u32*)&tmp->width);
	fpdata32(input, (u32*)&tmp->height);
	fpdata16(input, &tmp->planes);
	fpdata16(input, &tmp->bits);
	fpdata32(input, &tmp->compression);
	fpdata32(input, &tmp->imagesize);
	fpdata32(input, (u32*)&tmp->xresolution);
	fpdata32(input, (u32*)&tmp->yresolution);
	fpdata32(input, &tmp->ncolors);
	fpdata32(input, &tmp->importantcolours);
	
	return buf;
}

/* the magic number will probably not show correctly if big endian */
static void
print_bitmap_infos(BITMAP_HDATA *bmap)
{	
	printf("[%c%c] header data :\nsize %d\noffset %d\ninfoheader data :\nsize %d\nwidth %d\nheight %d\nplanes %d\nbits %d\ncompression %d\nimagesize %d\nxres %d\nyres %d\nncolors %d\nimportantcolors %d\n", 
		bmap->header.type & 0x00FF,
		(bmap->header.type & 0xFF00) >> 8,
		bmap->header.size,
		bmap->header.offset,
		bmap->infoheader.size,
		bmap->infoheader.width,
		bmap->infoheader.height,
		bmap->infoheader.planes,
		bmap->infoheader.bits,
		bmap->infoheader.compression,
		bmap->infoheader.imagesize,
		bmap->infoheader.xresolution,
		bmap->infoheader.yresolution,
		bmap->infoheader.ncolors,
		bmap->infoheader.importantcolours);
}


static void
process_palette(nFILE *input, BITMAP_HDATA *bmap, EBUF *bcolors)
{
	u32 i = 0;
	BITMAP_COLOR *buf;

	while (i < bmap->infoheader.ncolors)
	{
		Neuro_AllocEBuf(bcolors, sizeof(BITMAP_COLOR*), sizeof(BITMAP_COLOR));
		
		buf = Neuro_GiveCurEBuf(bcolors);
		
		/*buf->r = palette[(i * 4) + 2];
		buf->g = palette[(i * 4) + 1];
		buf->b = palette[(i * 4)];
		*/

		/*
		fpdata8(input, &buf->b);
		fpdata8(input, &buf->g);
		fpdata8(input, &buf->r);
		fpdata8(input, &buf->a);
		*/
#if USE_ZLIB
		buf->b = gzgetc(input);
		buf->g = gzgetc(input);
		buf->r = gzgetc(input);
		
		/* I leave this just in case */
		/* buf->a = fgetc(input); */
		
		/* skip the alpha color */
		gzgetc(input);	
#else /* NOT USE_ZLIB */
		buf->b = fgetc(input);
		buf->g = fgetc(input);
		buf->r = fgetc(input);
		
		/* I leave this just in case */
		/* buf->a = fgetc(input); */
		
		/* skip the alpha color */
		fgetc(input);
#endif /* NOT USE_ZLIB */
		i++;
	}
}

/* input the bits per pixel of the image
 * input a 1 byte of data to process 
 */
static void
process_bitmap2(BITMAP_HDATA *bmap, v_object *image, u8 *palette, u8 *data, EBUF *bcolors, u32 *x, u32 *y, int *aux, char **buf)
{
	
	/* we will call functions depending on the bpp of the image */
	switch (bmap->infoheader.bits)
	{
		case 1:
		{
			/* will do a loop to get each 8 pixels from the data */
			
			/* this is pretty much for little endian 
			 * the minimum data size we can have for a certain width is
			 * 32 bits (4 bytes increments). Those 32 bits will be able 
			 * to hold up to 32 pixels. In case the width is higher than 
			 * 32 pixels, it will put 32 bits until the whole width
			 * can be fulfilled.
			 */
			/* we will need to have the width value because we will need
			 * to know how many bits we have to read from 32 bits (4 bytes). 
			 */
			/* aux will keep how many 32 bits still need to be done in a 
			 * certain width 
			 */
			u8 temp;
			/* double calc = 0; */
			
			/* u8 r, g, b; */
			
			const u8 values[8] = {
				0x80,
				0x40,
				0x20,
				0x10,
				0x08,
				0x04,
				0x02,
				0x01
			};
			u32 i = 0;
			u32 max = 0;
			
			if (*buf == NULL)
			{
				*buf = calloc(1, sizeof(char));
				**buf = 0;
				*aux = 0;
			}

			/* set up aux for remaining pixels */
			if (*aux == 0)
				*aux = bmap->infoheader.width;

			/* set up the number of pixels we need to process in this cycle */
			if (*aux > 8)
			{
				max = 8;
				*aux = *aux - 8;
			}
			else
			{
				max = *aux;
				*aux = 0;
			}
			
			temp = *data;

			while (i < max)
			{
				BITMAP_COLOR *cbuf = NULL;

				if (IsLittleEndian())
					temp = *data & values[i];
				else
					temp = *data & values[7 - i];
				

				if (temp)
					temp = 1;	

				cbuf = Neuro_GiveEBuf(bcolors, temp);

				Neuro_PutPixel(image, *x, (bmap->infoheader.height - 1) - *y, Neuro_MapRGB(cbuf->r, cbuf->g, cbuf->b));

				*x = *x + 1;
				i++;
			}

			if (*x > bmap->infoheader.width - 1)
			{
				*x = 0;
				*y = *y + 1;
			}
		}
		break;

		case 4:
		{
			/* will do a loop to get each 2 pixels from the data */
			u8 temp;
			/* double calc = 0; */
			
			/* u8 r, g, b; */
			
			const u8 values[2] = {
				0xF0,
				0x0F,
			};
			u32 i = 0;
			u32 max = 0;
			
			if (*buf == NULL)
			{
				*buf = calloc(1, sizeof(char));
				**buf = 0;
				*aux = 0;
			}

			/* set up aux for remaining pixels */
			if (*aux == 0)
				*aux = bmap->infoheader.width;

			/* set up the number of pixels we need to process in this cycle */
			if (*aux > 2)
			{
				max = 2;
				*aux = *aux - 2;
			}
			else
			{
				max = *aux;
				*aux = 0;
			}
			
			temp = *data;

			while (i < max)
			{
				BITMAP_COLOR *cbuf = NULL;
				
				if (IsLittleEndian())
				{
					temp = *data & values[i];
					
					if (temp > values[1])
						temp >>= 4;	
				}
				else
				{
					temp = *data & values[1 - i];
				
					if (temp > values[0])
						temp <<= 4;		
				}

				cbuf = Neuro_GiveEBuf(bcolors, temp);

				Neuro_PutPixel(image, *x, (bmap->infoheader.height - 1) - *y, Neuro_MapRGB(cbuf->r, cbuf->g, cbuf->b));
		
				*x = *x + 1;
				i++;
			}

			if (*x > bmap->infoheader.width - 1)
			{
				*x = 0;
				*y = *y + 1;
			}

		}
		break;

		case 8:
		{
			/* will get the single pixel from the data */
			u8 temp;
			/* double calc = 0; */
			
			/* u8 r, g, b; */
			
			u32 i = 0;
			u32 max = 0;
			
			if (*buf == NULL)
			{
				*buf = calloc(1, sizeof(char));
				**buf = 0;
				*aux = 0;
			}

			/* set up aux for remaining pixels */
			if (*aux == 0)
				*aux = bmap->infoheader.width;

			/* set up the number of pixels we need to process in this cycle */
			if (*aux > 1)
			{
				max = 1;
				*aux = *aux - 1;
			}
			else
			{
				max = *aux;
				*aux = 0;
			}
			
			temp = *data;

			while (i < max)
			{
				BITMAP_COLOR *cbuf = NULL;
				temp = *data;
				

				cbuf = Neuro_GiveEBuf(bcolors, temp);
				
				Neuro_PutPixel(image, *x, (bmap->infoheader.height - 1) - *y, Neuro_MapRGB(cbuf->r, cbuf->g, cbuf->b));

				*x = *x + 1;
				i++;
			}

			if (*x > bmap->infoheader.width - 1)
			{
				*x = 0;
				*y = *y + 1;
			}

		}
		break;

		case 16:
		{
			/* we do not support 16 bit because I think they  
			 * use 24 bit for those. we'll see through use.
			 * if not, I really think this depth is pointless...
			 */
		}
		break;

		case 24:
		{
			/* will need to gather the data for 2 other bytes to get a
			 * single pixel. We will use the auxiliary variable to keep
			 * track of where we are at in the gathering.
			 */

			if (!*buf)
			{
				*buf = calloc(3, sizeof(u8));
				*aux = 0;
			}
	
			(*buf)[*aux] = *data;
			*aux = *aux + 1;

			if (*aux >= 3)
			{
				*aux = 0;

				Neuro_PutPixel(image, *x, (bmap->infoheader.height - 1) - *y, Neuro_MapRGB((*buf)[2], (*buf)[1], (*buf)[0]));

				*x = *x + 1;
			}

			if (*x > bmap->infoheader.width - 1)
			{
				*x = 0;
				*y = *y + 1;
			}
		}
		break;

		case 32:
		{
			/* we do not support 32 bit because it is Very uncommon if it 
			 * actually exist. Pretty much the same as 16 bpp.
			 */
		}
		break;

		default:
		{
			/* an error occured */
		}
		break;

	}
}

static i8
processGradual_BMP(BMP_CTX *ctx, u32 loops)
{
	if (ctx == NULL)
	{
		NEURO_WARN("argument ctx is NULL", NULL);
		return -1;
	}

	if (ctx->f_bitmap == NULL)
	{
		NEURO_WARN("bitmap file descriptor is NULL", NULL);
		return -1;
	}

	if (ctx->i == 0)
	{	
		ctx->bmap = parse_bitmap_header(ctx->f_bitmap);
		
		/* TODO TODO XXX check here if the bitmap is valid or not
		 * first check for the BM word
		 * then we check for the size of the file in header and size
		 * we got when reading the file
		 */
		{
			
		}

		/* if it is valid, we create the buffers */
		Neuro_CreateEBuf(&ctx->bmap_colors);
		Neuro_SetcallbEBuf(ctx->bmap_colors, &clean_bmap_color);
		
		
		/* print_bitmap_infos(bmap); */
		
		/* process the bitmap(load it in memory) */
		ctx->i = 0;
		ctx->psize = ctx->bmap->header.size - (sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER));
		ctx->psize = ctx->psize - (ctx->bmap->infoheader.ncolors * 4);
		/* printf("data size %d\n", psize); */

		if (ctx->bmap->infoheader.ncolors)
		{
			process_palette(ctx->f_bitmap, ctx->bmap, ctx->bmap_colors);
		}

		/* we create the v_object 
		 *
		 * will need to put better values for the masks to support SDL.
		 */
		{
			u32 rmask = 0, gmask = 0, bmask = 0, amask = 0;

			if (IsLittleEndian())
			{
				switch (Lib_GetDefaultDepth())
				{
					case 16:
					{
						rmask = 0x0000f800;
						gmask = 0x000007e0;
						bmask = 0x0000001f;
						amask = 0x00000000;
					}
					break;

					case 24:
					{
						rmask = 0x00ff0000;
						gmask = 0x0000ff00;
						bmask = 0x000000ff;
						amask = 0x00000000;
					}
					break;


					default:
					break;
				}
			}
			else
			{
				switch (Lib_GetDefaultDepth())
				{
					case 16:
					{
						rmask = 0x0000001f;
						gmask = 0x000007e0;
						bmask = 0x0000f800;
						amask = 0x00000000;
					}
					break;
					
					case 24:
					{
						rmask = 0x0000ff00;
						gmask = 0x00ff0000;
						bmask = 0xff000000;
						amask = 0x00000000;
					}
					break;

					default:
					break;
				}
			}

			ctx->output = Neuro_CreateVObject(0, ctx->bmap->infoheader.width, ctx->bmap->infoheader.height, ctx->bmap->infoheader.bits, rmask, gmask, bmask, amask);

			if (ctx->output == NULL)
			{
				NEURO_WARN("Created output v_object is NULL", NULL);
				return -1;
			}
		}

		/* semi static values to skip bytes that form 32 bit chunks in the data */

		ctx->pixellen = (8 / (double)ctx->bmap->infoheader.bits);
		ctx->msize = ctx->pixellen * 4;

		/* we calculate the number of bytes there is per rows 
		 * this is mainly so we can know how much "alignment"
		 * bytes there is (which need to be skipped)
		 */
		{
			ctx->wmult = (u32)ctx->bmap->infoheader.bits / 8;

			if (ctx->wmult == 0)
				ctx->wmult++;

			ctx->wmult = ctx->wmult * ctx->bmap->infoheader.width;
		}

		ctx->increm = (u32)ctx->pixellen;

		if (ctx->increm == 0)
			ctx->increm++;

		ctx->x = (u32)(ctx->bmap->infoheader.width / ctx->msize);
		ctx->tmp = ctx->msize * ctx->x;
		ctx->tmp = (double)ctx->bmap->infoheader.width - ctx->tmp;
		ctx->tmp = ctx->tmp - 0.000001; /* to avoid bugs */

		ctx->x = 0;
#if USE_ZLIB
		gzseek(ctx->f_bitmap, ctx->bmap->header.offset, SEEK_SET);
#else /* NOT USE_ZLIB */
		fseek(ctx->f_bitmap, ctx->bmap->header.offset, SEEK_SET);
#endif /* NOT USE_ZLIB */

	} /* if i == 0 */
	/* else */ /* if i != 0 */
	{
		i32 initial = ctx->i;

		Lib_LockVObject(ctx->output);


		
		/* while (ctx->i < ctx->psize) */
		while (ctx->i < (initial + loops))
		{			
			if (ctx->tmp > 0)
			{
				/* skip bytes that are inside the bitmap for 
				 * filling purpose. (the data is purposely filled
				 * with 0 bits so the data is 32bits aligned)
				 */
				if (ctx->skip_i >= ctx->wmult)
				{
					ctx->calc = ctx->tmp / ctx->pixellen;
					ctx->skip_i = (u32)ctx->calc;
					if (ctx->skip_i < ctx->calc)
					{
						ctx->skip_i++;
					}
					ctx->skip_i = (4 - ctx->skip_i);
					ctx->i += ctx->skip_i;

#if USE_ZLIB
					gzseek(ctx->f_bitmap, ctx->bmap->header.offset + ctx->i, SEEK_SET);
#else /* NOT USE_ZLIB */
					fseek(ctx->f_bitmap, ctx->bmap->header.offset + ctx->i, SEEK_SET);
#endif /* NOT USE_ZLIB */
					
					/*
					printf("skipping %d bytes  wmult %d width %d tmp %f plen %f calc %f\n", 
							skip_i,
							wmult, 
							bmap->infoheader.width, 
							tmp, 
							pixellen, calc);
					*/
					
					ctx->skip_i = 0;
				}
				
				ctx->skip_i += ctx->increm;

				if (ctx->i >= ctx->psize)
					break;
			}

			
			fpdata8(ctx->f_bitmap, &ctx->DATA);

			process_bitmap2(ctx->bmap, ctx->output, ctx->palette, &ctx->DATA, 
					ctx->bmap_colors, &ctx->x, &ctx->y, &ctx->aux_var, &ctx->aux_buf);

			ctx->i++;
		}

		Lib_UnlockVObject(ctx->output);



		if (ctx->i >= ctx->psize)
		{ /* this bitmap finished being loaded, we free everything */

			/* to prevent further calls to be processed */
			ctx->i = -1;

			if (ctx->bmap)
				free(ctx->bmap);
			if (ctx->buf)
				free(ctx->buf);
			if (ctx->aux_buf)
				free(ctx->aux_buf);
			
			Neuro_CleanEBuf(&ctx->bmap_colors);

#if USE_ZLIB
			if (ctx->f_bitmap)
				gzclose(ctx->f_bitmap);
#else /* NOT USE_ZLIB */
			if (ctx->f_bitmap)
				fclose(ctx->f_bitmap);
#endif /* NOT USE_ZLIB */

			return 100;
		}

		/*Debug_Val(0, "((%d * 100) / %d) == %d\n",
				ctx->i, ctx->psize,
				(i8)((u32)((ctx->i * 100) / ctx->psize)));*/

		return (i8)((u32)(ctx->i * 100) / ctx->psize);
	}

	/* this never happens unless the image was already loaded */
	NEURO_TRACE("Useless call of the function #%d", ctx->i);
	return -1;
}

static v_object *
processFD_BMP(nFILE *f_bitmap)
{
	/* major (buffers) */
	EBUF *bmap_colors = NULL; /* the colors */
	u8 *buf = NULL; /* the buffer that will contain the content of the file */
	
	/* minor (mostly pointers and temporary variables) */
	register i32 i = 0; /* incremental variable */
	u32 skip_i = 0, x = 0, y = 0;
	
	u32 psize = 0; /* the full size of the pixels data */
	u8 *palette = NULL; /* the pointer to the palette if theres one */
	BITMAP_HDATA *bmap; /* this is how we will get informations about the bitmap */
	int aux_var = 0; /* auxiliary variable that can be used by external functions */
	char *aux_buf = NULL; /* same as aux_var but a buffer */
	double msize = 0;
	double calc = 0;
	double tmp = 0;
	u32 wmult = 0;
	double pixellen = 0;
	u32 increm = 0;
	u8 DATA;
	v_object *output; /* the image into which we will load the bitmap */
	
	if (f_bitmap == NULL)
		return NULL;
	
	bmap = parse_bitmap_header(f_bitmap);
	
	/* TODO TODO XXX check here if the bitmap is valid or not
	 * first check for the BM word
	 * then we check for the size of the file in header and size
	 * we got when reading the file
	 */
	{
		
	}

	/* if it is valid, we create the buffers */
	Neuro_CreateEBuf(&bmap_colors);
	Neuro_SetcallbEBuf(bmap_colors, &clean_bmap_color);
	
	
	/* print_bitmap_infos(bmap); */
	
	/* process the bitmap(load it in memory) */
	i = 0;
	psize = bmap->header.size - (sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER));
	psize = psize - (bmap->infoheader.ncolors * 4);
	/* printf("data size %d\n", psize); */

	if (bmap->infoheader.ncolors)
	{
		process_palette(f_bitmap, bmap, bmap_colors);
	}

	/* we create the v_object 
	 *
	 * will need to put better values for the masks to support SDL.
	 */
	{
		u32 rmask = 0, gmask = 0, bmask = 0, amask = 0;

		if (IsLittleEndian())
		{
			switch (Lib_GetDefaultDepth())
			{
				case 16:
				{
					rmask = 0x0000f800;
					gmask = 0x000007e0;
					bmask = 0x0000001f;
					amask = 0x00000000;
				}
				break;

				case 24:
				{
					rmask = 0x00ff0000;
					gmask = 0x0000ff00;
					bmask = 0x000000ff;
					amask = 0x00000000;
				}
				break;


				default:
				break;
			}
		}
		else
		{
			switch (Lib_GetDefaultDepth())
			{
				case 16:
				{
					rmask = 0x0000001f;
					gmask = 0x000007e0;
					bmask = 0x0000f800;
					amask = 0x00000000;
				}
				break;
				
				case 24:
				{
					rmask = 0x0000ff00;
					gmask = 0x00ff0000;
					bmask = 0xff000000;
					amask = 0x00000000;
				}
				break;

				default:
				break;
			}
		}
/*
		if (IsLittleEndian())
		{
			rmask = 0x0000f800;
			gmask = 0x000007e0;
			bmask = 0x0000001f;
			amask = 0x00000000;	
		}
		else
		{
			rmask = 0x0000001f;
			gmask = 0x000007e0;
			bmask = 0x0000f800;
			amask = 0x00000000;
		}
*/

		output = Neuro_CreateVObject(0, bmap->infoheader.width, bmap->infoheader.height, bmap->infoheader.bits, rmask, gmask, bmask, amask);

		if (output == NULL)
			return NULL;
	}

	/* semi static values to skip bytes that form 32 bit chunks in the data */

	pixellen = (8 / (double)bmap->infoheader.bits);
	msize = pixellen * 4;

	/* we calculate the number of bytes there is per rows 
	 * this is mainly so we can know how much "alignment"
	 * bytes there is (which need to be skipped)
	 */
	{
		wmult = (u32)bmap->infoheader.bits / 8;

		if (wmult == 0)
			wmult++;

		wmult = wmult * bmap->infoheader.width;
	}

	increm = (u32)pixellen;

	if (increm == 0)
		increm++;

	x = (u32)(bmap->infoheader.width / msize);
	tmp = msize * x;
	tmp = (double)bmap->infoheader.width - tmp;
	tmp = tmp - 0.000001; /* to avoid bugs */

	x = 0;
#if USE_ZLIB
	gzseek(f_bitmap, bmap->header.offset, SEEK_SET);
#else /* NOT USE_ZLIB */
	fseek(f_bitmap, bmap->header.offset, SEEK_SET);
#endif /* NOT USE_ZLIB */

	Lib_LockVObject(output);

	/* Debug_Val(0, "Image bits depth %d  tmp %f\n", bmap->infoheader.bits, tmp); */

	while (i < psize)
	{			
		if (tmp > 0)
		{
			/* skip bytes that are inside the bitmap for 
			 * filling purpose. (the data is purposely filled
			 * with 0 bits so the data is 32bits aligned)
			 */
			if (skip_i >= wmult)
			{
				calc = tmp / pixellen;
				skip_i = (u32)calc;
				if (skip_i < calc)
				{
					skip_i++;
				}
				skip_i = (4 - skip_i);
				i += skip_i;

#if USE_ZLIB
				gzseek(f_bitmap, bmap->header.offset + i, SEEK_SET);
#else /* NOT USE_ZLIB */
				fseek(f_bitmap, bmap->header.offset + i, SEEK_SET);
#endif /* NOT USE_ZLIB */
				
				/*
				printf("skipping %d bytes  wmult %d width %d tmp %f plen %f calc %f\n", 
						skip_i,
						wmult, 
						bmap->infoheader.width, 
						tmp, 
						pixellen, calc);
				*/
				
				skip_i = 0;
			}
			
			skip_i += increm;

			if (i >= psize)
				break;
		}

		
		fpdata8(f_bitmap, &DATA);

		process_bitmap2(bmap, output, palette, &DATA, 
				bmap_colors, &x, &y, &aux_var, &aux_buf);
		

		/* process_bitmap2(bmap, output, palette, &DATA, 
				bmap_colors, x, y, &aux_var, &aux_buf); */


		/* printf("i %d psize %d\n", i, psize); */
		/* Debug_Val(0, "current coord : %d,%d   %d\n", x, y, wmult); */
		/*
		if (tmp <= 0)
		{
			static u8 t = 2;

			if (t == 0 || x > bmap->infoheader.width - 1)
			{
				if (x < bmap->infoheader.width - 1)
					x++;
				else
				{
					x = 0;
					y++;
				}

				t = 2;
			}
			else
				t--;
		}
		*/

		i++;
	}

	Lib_UnlockVObject(output);
	
	if (bmap)
		free(bmap);
	if (buf)
		free(buf);
	if (aux_buf)
		free(aux_buf);
	
	Neuro_CleanEBuf(&bmap_colors);

#if USE_ZLIB
	if (f_bitmap)
		gzclose(f_bitmap);
#else /* NOT USE_ZLIB */
	if (f_bitmap)
		fclose(f_bitmap);
#endif /* NOT USE_ZLIB */

	return output;
}

/*-------------------- Global Functions ----------------------------*/

v_object *
readBitmapFile(const char *bitmap)
{
	nFILE *f_bitmap;

#if USE_ZLIB
	f_bitmap = gzopen(bitmap, "r"); /* can also be used for non compressed files */
#else /* NOT USE_ZLIB */
	f_bitmap = fopen(bitmap, "r");
#endif /* NOT USE_ZLIB */
	
	return processFD_BMP(f_bitmap);
}

/*-------------------- Poll ----------------------------------------*/

/* returns a percentage of progress */
i8
Bitmap_Poll(BMP_CTX *ctx)
{
	return processGradual_BMP(ctx, 512);
}

/*-------------------- Constructor Destructor ----------------------*/

BMP_CTX *
Bitmap_CreateCTX(const char *path)
{
	BMP_CTX *output;

	output = calloc(1, sizeof(BMP_CTX));

	if (output == NULL)
		return NULL;

	output->f_bitmap = gzopen(path, "r");

	if (output->f_bitmap == NULL)
	{
		free(output);
		return NULL;
	}

	return output;
}

v_object *
Bitmap_DestroyCTX(BMP_CTX *ctx)
{
	v_object *output = NULL;

	if (ctx == NULL)
		return NULL;

	output = ctx->output;

	free(ctx);

	return output;
}
