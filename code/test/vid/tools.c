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

#define LOG_ERROR(label)               \
  do {                                 \
    const char *l = label;             \
    va_list ap;                        \
    va_start(ap, fmt);                 \
    if (l) fprintf(stderr, "%s: ", l); \
    vfprintf(stderr, fmt, ap);         \
    fprintf(stderr, "\n");             \
    va_end(ap);                        \
  } while (0)

void die(const char *fmt, ...) {
  LOG_ERROR(NULL);
  usage_exit();
}

void die_codec(vpx_codec_ctx_t *ctx, const char *s) {
  const char *detail = vpx_codec_error_detail(ctx);

  printf("%s: %s\n", s, vpx_codec_error(ctx));
  if (detail) printf("    %s\n", detail);
  exit(EXIT_FAILURE);
}

    