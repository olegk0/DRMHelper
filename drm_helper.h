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
	} dh_fbtype_t;

	typedef struct _dh_fb_info_s
	{
		drmModeFB2 fbi;
		//		uint8_t depth;
		uint8_t bpp;
		uint64_t fb_size;
		void *map_bufs[3]; // void *fb_buf -> map_bufs[0]
	} dh_fb_info_s;

	int DrmHelperInit(int drm_id);
	void DrmHelperFree(void);

	// return fb_id or -Error
	int DrmHelperAllocFb(dh_fbtype_t type, uint32_t fourcc_format, int width, int height, int x, int y,
						 uint8_t fullscreen, dh_fb_info_s *fb_info);

	int DrmHelperFreeFb(int fb_id);

	int DrmHelperSetPlanePos(int fb_id, int x, int y);

	int DrmHelperSetZpos(int fb_id, uint8_t zpos);

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