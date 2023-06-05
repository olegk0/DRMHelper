#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "drm_helper.h"

void print_params(DhPlaneInfo* pi) {
  printf("\n");
  printf("plane_uid: %d\n", pi->plane_uid);
  printf("bpp: %d\n", pi->bpp);
  printf("fb_size: %lu\n", pi->fb_size);
  printf("height: %d\n", DrmHelperWorkBuf(pi, 0).fbi.height);
  printf("width: %d\n", DrmHelperWorkBuf(pi, 0).fbi.width);
  printf("pitches: %d\n", DrmHelperWorkBuf(pi, 0).fbi.pitches[0]);
  printf("plane0: %p\n", DrmHelperWorkBuf(pi, 0).map_planes[0]);
  printf("pixel_format: %d\n", DrmHelperWorkBuf(pi, 0).fbi.pixel_format);
  printf("exposed_buf: %d\n", pi->exposed_buf);
  printf("opened_buf: %d\n", pi->opened_buf);
  printf("\n");
}

static int r_skip = 1;
void render_buf(DhPlaneInfo* pi, int lines, int buf_id) {
#if 0
  unsigned int clr_len = DrmHelperWorkBuf(pi,pi->opened_buf).fbi.height / lines;
  unsigned int clr = 0;
  uint32_t* fb_map = DrmHelperWorkBuf(pi,pi->opened_buf).map_planes[0];
  for (int i = 0; i < DrmHelperWorkBuf(pi,pi->opened_buf).fbi.height; i++) {
    int t = i / clr_len;
    clr = 0xF << t;
    for (int j = 0; j < DrmHelperWorkBuf(pi,pi->opened_buf).fbi.width; j++) {
      fb_map[i * DrmHelperWorkBuf(pi,pi->opened_buf).fbi.width + j] = clr;
    }
  }
  DrmHelperFlipConfirm(pi,pi->opened_buf);
#else
  if (buf_id < 0) {
    buf_id = pi->opened_buf;
  }
  unsigned int clr_len = DrmHelperWorkBuf(pi, buf_id).fbi.height / lines;
  unsigned int clr = 0;
  uint32_t* fb_map = DrmHelperWorkBuf(pi, buf_id).map_planes[0];
  for (int i = 0; i < DrmHelperWorkBuf(pi, buf_id).fbi.height; i++) {
    int t = i / clr_len;
    clr = 0xF << t;
    for (int j = 0; j < DrmHelperWorkBuf(pi, buf_id).fbi.width; j++) {
      fb_map[i * DrmHelperWorkBuf(pi, buf_id).fbi.width + j] = clr;
    }
  }
  DrmHelperWorkBufConfirm(pi, buf_id);

  if (pi->count_bufs > 1) {
    // fprintf(stderr, "buf_id:%d\n", buf_id);
    if (r_skip == 1) {
      r_skip = 0;
    } else {
      r_skip = 1;
      int ret;
      do {
        ret = DrmHelperFlip(pi);
      } while (ret == 0);
      // fprintf(stderr, "DrmHelperFlip ret:%d\n", ret);
    }
  }

#endif
}

int main() {
  /* init */
  DhHwInfo* dhHwInfo = DrmHelperInit(0);

  if (!dhHwInfo) {
    printf("Error DrmHelperInit\n");
    return -1;
  }

  printf("\n");
  printf("height: %d\n", dhHwInfo->height);
  printf("width: %d\n", dhHwInfo->width);
  printf("depth: %d\n", dhHwInfo->depth);
  printf("bpp: %d\n", dhHwInfo->bpp);
  printf("pitch: %d\n", dhHwInfo->pitch);
  printf("primarys: %d\n", dhHwInfo->count_planes[dh_plane_type_primary]);
  printf("overlays: %d\n", dhHwInfo->count_planes[dh_plane_type_overlay]);
  printf("cursors: %d\n", dhHwInfo->count_planes[dh_plane_type_cursor]);
  printf("\n");

  DhPlaneInfo* prim_fb = NULL;
  // prim_fb = DrmHelperAllocFb(dh_plane_type_primary, DRM_FORMAT_XRGB8888, 0,
  // 0,
  //                          0, 0, 2, 0);
  if (!prim_fb) {
    printf("Error DrmHelperAllocFb1\n");
  } else {
    print_params(prim_fb);

    // prim_fb->opened_buf =0;
    render_buf(prim_fb, 24, -1);

    //	sleep(5);
    //	return 0;
  }

  {
    DhPlaneInfo* ovl1_fb = DrmHelperAllocFb(
        dh_plane_type_overlay, DRM_FORMAT_XRGB8888, 300, 200, 0, 0, 3, 0);
    if (!ovl1_fb) {
      printf("Error DrmHelperAllocFb2\n");
    } else {
      DrmHelperSetZpos(ovl1_fb, 1);

      print_params(ovl1_fb);

      // int DrmHelperSetZpos(int fb_id, uint8_t zpos);
      render_buf(ovl1_fb, 24, DrmHelperWorkGetBufId(ovl1_fb));
    }

    // DrmHelperSetPlanePos(o1, i, i);
    DhPlaneInfo* ovl2_fb = DrmHelperAllocFb(
        dh_plane_type_overlay, DRM_FORMAT_XRGB8888, 50, 50, 200, 200, 1, 0);
    if (!ovl2_fb) {
      printf("Error DrmHelperAllocFb3\n");
    } else {
      DrmHelperSetZpos(ovl2_fb, 2);

      print_params(ovl2_fb);

      render_buf(ovl2_fb, 24, -1);

      int x = 0;
      for (int i = 0; i < 400; i++) {
        if (prim_fb)
          render_buf(prim_fb, i % 50, -1);

        if (ovl1_fb) {
          int buf_id = DrmHelperWorkGetBufId(ovl1_fb);
          if (buf_id < 0) {
            // wait
          } else {
            render_buf(ovl1_fb, i % 50, buf_id);
          }
        }

        DrmHelperSetPlanePos(ovl2_fb, x, i);
        // usleep(10);
        x += 2;
      }
    }
  }

  /* destroy */
  DrmHelperFree();

  return 0;
}
