/* please refer better example: https://github.com/dvdhrm/docs/tree/master/drm-howto/ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "drm_helper.h"

int main()
{
	int i, j;

	/* init */
	DhHwInfo *dhHwInfo = DrmHelperInit(0);

	if (!dhHwInfo)
	{
		printf("Error DrmHelperInit\n");
		return -1;
	}

	printf("\n");
	printf("height: %d\n", dhHwInfo->height);
	printf("width: %d\n", dhHwInfo->width);
	printf("depth: %d\n", dhHwInfo->depth);
	printf("bpp: %d\n", dhHwInfo->bpp);
	printf("pitch: %d\n", dhHwInfo->pitch);
	printf("primarys: %d\n", dhHwInfo->count_planes[plane_type_primary]);
	printf("overlays: %d\n", dhHwInfo->count_planes[plane_type_overlay]);
	printf("cursors: %d\n", dhHwInfo->count_planes[plane_type_cursor]);
	printf("\n");

	{
		DhPlaneInfo *prim_fb = DrmHelperAllocFb(plane_type_primary, DRM_FORMAT_XRGB8888, 0, 0, 0, 0, 0);
		if (!prim_fb)
		{
			printf("Error DrmHelperAllocFb1\n");
		}
		else
		{
			printf("\n");
			printf("plane_uid: %d\n", prim_fb->plane_uid);
			printf("bpp: %d\n", prim_fb->bpp);
			printf("fb_size: %lu\n", prim_fb->fb_size);
			printf("height: %d\n", prim_fb->fbi.height);
			printf("width: %d\n", prim_fb->fbi.width);
			printf("pitches: %d\n", prim_fb->fbi.pitches[0]);
			printf("plane0: %p\n", prim_fb->map_bufs[0]);
			printf("pixel_format: %d\n", prim_fb->fbi.pixel_format);
			printf("\n");

			unsigned int clr_len = prim_fb->fbi.height / 24;
			unsigned int clr = 0;
			uint32_t *fb_map = prim_fb->map_bufs[0];
			for (i = 0; i < prim_fb->fbi.height; i++)
			{
				int t = i / clr_len;
				clr = 0xF << t;
				for (j = 0; j < prim_fb->fbi.width; j++)
				{
					fb_map[i * prim_fb->fbi.width + j] = clr;
				}
			}
		}
	}

	DhPlaneInfo *ovl1_fb;
	{
		ovl1_fb = DrmHelperAllocFb(plane_type_overlay, DRM_FORMAT_XRGB8888, 320, 240, 0, 0, 0);
		if (!ovl1_fb)
		{
			printf("Error DrmHelperAllocFb2\n");
		}
		else
		{
			DrmHelperSetZpos(ovl1_fb, 1);

			printf("\n");
			printf("plane_uid: %d\n", ovl1_fb->plane_uid);
			printf("bpp: %d\n", ovl1_fb->bpp);
			printf("fb_size: %lu\n", ovl1_fb->fb_size);
			printf("height: %d\n", ovl1_fb->fbi.height);
			printf("width: %d\n", ovl1_fb->fbi.width);
			printf("pitches: %d\n", ovl1_fb->fbi.pitches[0]);
			printf("plane0: %p\n", ovl1_fb->map_bufs[0]);
			printf("pixel_format: %d\n", ovl1_fb->fbi.pixel_format);
			printf("\n");

			// int DrmHelperSetZpos(int fb_id, uint8_t zpos);
			//  draw something
			unsigned int clr_len = ovl1_fb->fbi.height / 24;
			unsigned int clr = 0;
			uint32_t *fb_map = ovl1_fb->map_bufs[0];
			for (i = 0; i < ovl1_fb->fbi.height; i++)
			{
				int t = i / clr_len;
				clr = 0xF << t;
				for (j = 0; j < ovl1_fb->fbi.width; j++)
				{
					fb_map[i * ovl1_fb->fbi.width + j] = clr;
				}
			}
		}
	}
	{
		// DrmHelperSetPlanePos(o1, i, i);
		DhPlaneInfo *ovl2_fb = DrmHelperAllocFb(plane_type_overlay, DRM_FORMAT_XRGB8888, 50, 50, 200, 200, 0);
		if (!ovl2_fb)
		{
			printf("Error DrmHelperAllocFb3\n");
		}
		else
		{
			DrmHelperSetZpos(ovl2_fb, 2);
			if (ovl1_fb)
			{
				DrmHelperSetPlanePos(ovl1_fb, 0, 0); // update TODO
			}
			printf("\n");
			printf("plane_uid: %d\n", ovl2_fb->plane_uid);
			printf("bpp: %d\n", ovl2_fb->bpp);
			printf("fb_size: %lu\n", ovl2_fb->fb_size);
			printf("height: %d\n", ovl2_fb->fbi.height);
			printf("width: %d\n", ovl2_fb->fbi.width);
			printf("pitches: %d\n", ovl2_fb->fbi.pitches[0]);
			printf("plane0: %p\n", ovl2_fb->map_bufs[0]);
			printf("pixel_format: %d\n", ovl2_fb->fbi.pixel_format);
			printf("\n");

			unsigned int clr_len = ovl2_fb->fbi.height / 24;
			unsigned int clr = 0;
			uint32_t *fb_map = ovl2_fb->map_bufs[0];
			for (i = 0; i < ovl2_fb->fbi.height; i++)
			{
				int t = i / clr_len;
				clr = 0xF << t;
				for (j = 0; j < ovl2_fb->fbi.width; j++)
				{
					fb_map[i * ovl2_fb->fbi.width + j] = clr;
				}
			}

			int x = 0;
			for (i = 0; i < 400; i++)
			{
				DrmHelperSetPlanePos(ovl2_fb, i, i);
				usleep(10);
				x++;
			}

			for (i = 400; i > 1; i--)
			{
				DrmHelperSetPlanePos(ovl2_fb, x, i);
				usleep(10);
				x++;
			}
		}
	}
	sleep(5);

	/* destroy */
	DrmHelperFree();

	return 0;
}
