/*
 * 2023
 *
 * olegvedi@gmail.com
 *
 * used parts from:
 *
 *
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <xf86drm.h>

#include "drm_helper.h"

typedef struct _plane_s
{
	uint32_t plane_id;		// ro
	// Formats supported
	uint32_t count_formats; // ro
	uint32_t *formats;		// ro
	// Props
	uint32_t count_props;		   // ro
	drmModePropertyPtr properties; // ro
	// Zpos
	int zpos_min; // ro
	int zpos_max; // ro
	// FB
	DhPlaneInfo fb_info;
	uint16_t crtc_w, crtc_h;
} plane_s;

typedef struct _dev_s
{
	int fd;
	//	connector_s *connectors;
	uint32_t conn_id, enc_id;
	// CRTC
	uint32_t crtc_id;
	drmModeCrtc crtc;
	drmModeModeInfo mode_info;
	drmModeCrtcPtr saved_crtc;
	// Primary Fb
	drmModeFB primary_fb_info;
	DhPlaneInfo primary_fb;
	//  Planes
	int count_planes;
	plane_s *planes;
	//
	DhHwInfo dh_hw_info;
} dev_s;

static dev_s *dev = NULL;

static int drm_open(const char *path)
{
	int fd;
	uint64_t has_dumb;

	if ((fd = open(path, O_RDWR | O_CLOEXEC)) < 0)
	{
		fprintf(stderr, "drmOpen failed or doesn't have dumb buffer");
	}
	else
	{
		/* check capability */
		if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || has_dumb == 0)
		{
			fprintf(stderr, "drmGetCap DRM_CAP_DUMB_BUFFER failed or doesn't have dumb buffer");
			close(fd);
			fd = -1;
		}
	}
	return fd;
}

static int drm_find_crt(void)
{
	int ret = -ENXIO;

	drmModeRes *res = drmModeGetResources(dev->fd);
	if (!res)
	{
		fprintf(stderr, "drmModeGetResources() failed");
	}
	else
	{
		// if (res->count_connectors > 0) {
		// dev->connectors = (connector_s *)calloc(res->count_connectors,sizeof(connector_s));
		//  connectors loop
		int i = 0;
		while (i < res->count_connectors && ret)
		{
			dev->crtc_id = 0;
			// connector_s *connector = &dev->connectors[i];
			// connector->conn_id = -1;
			drmModeConnector *conn = drmModeGetConnector(dev->fd, res->connectors[i]);
			if (!conn)
			{
				fprintf(stderr, "drmModeGetConnector() failed");
			}
			else
			{
				if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0)
				{
					// connector->conn_id = conn->connector_id;
					// connector->enc_id = conn->encoder_id;
					// memcpy(&connector->modeInfo, &conn->modes[0], sizeof(drmModeModeInfo));
					//  FIXME: use default encoder/crtc pair
					drmModeEncoderPtr enc = drmModeGetEncoder(dev->fd, conn->encoder_id);
					if (!enc)
					{
						fprintf(stderr, "drmModeGetEncoder() failed");
					}
					else
					{
						dev->crtc_id = enc->crtc_id;
						dev->conn_id = conn->connector_id;
						dev->enc_id = conn->encoder_id;
						drmModeFreeEncoder(enc);
					}
				}

				if (dev->crtc_id > 0)
				{
					drmModeCrtcPtr crtc = drmModeGetCrtc(dev->fd, dev->crtc_id);
					if (!crtc)
					{
						fprintf(stderr, "drmModeGetCrtc() failed");
						dev->crtc_id = 0;
					}
					else
					{
						memcpy(&dev->crtc, crtc, sizeof(drmModeCrtc));
						memcpy(&dev->mode_info, &conn->modes[0], sizeof(drmModeModeInfo));
						drmModeFBPtr fb = drmModeGetFB(dev->fd, crtc->buffer_id);
						if (!fb)
						{
							fprintf(stderr, "drmModeGetFB2() failed\n");
						}
						else
						{
							dev->primary_fb.type = plane_type_primary;
							dev->dh_hw_info.count_planes[plane_type_primary] = 1;
							dev->dh_hw_info.height = fb->height;
							dev->dh_hw_info.width = fb->width;
							dev->dh_hw_info.bpp = fb->bpp;
							dev->dh_hw_info.depth = fb->depth;
							dev->dh_hw_info.pitch = fb->pitch;
							memcpy(&dev->primary_fb_info, fb, sizeof(drmModeFB));
							ret = 0;
							drmModeFreeFB(fb);
						}
						drmModeFreeCrtc(crtc);
					}
				}
				drmModeFreeConnector(conn);
			}

			i++;
		} // loop connectors
		/*} else {
			fprintf(stderr, "connectors not found");
		}*/
		drmModeFreeResources(res);
	}
	return ret;
}

static int parse_planes(dev_s *dev)
{
	int ret = -ENXIO;
	drmModePlaneRes *plane_resources = drmModeGetPlaneResources(dev->fd);
	if (!plane_resources)
	{
		fprintf(stderr, "drmModeGetPlaneResources failed: %s\n", strerror(errno));
	}
	else
	{
		dev->planes = (plane_s *)calloc(plane_resources->count_planes, sizeof(plane_s));
		if (!dev->planes)
		{
			fprintf(stderr, "error memory alloc for cur_plane->formats\n");
		}
		else
		{
			dev->count_planes = plane_resources->count_planes;
			printf("Planes:\n");
			printf("id\tcrtc\tfb\tCRTC x,y\tx,y\tgamma size\tpossible crtcs\n");
			for (int pli = 0; pli < plane_resources->count_planes; pli++)
			{
				plane_s *cur_plane = &dev->planes[pli];
				cur_plane->fb_info.plane_uid = pli + 1;
				drmModePlane *ovr = drmModeGetPlane(dev->fd, plane_resources->planes[pli]);
				if (!ovr)
				{
					printf("drmModeGetPlane failed: %s\n", strerror(errno));
					continue;
				}
				cur_plane->count_formats = ovr->count_formats;
				cur_plane->formats = malloc(sizeof(cur_plane->formats) * ovr->count_formats);
				if (cur_plane->formats)
				{
					memcpy(cur_plane->formats, ovr->formats, sizeof(cur_plane->formats) * ovr->count_formats);
				}
				else
				{
					fprintf(stderr, "error memory alloc for cur_plane->formats\n");
				}
				printf("%d\t%d\t%d\t%d,%d\t\t%d,%d\t%-8d\t0x%08x\n", ovr->plane_id, ovr->crtc_id, ovr->fb_id,
					   ovr->crtc_x, ovr->crtc_y, ovr->x, ovr->y, ovr->gamma_size, ovr->possible_crtcs);

				cur_plane->plane_id = ovr->plane_id;
				drmModeObjectPropertiesPtr props =
					drmModeObjectGetProperties(dev->fd, plane_resources->planes[pli], DRM_MODE_OBJECT_PLANE);
				if (props)
				{
					printf("Property:\n");
					printf("id\tname\tvalue\n");

					cur_plane->count_props = 0;
					cur_plane->properties =
						(drmModePropertyPtr)calloc(props->count_props, sizeof(drmModePropertyRes));
					if (!cur_plane->properties)
					{
						fprintf(stderr, "error memory alloc for cur_plane->properties\n");
					}
					else
					{
						for (int pri = 0; pri < props->count_props; pri++)
						{
							drmModePropertyPtr prop = drmModeGetProperty(dev->fd, props->props[pri]);
							if (!prop)
							{
								fprintf(stderr, "drmModeGetProperty failed: %s\n", strerror(errno));
							}
							else
							{
								memcpy(&cur_plane->properties[cur_plane->count_props], prop,
									   sizeof(drmModePropertyRes));
								cur_plane->count_props++;
								printf("%d\t%s\t%" PRIu64 "\n", prop->prop_id, prop->name,
									   props->prop_values[pri]);
								if (strcmp(prop->name, "zpos") == 0)
								{
									// DRM_MODE_PROP_RANGE or DRM_MODE_PROP_SIGNED_RANGE
									if (prop->count_values > 1)
									{
										cur_plane->zpos_min = prop->values[0];
										cur_plane->zpos_max = prop->values[1];
									}
									cur_plane->fb_info.zpos = props->prop_values[pri];
									// set_plane_property(dev->fd, ovr->plane_id, prop->name, prop->prop_id, 2); //0-bg 1-fg 2-cursor
								}
								else if (strcmp(prop->name, "type") == 0)
								{
									cur_plane->fb_info.type = props->prop_values[pri];
									if (cur_plane->fb_info.type <= plane_type_cursor)
									{
										dev->dh_hw_info.count_planes[cur_plane->fb_info.type]++;
									}
								}
								ret = 0;
								// dump_prop(dev->fd, prop, props->props[pri], props->prop_values[pri]);
								drmModeFreeProperty(prop);
							}
						}
						// free(cur_plane->properties);
						// cur_plane->properties = NULL;
					}
					drmModeFreeObjectProperties(props);
				}
				// set_plane(dev->fd, dev, ovr->plane_id, 200, 200, 2);
				drmModeFreePlane(ovr);
			}
			printf("\n");
		}
		drmModeFreePlaneResources(plane_resources);
	}
	return ret;
}

static int set_property_by_id(int object_id, uint32_t object_type, uint32_t prop_id, uint64_t value)
{
	//	if (!dev->use_atomic)
	int ret = drmModeObjectSetProperty(dev->fd, object_id, object_type, prop_id, value);
	//	else
	//		ret = drmModeAtomicAddProperty(dev->req, plane_id, prop_id, value);
	if (ret < 0)
	{
		fprintf(stderr, "failed to set property %d for object %d to %" PRIu64 ": %s\n",
				prop_id, object_id, value, strerror(errno));
	}

	return ret;
}

static int set_plane_property_by_name(plane_s *plane, char *prop_name, uint64_t value)
{
	for (int i = 0; i < plane->count_props; i++)
	{
		if (strcmp(plane->properties[i].name, prop_name) == 0)
		{
			return set_property_by_id(plane->plane_id, DRM_MODE_OBJECT_PLANE,
									  plane->properties[i].prop_id, value);
		}
	}

	return -ENXIO;
}

//************************************ BO management ***********************************
static int bo_map(int fd, DhPlaneInfo *fb_info)
{
	void *map;
	int ret;
	uint64_t offset;

	ret = drmModeMapDumbBuffer(fd, fb_info->fbi.handles[0], &offset);
	if (ret)
		return ret;

	map = mmap(0, fb_info->fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
	if (map == MAP_FAILED)
		return -EINVAL;

	fb_info->map_bufs[0] = map;

	return 0;
}

static void bo_unmap(DhPlaneInfo *fb_info)
{
	if (!fb_info->map_bufs[0])
		return;

	munmap(fb_info->map_bufs[0], fb_info->fb_size);
	fb_info->map_bufs[0] = NULL;
}

void bo_destroy(int fd, DhPlaneInfo *fbi)
{
	int ret;

	ret = drmModeDestroyDumbBuffer(fd, fbi->fbi.handles[0]);
	if (ret)
		fprintf(stderr, "failed to destroy dumb buffer: %s\n", strerror(errno));
}

int bo_create(int fd, DhPlaneInfo *fb_info)
{
	unsigned int virtual_height;
	unsigned int bpp;
	int ret = -EINVAL;

	switch (fb_info->fbi.pixel_format)
	{
	case DRM_FORMAT_C8:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		bpp = 8;
		break;

	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		bpp = 16;
		break;

	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
		bpp = 24;
		break;

	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRX1010102:
		bpp = 32;
		break;

	case DRM_FORMAT_XRGB16161616F:
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ARGB16161616F:
	case DRM_FORMAT_ABGR16161616F:
		bpp = 64;
		break;

	default:
		fprintf(stderr, "unsupported format 0x%08x\n", fb_info->fbi.pixel_format);
		return ret;
	}

	fb_info->bpp = bpp;

	switch (fb_info->fbi.pixel_format)
	{
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		virtual_height = fb_info->fbi.height * 3 / 2;
		break;

	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		virtual_height = fb_info->fbi.height * 2;
		break;

	default:
		virtual_height = fb_info->fbi.height;
		break;
	}

	ret = drmModeCreateDumbBuffer(fd, fb_info->fbi.width, virtual_height, fb_info->bpp, 0, &fb_info->fbi.handles[0],
								  &fb_info->fbi.pitches[0], &fb_info->fb_size);

	if (ret)
	{
		fprintf(stderr, "failed to create dumb buffer: %s\n", strerror(errno));
		return ret;
	}

	ret = bo_map(fd, fb_info);
	if (ret)
	{
		fprintf(stderr, "failed to map buffer: %s\n", strerror(-errno));
		bo_destroy(fd, fb_info);
		return ret;
	}

	/* just testing a limited # of formats to test single
	 * and multi-planar path.. would be nice to add more..
	 */
	switch (fb_info->fbi.pixel_format)
	{
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		fb_info->fbi.offsets[0] = 0;
		// handles[0] = bo->handle;
		// pitches[0] = bo->pitch;
		// fbi->map_bufs[0] = fbi->fb_buf;
		break;

	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		fb_info->fbi.offsets[0] = 0;
		// handles[0] = bo->handle;
		// pitches[0] = bo->pitch;
		fb_info->fbi.pitches[1] = fb_info->fbi.pitches[0];
		fb_info->fbi.offsets[1] = fb_info->fbi.pitches[0] * fb_info->fbi.height;
		fb_info->fbi.handles[1] = fb_info->fbi.handles[0];
		// fbi->map_bufs[0] = fbi->fb_buf;
		fb_info->map_bufs[1] = fb_info->map_bufs[0] + fb_info->fbi.offsets[1];
		break;

	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		fb_info->fbi.offsets[0] = 0;
		// handles[0] = bo->handle;
		// pitches[0] = bo->pitch;
		fb_info->fbi.pitches[1] = fb_info->fbi.pitches[0] / 2;
		fb_info->fbi.offsets[1] = fb_info->fbi.pitches[0] * fb_info->fbi.height;
		fb_info->fbi.handles[1] = fb_info->fbi.handles[0];
		fb_info->fbi.pitches[2] = fb_info->fbi.pitches[1];
		fb_info->fbi.offsets[2] = fb_info->fbi.offsets[1] + fb_info->fbi.pitches[1] * fb_info->fbi.height / 2;
		fb_info->fbi.handles[2] = fb_info->fbi.handles[0];
		// fbi->map_bufs[0] = fbi->fb_buf;
		fb_info->map_bufs[1] = fb_info->map_bufs[0] + fb_info->fbi.offsets[1];
		fb_info->map_bufs[2] = fb_info->map_bufs[0] + fb_info->fbi.offsets[2];
		break;

	case DRM_FORMAT_C8:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRA1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_XRGB16161616F:
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ARGB16161616F:
	case DRM_FORMAT_ABGR16161616F:
		fb_info->fbi.offsets[0] = 0;
		// handles[0] = bo->handle;
		// pitches[0] = bo->pitch;
		// fbi->map_bufs[0] = virtual;
		break;
	}

	//	util_fill_pattern(format, pattern, planes, width, height, pitches[0]);
	//	bo_unmap(bo);

	return ret;
}
//***********************************************************************
static int bo_fb_create(int fd, DhPlaneInfo *fb_info)
{
	int ret = bo_create(fd, fb_info);

	if (ret == 0)
	{
		ret = drmModeAddFB2(fd, fb_info->fbi.width, fb_info->fbi.height, fb_info->fbi.pixel_format, fb_info->fbi.handles,
							fb_info->fbi.pitches, fb_info->fbi.offsets, &fb_info->fbi.fb_id, 0);
		if (ret)
		{
			fprintf(stderr, "failed to add fb (%ux%u): %s\n", fb_info->fbi.width, fb_info->fbi.height, strerror(errno));
			bo_destroy(fd, fb_info);
			bo_unmap(fb_info);
		}
	}
	return ret;
}

//****************************** Primary*****************************************
static void free_primary_fb()
{
	fprintf(stderr, "free_primary_fb\n");
	if (dev->saved_crtc)
	{
		drmModeSetCrtc(dev->fd, dev->saved_crtc->crtc_id, dev->saved_crtc->buffer_id, dev->saved_crtc->x,
					   dev->saved_crtc->y, &dev->conn_id, 1, &dev->saved_crtc->mode);
		drmModeFreeCrtc(dev->saved_crtc);
		dev->saved_crtc = NULL;
	}
	if (dev->primary_fb.map_bufs[0])
	{
		drmModeRmFB(dev->fd, dev->primary_fb.fbi.fb_id);
		bo_destroy(dev->fd, &dev->primary_fb);
		bo_unmap(&dev->primary_fb);
	}
}

static int alloc_primary_fb(uint32_t fourcc_format, uint16_t width, uint16_t height, uint16_t x, uint16_t y)
{
	if (dev->primary_fb.map_bufs[0])
	{
		fprintf(stderr, "primary_fb in use\n");
		return -EBUSY;
	}

	DhPlaneInfo *primary_fb = &dev->primary_fb;

	if (width > 0)
	{
		primary_fb->fbi.width = width;
	}
	else
	{
		primary_fb->fbi.width = dev->primary_fb_info.width;
		//		x = 0;
	}

	if (height > 0)
	{
		primary_fb->fbi.height = height;
	}
	else
	{
		primary_fb->fbi.height = dev->primary_fb_info.height;
		//		y = 0;
	}
	primary_fb->fbi.pixel_format = fourcc_format;

	// primary_fb->depth = dev->primary_fb_info.depth;
	//  RET primary_fb->fbi.fb_id;
	//  RET primary_fb->fbi.offsets;
	//  RET primary_fb->fbi.pitches;
	//  RET primary_fb->fbi.handles;
	//  primary_fb->fbi.flags;
	//  primary_fb->fbi.modifier;

	/* just use single plane format for now.. */
	int ret = bo_fb_create(dev->fd, primary_fb);
	if (ret == 0)
	{
		printf("fd:%d, primary_fb, dev->crtc_id:%d, out_fb_id:%d\n", dev->fd, dev->crtc_id, primary_fb->fbi.fb_id);
		dev->saved_crtc = drmModeGetCrtc(dev->fd, dev->crtc_id); // must store crtc data
		if (!dev->saved_crtc)
		{
			fprintf(stderr, "drmModeGetCrtc failed\n");
		}
		else
		{
			if (drmModeSetCrtc(dev->fd, dev->crtc_id, dev->primary_fb.fbi.fb_id, x, y, &dev->conn_id, 1,
							   &dev->mode_info) == 0)
			{
				primary_fb->x = x;
				primary_fb->y = y;
				return 0;
			}
			fprintf(stderr, "drmModeSetCrtc failed\n");
		}

		fprintf(stderr, "failed to enable primary_fb: %s\n", strerror(errno));
		free_primary_fb();
	}
	return ret;
}

//****************************** Planes*****************************************

int set_plane_property_zpos(plane_s *plane, uint8_t value)
{
	int ret = 0;

	if (value < plane->zpos_min || value > plane->zpos_max)
	{
		fprintf(stderr, "value zpos (%d) must be in range: [%d:%d]", value, plane->zpos_min, value > plane->zpos_max);
		ret = -EINVAL;
	}
	else
	{
		if (plane->fb_info.zpos != value)
		{
			ret = set_plane_property_by_name(plane, "zpos", value);
			if (ret == 0)
			{
				plane->fb_info.zpos = value;
			}
		}
	}

	return ret;
}

void free_plane(plane_s *plane)
{
	set_plane_property_zpos(plane, 0);
	if (plane->fb_info.map_bufs[0])
	{
		drmModeRmFB(dev->fd, plane->fb_info.fbi.fb_id);
		bo_destroy(dev->fd, &plane->fb_info);
		bo_unmap(&plane->fb_info);
	}
}

static int alloc_plane(plane_s *plane, uint32_t fourcc_format, uint16_t width, uint16_t height,
					   uint16_t x, uint16_t y, uint8_t fullscreen)
{
	if (plane->fb_info.map_bufs[0])
	{
		fprintf(stderr, "plane %d in use\n", plane->plane_id);
		return -EBUSY;
	}

	int crtc_w, crtc_h;

	plane->fb_info.fbi.width = width;
	plane->fb_info.fbi.height = height;
	plane->fb_info.fbi.pixel_format = fourcc_format;

	// RET plane->fb_info.fbi.fb_id;
	// RET plane->fb_info.fbi.offsets;
	// RET plane->fb_info.fbi.pitches;
	// RET plane->fb_info.fbi.handles;
	// plane->fb_info.fbi.flags;
	// plane->fb_info.fbi.modifier;

	/* just use single plane format for now.. */
	int ret = bo_fb_create(dev->fd, &plane->fb_info);
	if (ret == 0)
	{
		if (width == 0)
		{
			width = dev->mode_info.hdisplay;
		}

		if (height == 0)
		{
			height = dev->mode_info.vdisplay;
		}

		if (fullscreen)
		{
			crtc_w = dev->mode_info.hdisplay;
			crtc_h = dev->mode_info.vdisplay;
		}
		else
		{
			crtc_w = width;
			crtc_h = height;
		}

		printf("fd:%d, plane_id:%d, dev->crtc_id:%d, out_fb_id:%d\n", dev->fd, plane->plane_id, dev->crtc_id,
			   plane->fb_info.fbi.fb_id);
		/* note src coords (last 4 args) are in Q16 format */
		ret = drmModeSetPlane(dev->fd, plane->plane_id, dev->crtc_id, plane->fb_info.fbi.fb_id, 0, x, y, crtc_w,
							  crtc_h, 0, 0, width << 16, height << 16);
		if (ret)
		{
			fprintf(stderr, "failed to enable plane: %s\n", strerror(errno));
			free_plane(plane);
		}
		else
		{
			plane->fb_info.x = x;
			plane->fb_info.y = y;
			plane->crtc_w = crtc_w;
			plane->crtc_h = crtc_h;
			plane->fb_info.fullscreen = fullscreen;
		}
	}
	return ret;
}

void free_planes()
{
	fprintf(stderr, "free_planes\n");
	if (dev->planes)
	{
		for (int i = 0; i < dev->count_planes; i++)
		{
			plane_s *plane = &dev->planes[i];
			free_plane(plane);
			if (plane->formats)
			{
				free(plane->formats);
				plane->formats = NULL;
			}
			if (plane->properties)
			{
				free(plane->properties);
				plane->properties = NULL;
			}
		}
		free(dev->planes);
		dev->planes = NULL;
	}
}

/*
static int set_plane_property_by_name(int plane_pid, char *prop_name, uint64_t value)
{
	for (int i = 0; i < dev->map_bufs[plane_pid].count_props; i++)
	{
		if (strcmp(dev->map_bufs[plane_pid].properties[i].name, prop_name) == 0)
		{
			int ret = set_property_by_id(dev->map_bufs[plane_pid].plane_id, DRM_MODE_OBJECT_PLANE,
										 dev->map_bufs[plane_pid].properties[i].prop_id, value);
			break;
		}
	}

	return -ENXIO;
}
*/

static int set_plane_pos(plane_s *plane, int x, int y)
{
	if (x >= dev->mode_info.hdisplay || y >= dev->mode_info.vdisplay)
	{
		return -EINVAL;
	}
	/* note src coords (last 4 args) are in Q16 format */
	return drmModeSetPlane(dev->fd, plane->plane_id, dev->crtc_id, plane->fb_info.fbi.fb_id, 0, x, y, plane->crtc_w,
						   plane->crtc_h, 0, 0, plane->fb_info.fbi.width << 16, plane->fb_info.fbi.height << 16);
}

//***************************Extern*************************
int DrmHelperSetZpos(DhPlaneInfo *fb_info, uint8_t value)
{
	int ret = -EINVAL;
	if (fb_info->plane_uid > 0 && fb_info->plane_uid <= dev->count_planes)
	{
		plane_s *plane = &dev->planes[fb_info->plane_uid - 1];
		ret = set_plane_property_zpos(plane, value);
		if (ret == 0)
		{
			set_plane_pos(plane, plane->fb_info.x, plane->fb_info.y);
		}
	}
	else
	{
		fprintf(stderr, "DrmHelperSetZpos: Invalid plane: %d\n", fb_info->plane_uid);
	}
	return ret;
}

int DrmHelperSetPlanePos(DhPlaneInfo *fb_info, int x, int y)
{
	int ret = -EINVAL;
	if (fb_info->plane_uid > 0 && fb_info->plane_uid <= dev->count_planes)
	{
		ret = set_plane_pos(&dev->planes[fb_info->plane_uid - 1], x, y);
	}
	else
	{
		fprintf(stderr, "DrmHelperSetPlanePos: Invalid plane: %d\n", fb_info->plane_uid);
	}
	return ret;
}

DhPlaneInfo *DrmHelperAllocFb(DhPlaneType type, uint32_t fourcc_format, uint16_t width, uint16_t height,
							  uint16_t x, uint16_t y, uint8_t fullscreen)
{
	DhPlaneInfo *fbi = NULL;
	switch (type)
	{
	case plane_type_cursor:
	case plane_type_overlay:
		for (int i = 0; i < dev->count_planes; i++)
		{
			plane_s *plane = &dev->planes[i];
			if (plane->fb_info.type == type && plane->fb_info.map_bufs[0] == NULL)
			{
				int rt = alloc_plane(plane, fourcc_format, width, height, x, y, fullscreen);
				if (rt == 0)
				{
					fbi = &plane->fb_info;
					break;
				}
			}
		}
		break;
	case plane_type_primary:
	{
		int rt = alloc_primary_fb(fourcc_format, width, height, x, y);
		if (rt == 0)
		{
			fbi = &dev->primary_fb;
		}
	}
	break;
	default:
		fprintf(stderr, "Invalid fb type: %d\n", type);
	}

	return fbi;
}

int DrmHelperFreeFb(DhPlaneInfo *fb_info)
{
	int ret = 0;
	if (fb_info->plane_uid == 0)
	{ // primary_fb
		free_primary_fb();
	}
	else if (fb_info->plane_uid > 0 && fb_info->plane_uid <= dev->count_planes)
	{
		free_plane(&dev->planes[fb_info->plane_uid - 1]);
	}
	else
	{
		fprintf(stderr, "DrmHelperFreeFb: Invalid plane: %d\n", fb_info->plane_uid);
		ret = -EINVAL;
	}
	return ret;
}

void DrmHelperFree(void)
{
	fprintf(stderr, "DrmHelperFree\n");
	if (dev)
	{
		free_primary_fb();
		free_planes();

		if (dev->fd >= 0)
		{
			close(dev->fd);
			dev->fd = -1;
		}
		free(dev);
		dev = NULL;
	}
}
//
DhHwInfo *DrmHelperInit(int drm_id)
{
	if (dev)
	{
		fprintf(stderr, "DrmHelper already in use\n");
		return NULL;
	}

	dev = calloc(1, sizeof(dev_s));
	if (!dev)
	{
		fprintf(stderr, "Error memory alloc for dev\n");
		return NULL;
	}
	dev->fd = -1;

	char dp_name[32];
	snprintf(dp_name, sizeof(dp_name), "/dev/dri/card%d", drm_id);

	dev->fd = drm_open(dp_name);
	if (dev->fd >= 0)
	{
		int ret = drm_find_crt();
		if (ret == 0)
		{
			ret = parse_planes(dev);
			if (ret == 0)
			{
				return &dev->dh_hw_info;
			}
			else
			{
				fprintf(stderr, "Error parse_planes\n");
			}
		}
		else
		{
			fprintf(stderr, "Error drm_find_crt\n");
		}
	}
	else
	{
		fprintf(stderr, "Error drm_open\n");
	}
	DrmHelperFree();
	return NULL;
}

__attribute__((constructor)) void construct()
{
	// printf("construct\n");
	dev = NULL;
}

__attribute__((destructor)) void destruct()
{
	DrmHelperFree();
}
