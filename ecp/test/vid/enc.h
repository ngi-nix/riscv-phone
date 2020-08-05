#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "tools.h"

int vpx_encode_frame(vpx_codec_ctx_t *codec, vpx_image_t *img, int frame_index, int flags);
void vpx_open(const char *codec_arg, int width, int height, int fps, int bitrate, vpx_codec_er_flags_t err_resilient, vpx_codec_ctx_t *codec, vpx_image_t *raw);
void vpx_close(vpx_codec_ctx_t *codec, vpx_image_t *raw);
