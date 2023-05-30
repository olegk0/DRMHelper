/* please refer better example: https://github.com/dvdhrm/docs/tree/master/drm-howto/ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "drm_helper.h"

int main()
{
	int i, j;

	/* init */
	int reti = DrmHelperInit(0);

	if (reti)
	{
		printf("DrmHelperInit ret:%d\n", reti);
		return reti;
	}

	dh_fb_info_s fb_info;

	int prim = DrmHelperAllocFb(plane_type_primary, DRM_FORMAT_XRGB8888, 0, 0, 0, 0, 0, &fb_info);
	if (prim < 0)
	{
		printf("DrmHelperAllocFb ret:%d\n", prim);
	}
	else
	{
		printf("bpp: %d\n", fb_info.bpp);
		printf("fb_size: %lu\n", fb_info.fb_size);
		printf("height: %d\n", fb_info.fbi.height);
		printf("width: %d\n", fb_info.fbi.width);
		printf("pitches: %d\n", fb_info.fbi.pitches[0]);
		printf("plane0: %p\n", fb_info.map_bufs[0]);
		printf("pixel_format: %d\n", fb_info.fbi.pixel_format);

		// int DrmHelperSetZpos(int fb_id, uint8_t zpos);
		//  draw something
		unsigned int clr_len = fb_info.fbi.height / 24;
		unsigned int clr = 0;
		uint32_t *fb_map = fb_info.map_bufs[0];
		for (i = 0; i < fb_info.fbi.height; i++)
		{
			int t = i / clr_len;
			clr = 0xF << t;
			for (j = 0; j < fb_info.fbi.width; j++)
			{
				//			color = (double) (i * j) / (dev->height * dev->width) * 0xFF;
				//			*(dev->buf + i * dev->width + j) = (uint32_t) 0xFFFFFF & (color << 16 | color << 8 | 0);
				// *((uint32_t *)fb_info.map_bufs[0] + i * fb_info.fbi.width + j) = (uint32_t)0xFFFFFF & (fb_info.fbi.height * fb_info.fbi.width * 40);
				fb_map[i * fb_info.fbi.width + j] = clr;
			}
		}
	}

	int o1 = DrmHelperAllocFb(plane_type_overlay, DRM_FORMAT_XRGB8888, 320, 240, 0, 0, 1, &fb_info);
	if (o1 < 0)
	{
		printf("DrmHelperAllocFb ret:%d\n", o1);
	}
	else
	{
		DrmHelperSetZpos(o1, 1);

		printf("bpp: %d\n", fb_info.bpp);
		printf("fb_size: %lu\n", fb_info.fb_size);
		printf("height: %d\n", fb_info.fbi.height);
		printf("width: %d\n", fb_info.fbi.width);
		printf("pitches: %d\n", fb_info.fbi.pitches[0]);
		printf("plane0: %p\n", fb_info.map_bufs[0]);
		printf("pixel_format: %d\n", fb_info.fbi.pixel_format);

		// int DrmHelperSetZpos(int fb_id, uint8_t zpos);
		//  draw something
		unsigned int clr_len = fb_info.fbi.height / 24;
		unsigned int clr = 0;
		uint32_t *fb_map = fb_info.map_bufs[0];
		for (i = 0; i < fb_info.fbi.height; i++)
		{
			int t = i / clr_len;
			clr = 0xF << t;
			for (j = 0; j < fb_info.fbi.width; j++)
			{
				//			color = (double) (i * j) / (dev->height * dev->width) * 0xFF;
				//			*(dev->buf + i * dev->width + j) = (uint32_t) 0xFFFFFF & (color << 16 | color << 8 | 0);
				//*((uint32_t *)fb_info.map_bufs[0] + i * fb_info.fbi.width + j) = (uint32_t)0xFFFFFF & (fb_info.fbi.height * fb_info.fbi.width * 40);
				fb_map[i * fb_info.fbi.width + j] = clr;
			}
		}
	}

	// DrmHelperSetPlanePos(o1, i, i);

	int o2 = DrmHelperAllocFb(plane_type_overlay, DRM_FORMAT_XRGB8888, 50, 50, 200, 200, 0, &fb_info);
	DrmHelperSetPlanePos(o1, 0, 0);
	if (o2 < 0)
	{
		printf("DrmHelperAllocFb ret:%d\n", o2);
	}
	else
	{
		DrmHelperSetZpos(o2, 2);

		unsigned int clr_len = fb_info.fbi.height / 24;
		unsigned int clr = 0;
		uint32_t *fb_map = fb_info.map_bufs[0];
		for (i = 0; i < fb_info.fbi.height; i++)
		{
			int t = i / clr_len;
			clr = 0xF << t;
			for (j = 0; j < fb_info.fbi.width; j++)
			{
				//			color = (double) (i * j) / (dev->height * dev->width) * 0xFF;
				//			*(dev->buf + i * dev->width + j) = (uint32_t) 0xFFFFFF & (color << 16 | color << 8 | 0);
				//*((uint32_t *)fb_info.map_bufs[0] + i * fb_info.fbi.width + j) = (uint32_t)0xFFFFFF & (fb_info.fbi.height * fb_info.fbi.width * 40);
				fb_map[i * fb_info.fbi.width + j] = clr;
			}
		}

		int x = 0;
		for (i = 0; i < 400; i++)
		{
			DrmHelperSetPlanePos(o2, i, i);
			usleep(10);
			x++;
		}

		for (i = 400; i > 1; i--)
		{
			DrmHelperSetPlanePos(o2, x, i);
			usleep(10);
			x++;
		}
	}

	sleep(5);

	/* destroy */
	DrmHelperFree();

	return 0;
}
