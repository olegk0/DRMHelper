#ifndef DRM_HELPER_H
#define DRM_HELPER_H

#include <drm_fourcc.h>
#include <errno.h>
#include <stdint.h>
#include <xf86drmMode.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define DEBUG

typedef enum {
  dh_plane_type_overlay,
  dh_plane_type_primary,
  dh_plane_type_cursor,
} DhPlaneType;

typedef struct _DhBufInfo {
  drmModeFB2 fbi;
  void* map_planes[3];
  uint8_t buf_ready;
} DhBufInfo;
// read only, do not change
typedef struct _DhPlaneInfo {
  uint8_t plane_uid;
  DhPlaneType type;
  uint16_t x, y;
  uint8_t zpos;
  uint8_t fullscreen;
  // uint8_t depth;
  uint8_t bpp;
  uint64_t fb_size;
  uint8_t count_bufs;
  uint8_t exposed_buf;
  uint8_t opened_buf;
  uint8_t back_buf;
  DhBufInfo buf_info[3];
} DhPlaneInfo;

// read only, do not change
typedef struct _DhHwInfo {
  uint8_t count_planes[dh_plane_type_cursor + 1];
  uint8_t bpp, depth;
  uint16_t width, height;
  uint16_t pitch;
} DhHwInfo;

// param DhPlaneInfo
#define DrmHelperWorkBuf(dh_plane_info_ptr, buf_id) \
  (dh_plane_info_ptr->buf_info[buf_id])
#define DrmHelperWorkBufConfirm(dh_plane_info_ptr, buf_id) \
  dh_plane_info_ptr->buf_info[buf_id].buf_ready = 1

DhHwInfo* DrmHelperInit(int drm_id);
void DrmHelperFree(void);

// for primary: "fullscreen" not used
// if "width" and or "height" = 0 -> set default
// return fb_id or NULL
DhPlaneInfo* DrmHelperAllocFb(DhPlaneType type,
                              uint32_t fourcc_format,
                              uint16_t width,
                              uint16_t height,
                              uint16_t x,
                              uint16_t y,
                              uint8_t count_bufs,
                              uint8_t fullscreen);
int DrmHelperFreeFb(DhPlaneInfo* plane_info);

int DrmHelperWorkGetBufId(DhPlaneInfo* plane_info);

int DrmHelperFlipConfirm(DhPlaneInfo* plane_info);

// for buffers flip from thread with lock
int DrmHelperFlip(DhPlaneInfo* plane_info);

int DrmHelperSetPlanePos(DhPlaneInfo* plane_info, uint16_t x, uint16_t y);
int DrmHelperSetZpos(DhPlaneInfo* plane_info, uint8_t zpos);

// For test build with old libdrm
// #ifndef drmModeMapDumbBuffer
extern int drmModeMapDumbBuffer(int fd, uint32_t handle, uint64_t* offset);
extern int drmModeCreateDumbBuffer(int fd,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t bpp,
                                   uint32_t flags,
                                   uint32_t* handle,
                                   uint32_t* pitch,
                                   uint64_t* size);
extern int drmModeDestroyDumbBuffer(int fd, uint32_t handle);
// #endif

#if defined(__cplusplus)
}
#endif

#endif