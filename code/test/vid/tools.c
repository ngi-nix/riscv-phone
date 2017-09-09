#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"

static const VpxInterface vpx_encoders[] = {
  { "vp8", VP8_FOURCC, &vpx_codec_vp8_cx },
  { "vp9", VP9_FOURCC, &vpx_codec_vp9_cx },
};

int get_vpx_encoder_count(void) {
  return sizeof(vpx_encoders) / sizeof(vpx_encoders[0]);
}

const VpxInterface *get_vpx_encoder_by_index(int i) { return &vpx_encoders[i]; }

const VpxInterface *get_vpx_encoder_by_name(const char *name) {
  int i;

  for (i = 0; i < get_vpx_encoder_count(); ++i) {
    const VpxInterface *encoder = get_vpx_encoder_by_index(i);
    if (strcmp(encoder->name, name) == 0) return encoder;
  }

  return NULL;
}

static const VpxInterface vpx_decoders[] = {
  { "vp8", VP8_FOURCC, &vpx_codec_vp8_dx },
  { "vp9", VP9_FOURCC, &vpx_codec_vp9_dx },
};

int get_vpx_decoder_count(void) {
  return sizeof(vpx_decoders) / sizeof(vpx_decoders[0]);
}

const VpxInterface *get_vpx_decoder_by_index(int i) { return &vpx_decoders[i]; }

const VpxInterface *get_vpx_decoder_by_name(const char *name) {
  int i;

  for (i = 0; i < get_vpx_decoder_count(); ++i) {
    const VpxInterface *const decoder = get_vpx_decoder_by_index(i);
    if (strcmp(decoder->name, name) == 0) return decoder;
  }

  return NULL;
}

void die_codec(vpx_codec_ctx_t *ctx, const char *s) {
  if (ctx) {
    const char *detail = vpx_codec_error_detail(ctx);

    fprintf(stderr, "%s: %s\n", s, vpx_codec_error(ctx));
    if (detail) fprintf(stderr, "    %s\n", detail);
  } else {
    fprintf(stderr, "%s", s);
  }
  exit(EXIT_FAILURE);
}

// TODO(dkovalev): move this function to vpx_image.{c, h}, so it will be part
// of vpx_image_t support
int vpx_img_plane_width(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->x_chroma_shift > 0)
    return (img->d_w + 1) >> img->x_chroma_shift;
  else
    return img->d_w;
}

int vpx_img_plane_height(const vpx_image_t *img, int plane) {
  if (plane > 0 && img->y_chroma_shift > 0)
    return (img->d_h + 1) >> img->y_chroma_shift;
  else
    return img->d_h;
}

int vpx_img_read(vpx_image_t *img, void *img_buf, int sz) {
  int plane;
  int off = 0;

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      if (off + w > sz) return 0;
      memcpy(buf, img_buf + off, w);
      off += w;
      buf += stride;
    }
  }

  return 1;
}

int vpx_img_write(const vpx_image_t *img, void *img_buf, int sz) {
  int plane;
  int off = 0;

  for (plane = 0; plane < 3; ++plane) {
    const unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      if (off + w > sz) return 0;
      memcpy(img_buf + off, buf, w);
      off += w;
      buf += stride;
    }
  }
  
  return 1;
}

int vpx_img_read_f(vpx_image_t *img, FILE *file) {
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      if (fread(buf, 1, w, file) != (size_t)w) return 0;
      buf += stride;
    }
  }

  return 1;
}

int vpx_img_write_f(const vpx_image_t *img, FILE *file) {
  int plane;

  for (plane = 0; plane < 3; ++plane) {
    const unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      if (fwrite(buf, 1, w, file) != (size_t)w) return 0;
      buf += stride;
    }
  }
}

