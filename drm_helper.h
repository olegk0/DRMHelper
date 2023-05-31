#ifndef DRM_HELPER_H
#define DRM_HELPER_H

// #include "drm.h"
#include <drm_fourcc.h>
#include <xf86drmMode.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

	typedef enum
	{
		plane_type_overlay,
		plane_type_primary,
		plane_type_cursor,
	} DhPlaneType;

	// read only, do not change
	typedef struct _DhPlaneInfo
	{
		uint8_t plane_uid;
		DhPlaneType type;
		drmModeFB2 fbi;
		uint16_t x, y;
		uint8_t zpos;
		uint8_t fullscreen;
		//		uint8_t depth;
		uint8_t bpp;
		uint64_t fb_size;
		void *map_bufs[3]; // void *fb_buf -> map_bufs[0]
	} DhPlaneInfo;

	// read only, do not change
	typedef struct _DhHwInfo
	{
		uint8_t count_planes[plane_type_cursor + 1];
		uint8_t bpp, depth;
		uint16_t width, height;
		uint16_t pitch;
	} DhHwInfo;

	DhHwInfo *DrmHelperInit(int drm_id);
	void DrmHelperFree(void);

	// for primary: "fullscreen" not used
	// if "width" and or "height" = 0 -> set default
	// return fb_id or -Error
	DhPlaneInfo *DrmHelperAllocFb(DhPlaneType type, uint32_t fourcc_format, uint16_t width, uint16_t height,
								  uint16_t x, uint16_t y, uint8_t fullscreen);
	int DrmHelperFreeFb(DhPlaneInfo *fb_info);

	int DrmHelperSetPlanePos(DhPlaneInfo *fb_info, int x, int y);
	int DrmHelperSetZpos(DhPlaneInfo *fb_info, uint8_t zpos);

// For test build with old libdrm
#ifndef drmModeMapDumbBuffer
	extern int drmModeMapDumbBuffer(int fd, uint32_t handle, uint64_t *offset);
	extern int drmModeCreateDumbBuffer(int fd, uint32_t width, uint32_t height, uint32_t bpp, uint32_t flags, uint32_t *handle,
									   uint32_t *pitch, uint64_t *size);
	extern int drmModeDestroyDumbBuffer(int fd, uint32_t handle);
#endif

#if defined(__cplusplus)
}
#endif

#endif