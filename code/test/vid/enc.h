#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "tools.h"
#include "video_writer.h"

int vpx_img_read(vpx_image_t *img, void *img_buf, int sz);
int vpx_encode_frame(vpx_codec_ctx_t *codec, vpx_image_t *img, int frame_index, int flags);
void vpx_open(vpx_codec_ctx_t *codec, vpx_codec_iface_t *codec_interface, int width, int height, int fps, int bitrate, vpx_codec_er_flags_t err_resilient, vpx_image_t *raw);
void vpx_close(vpx_codec_ctx_t *codec, vpx_image_t *raw);
void vpx_init(const char *codec_arg, const char *outfile_arg, int width, int height, int fps);
