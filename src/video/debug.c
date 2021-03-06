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


/* debug.c
 * 
 * debugging functions (mostly dumps and checks) for
 * the painter's algorithm mainly and will probably
 * be home to all the debugging function for the
 * whole graphics module.
 */

/*-------------------- Extern Headers Including --------------------*/
#include <ebuf.h> /* we use the normal allocation functions from this */
#include <debug.h>

/*-------------------- Local Headers Including ---------------------*/
#include <global.h>

/*-------------------- Main Module Header --------------------------*/
#include "video.h"
#include <graphics.h>


/*--------------------      Other       ----------------------------*/

NEURO_MODULE_CHANNEL("video/debug");

/*-------------------- Global Variables ----------------------------*/

/*-------------------- Static Variables ----------------------------*/

/*-------------------- Static Prototypes ---------------------------*/



/*-------------------- Static Functions ----------------------------*/

/* very simple function to create a string memory segment */
static char *
s(char *str)
{
	char *result = NULL;
	result = malloc(strlen(str) + 1);
	strncpy(result, str, strlen(str) + 1);
	return result;
}

/*-------------------- Global Functions ----------------------------*/

char *
showDrawingType(int drawing_type)
{
	switch (drawing_type)
	{
		case TDRAW_STATIC:
		{
			return s("TDRAW_STATIC");
		}
		break;
		case TDRAW_DYNAMIC:
		{
			return s("TDRAW_DYNAMIC");
		}
		break;
		case TDRAW_DYNAMIC_CLEAN:
		{
			return s("TDRAW_DYNAMIC_CLEAN");
		}
		break;
		case TDRAW_SDRAWN:
		{
			return s("TDRAW_SDRAWN");
		}
		break;

		case TDRAW_SREDRAW:
		{
			return s("TDRAW_SREDRAW");
		}
		break;
		case TDRAW_VOLATILE:
		{
			return s("TDRAW_VOLATILE");
		}
		break;
		case TDRAW_SDESTROY:
		{
			return s("TDRAW_SDESTROY");
		}
		break;

		default:
			return NULL;
	}
}

/* debug function */
void
Graphics_DebugPrintQueue(void) 
{
	INSTRUCTION_ENGINE *cur;
	
	if (Neuro_EBufIsEmpty(Graphics_GetQueueBuffer()))
		return;
	/* cur = Neuro_GiveEBuf(_Queue, 0); */
	cur = Graphics_GetFirstElem();
	/* printf("Queue address %d\n", (int)cur); */
	
	while (cur != NULL)
	{
		char *typeStr = showDrawingType(cur->current->type);
		/*if (cur->current->type == TDRAW_SDRAWN)
		{
			cur = cur->next;
			continue;
		}*/
		
		TRACE(Neuro_s("layer #%d address &%x type %s", cur->current->layer, cur, 
				typeStr));
		free(typeStr);
		
		if (cur->next == Graphics_GetFirstElem())
		{
			ERROR("this element points to the beginning element");
			break;
		}
		else
			cur = cur->next;
	}
}

void
Graphics_DebugBufferQueue(EBUF *src)
{
	INSTRUCTION_ENGINE *cur;
	
	if (Neuro_EBufIsEmpty(Graphics_GetQueueBuffer()))
		return;

	cur = Graphics_GetFirstElem();
	
	if (verbose_missing_output == 1)
	{
		TRACE("Outputting initial queue for missing data");
	}

	while (cur != NULL)
	{	
		debug_status *tmp = NULL;
		Neuro_AllocEBuf(src, sizeof(debug_status*), sizeof(debug_status));

		tmp = Neuro_GiveCurEBuf(src);

		/* memcpy(tmp, cur, sizeof(INSTRUCTION_ENGINE*)); */
		tmp->missing = (u32)cur;

		if (verbose_missing_output == 1)
		{
			char *typeStr = showDrawingType(cur->current->type);
			TRACE(Neuro_s("layer #%d address &%x (%x) type %s", cur->current->layer, cur, 
					tmp->missing, typeStr));
			free(typeStr);
		}
		
		if (cur->next == Graphics_GetFirstElem())
		{
			ERROR("this element points to the beginning element");
			break;
		}
		else
			cur = cur->next;
	}
}

void
Graphics_DebugPrintMissing(EBUF *src)
{
	INSTRUCTION_ENGINE *cur;
	debug_status *tmp;
	EBUF *missing_list;
	u32 total;
	u8 found = 0;
	
	if (Neuro_EBufIsEmpty(Graphics_GetQueueBuffer()))
		return;

	if (Neuro_EBufIsEmpty(src))
		return;

	Neuro_CreateEBuf(&missing_list);

	cur = Graphics_GetFirstElem();
	
	while (cur != NULL)
	{
		total = Neuro_GiveEBufCount(src) - 1;
		found = 0;

		while (total-- > 0)
		{
			tmp = Neuro_GiveEBuf(src, total);

			/* Debug_Val(0, "is 0x%x the same as 0x%x? ", tmp->missing, cur); */
			if (tmp->missing == (u32)cur)
			{
				/* Debug_Val(0, "yes\n"); */
				found++;
			}
			/*else
				Debug_Val(0, "no\n");*/
		}

		if (found == 0)
		{
			struct debug_status *dstmp;
			Neuro_AllocEBuf(missing_list, sizeof(struct debug_status*), sizeof(struct debug_status));

			dstmp = Neuro_GiveCurEBuf(missing_list);

			dstmp->missing = (u32)cur;

			dstmp->duplicates = 0;
		}
		else if (found > 1)
		{
			struct debug_status *dstmp;
			Neuro_AllocEBuf(missing_list, sizeof(struct debug_status*), sizeof(struct debug_status));

			dstmp = Neuro_GiveCurEBuf(missing_list);

			dstmp->missing = (u32)cur;

			dstmp->duplicates = found;
		}
		
		if (cur->next == Graphics_GetFirstElem())
		{
			ERROR("This element points to the beginning element");
			break;
		}
		else
			cur = cur->next;
	}

	/* now that we filled the missing_list buffer, we can output
	 * its data. 
	 */

	TRACE("Debug queue status report");

	if (!Neuro_EBufIsEmpty(missing_list))
	{
		total = Neuro_GiveEBufCount(missing_list);
		TRACE(Neuro_s("We found %d missing/destroyed/duplicate addresses on %d\n", total, 
			Neuro_GiveEBufCount(Graphics_GetQueueBuffer())));
	}
	else
	{
		TRACE("NO missing/destroyed/duplicate addresses!");
		total = 0;
		Neuro_CleanEBuf(&missing_list);
		return;
	}
	
	if (total > 0)
		total--; 
	else
		total = 1; /* this ensures one loop to be done in the next section */

	while (total-- > 0)
	{
		struct debug_status *dstmp = NULL;

		if (Neuro_EBufIsEmpty(missing_list))
			break;

		dstmp = Neuro_GiveEBuf(missing_list, total);

		if (!dstmp)
			continue;

		if (dstmp->duplicates == 0)
			TRACE(Neuro_s("the address 0x%x is missing", dstmp->missing));
		else
			TRACE(Neuro_s("the address 0x%x is present %d times\n", dstmp->missing,
					dstmp->duplicates));
	}

	Neuro_CleanEBuf(&missing_list);
}

void
Graphics_DebugQueueIntegrityCheck(void)
{
	INSTRUCTION_ENGINE *cur; /* ordered element from the queue */
	u32 qtotal; /* the queue engine buffer total decrementor */
	INSTRUCTION_ENGINE *raw; /* unordered element from the buffer */
	RAW_ENGINE *data; /* the raw data pointer */
	u32 rtotal; /* the raw engine buffer total decrementor */
	u8 temp; /* used in the tests */

	/* those are booleans for the report this function will give */
	u8 correct_queue_integ = 0; /* integrity of the queue data; 
				      ie the content of each exist and
				      it exists in the raw_engine buffer */
	u8 correct_last_elem = 0; /* specific */
	u8 correct_queue_and_buffer = 0; /* the queue contains all the elements 
					from its buffer */
	u8 correct_raw_and_queue = 0; /* the raw data are all contained in the 
					queue */
	u8 correct_order_queue = 0; /* the queue has the correct ordering */

	/* this function will check the queue's 
	 * elements with all the unordered elements
	 * from the ebuf. 
	 * 
	 * -- correct_queue_integ
	 * It will first check if all the elements in 
	 * the queue exist in the ebuf and if the raw 
	 * content they contain also exist.
	 *
	 * -- correct_last_elem
	 * It will also check if the first and last 
	 * elements are the same as the variables pointers
	 * for those.
	 *
	 * -- correct_queue_and_buffer
	 * It will then see if all the unordered ebuf queue
	 * elements are contained (linked) in the ordered queue.
	 *
	 * -- correct_raw_and_queue
	 * It will check if ALL the elements from the RAW_ENGINE
	 * buffer are contained in the unordered queue buffer.
	 *
	 * -- correct_order_queue
	 * It will check if the order of the ordered queue is good.
	 *
	 * Take good note that this integrity check should in fact
	 * have its own module because we can't currently easily
	 * see if the first_element is correct or no...
	 *
	 * We currently ASSUME the first_element is correct and non
	 * NULL or else we don't do any tests.
	 */

	if (Graphics_GetFirstElem == NULL)
	{
		ERROR("failed, first_element is NULL");
		return;
	}

	/* we start to check the queue integrity 
	 * -- correct_queue_integ
	 */
	cur = Graphics_GetFirstElem();
	correct_queue_integ = 1;

	while (cur != NULL)
	{

		if (cur->current == NULL)
			correct_queue_integ = 0;

		cur = cur->next;
	}

	/* now we check if the last element is the
	 * correct one.
	 * -- correct_last_elem
	 */
	cur = Graphics_GetFirstElem();

	while (cur != NULL)
	{

		if (cur->next == NULL)
		{
			if (cur == Graphics_GetLastElem())
				correct_last_elem = 1;

			break;
		}

		cur = cur->next;
	}

	/* It will then see if all the unordered ebuf queue
	 * elements are contained (linked) in the ordered queue.
	 * -- correct_queue_and_buffer
	 */
	correct_queue_and_buffer = 1;

	qtotal = Neuro_GiveEBufCount(Graphics_GetQueueBuffer()) - 1;

	while (qtotal-- > 0)
	{
		raw = Neuro_GiveEBuf(Graphics_GetQueueBuffer(), qtotal);

		cur = Graphics_GetFirstElem();
		temp = 0;

		while (cur != NULL)
		{
			if (cur == raw)
				temp = 1;

			cur = cur->next;
		}

		if (temp == 0)
			correct_queue_and_buffer = 0;
	}

	/* It will check if ALL the elements from the RAW_ENGINE
	 * buffer are contained in the unordered queue buffer.
	 * -- correct_raw_and_queue
	 */
	rtotal = Neuro_GiveEBufCount(Graphics_GetRawBuffer()) - 1;
	correct_raw_and_queue = 1;

	while (rtotal-- > 0)
	{
		data = Neuro_GiveEBuf(Graphics_GetRawBuffer(), rtotal);

		qtotal = Neuro_GiveEBufCount(Graphics_GetQueueBuffer()) - 1;

		temp = 0;

		while (qtotal-- > 0)
		{
			raw = Neuro_GiveEBuf(Graphics_GetQueueBuffer(), qtotal);
			
			if (raw->current == data)
				temp = 1;
		}

		if (temp == 0)
			correct_raw_and_queue = 0;
	}
	/*
	 * It will check if the order of the ordered queue is good.
	 * -- correct_order_queue
	 */
	correct_order_queue = 1;
	
	cur = Graphics_GetFirstElem();
	temp = 0;

	while (cur != NULL)
	{
		
		if (cur->current)
		{
			if (cur->current->layer < temp)
				correct_order_queue = 0;

			temp = cur->current->layer;
		}

		cur = cur->next;
	}

	/* now we output our report */
	TRACE("Data Integrity Check Report");

	TRACE(Neuro_s("Queue integrity : %d", correct_queue_integ));
	TRACE(Neuro_s("Correct last element : %d", correct_last_elem));
	TRACE(Neuro_s("Queue and its buffer : %d", correct_queue_and_buffer));
	TRACE(Neuro_s("Raw and Queue buffer presence : %d", correct_raw_and_queue));
	TRACE(Neuro_s("Order of the queue : %d", correct_order_queue));
}

