/*! \file graphics.h
 * \brief graphics engine header
 *
 * The graphic engine is part of the most important engines in a game. 
 * It provides function to draw images on the screen.
 */

#ifndef __GRAPHICS_H
#define __GRAPHICS_H

#include "neuro_engine.h"
#include "other.h"

#include <stdlib.h>

/*! default : 60
 * the maximum amount of frames per seconds -deprecated-
 */
#define FRAMES_PER_SECOND 60

/*! default : 2
 * how many time the computer has to check 
 * out which one of the image has the lowest
 * layer(to be placed in the instruction
 * buffer).
 */
#define LAYER_CHECK_PER_FRAMES 2

/*! The Total screen width (can be changed before compile time to set the new size) */
#define SCREEN_X 800
/*! The Total screen height (can be changed before compile time to set the new size) */
#define SCREEN_Y 600

extern void *Neuro_GetScreenBuffer();

/*! Add a Drawing Instruction to the graphics loop. This function will
 * put take the image, place it in a special buffer and then draw the image
 * with the desired draw time(in comparison with other to draw images : layer) and 
 * place it on the screen depending on the given coordinates (also draws the portion
 * of the image that you set). This function is in general meant to be inside a function
 * that has a pointer callback set in graphics_AddDrawingElement().
 * @param [in] layer the drawing order by which the frame needs to be drawn on screen 
 * @param [in] src the image rectangle to draw (to select different parts of a single image)
 * @param [in] dst the coordinate where the image must be drawn to.
 * @param [in] surface the pointer to the image itself.
 */
extern void Neuro_AddDrawingInstruction(u8 layer, Rectan *src, Rectan *dst, void *surface);

/*! This function is used to add a callback function in a buffer. This buffer is looped and 
 * the callback functions(which are added using this function) are ran one by one every cycles. 
 * @param [in] func this is the callback function that the graphics engine will loop and 
 * run every cycles 
 */
extern void Neuro_AddDrawingElement(void (*func)());

/* directly draw a surface on the screen without
 * the layering feature and without the cleaning feature either (no feature actually :) ).
 */
extern void Neuro_AddDirectDrawing(Rectan *src, Rectan *dst, void *surface);

/*! use this function to set the background */
extern void Neuro_AddBackground(void *isurface);

/* refreshes the screen nicely also support backgrounds */
extern void Neuro_RefreshScreen();

/* tells neuro to clean the drawn images from the screen */
extern void Neuro_CycleClean();

/*! give the current fps */
extern void Neuro_GiveFPS(t_tick *output);

/* the u32 pointers will be given the current screen size in pixels */
extern void Neuro_GiveScreenSize(u32 *width, u32 *height);

/* draw a pixel on the screen */
extern void Neuro_PutPixel(u32 x, u32 y, u32 pixel);

/* get the color of a pixel on the screen */
extern u32 Neuro_GetPixel(u32 x, u32 y);

/* clean all the pixels that were drawn using the handled pixel drawer in this module */
extern void Neuro_CleanPixels();

/*! internal use Initialisation of the Graphics Engine */
extern int Graphics_Init(void);
/*! Poll of the Graphics Engine */
extern void Graphics_Poll(void);
/*! Cleaning of the Graphics Engine */
extern void Graphics_Clean(void);



#endif /* __GRAPHICS_H */
