#ifndef TOOLS_COMMON_H_
#define TOOLS_COMMON_H_

#include "vpx/vpx_codec.h"
#include "vpx/vpx_image.h"
#include "vpx/vp8cx.h"

#define VP8_FOURCC 0x30385056
#define VP9_FOURCC 0x30395056

struct VpxRational {
  int numerator;
  int denominator;
};

typedef struct VpxInterface {
  const char *const name;
  const uint32_t fourcc;
  vpx_codec_iface_t *(*const codec_interface)();
} VpxInterface;

void usage_exit(void);
const VpxInterface *get_vpx_encoder_by_index(int i);
const VpxInterface *get_vpx_encoder_by_name(const char *name);

void die(const char *fmt, ...);
void die_codec(vpx_codec_ctx_t *ctx, const char *s);

#endif