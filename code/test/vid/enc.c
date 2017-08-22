/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Simple Encoder
// ==============
//
// This is an example of a simple encoder loop. It takes an input file in
// YV12 format, passes it through the encoder, and writes the compressed
// frames to disk in IVF format. Other decoder examples build upon this
// one.
//
// The details of the IVF format have been elided from this example for
// simplicity of presentation, as IVF files will not generally be used by
// your application. In general, an IVF file consists of a file header,
// followed by a variable number of frames. Each frame consists of a frame
// header followed by a variable length payload. The length of the payload
// is specified in the first four bytes of the frame header. The payload is
// the raw compressed data.
//
// Standard Includes
// -----------------
// For encoders, you only have to include `vpx_encoder.h` and then any
// header files for the specific codecs you use. In this case, we're using
// vp8.
//
// Getting The Default Configuration
// ---------------------------------
// Encoders have the notion of "usage profiles." For example, an encoder
// may want to publish default configurations for both a video
// conferencing application and a best quality offline encoder. These
// obviously have very different default settings. Consult the
// documentation for your codec to see if it provides any default
// configurations. All codecs provide a default configuration, number 0,
// which is valid for material in the vacinity of QCIF/QVGA.
//
// Updating The Configuration
// ---------------------------------
// Almost all applications will want to update the default configuration
// with settings specific to their usage. Here we set the width and height
// of the video file to that specified on the command line. We also scale
// the default bitrate based on the ratio between the default resolution
// and the resolution specified on the command line.
//
// Initializing The Codec
// ----------------------
// The encoder is initialized by the following code.
//
// Encoding A Frame
// ----------------
// The frame is read as a continuous block (size width * height * 3 / 2)
// from the input file. If a frame was read (the input file has not hit
// EOF) then the frame is passed to the encoder. Otherwise, a NULL
// is passed, indicating the End-Of-Stream condition to the encoder. The
// `frame_cnt` is reused as the presentation time stamp (PTS) and each
// frame is shown for one frame-time in duration. The flags parameter is
// unused in this example. The deadline is set to VPX_DL_REALTIME to
// make the example run as quickly as possible.

// Forced Keyframes
// ----------------
// Keyframes can be forced by setting the VPX_EFLAG_FORCE_KF bit of the
// flags passed to `vpx_codec_control()`. In this example, we force a
// keyframe every <keyframe-interval> frames. Note, the output stream can
// contain additional keyframes beyond those that have been forced using the
// VPX_EFLAG_FORCE_KF flag because of automatic keyframe placement by the
// encoder.
//
// Processing The Encoded Data
// ---------------------------
// Each packet of type `VPX_CODEC_CX_FRAME_PKT` contains the encoded data
// for this frame. We write a IVF frame header, followed by the raw data.
//
// Cleanup
// -------
// The `vpx_codec_destroy` call frees any memory allocated by the codec.
//
// Error Handling
// --------------
// This example does not special case any error return codes. If there was
// an error, a descriptive message is printed and the program exits. With
// few exeptions, vpx_codec functions return an enumerated error status,
// with the value `0` indicating success.
//
// Error Resiliency Features
// -------------------------
// Error resiliency is controlled by the g_error_resilient member of the
// configuration structure. Use the `decode_with_drops` example to decode with
// frames 5-10 dropped. Compare the output for a file encoded with this example
// versus one encoded with the `simple_encoder` example.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enc.h"

static const char *exec_name = "./enc";

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s <codec> <width> <height> <infile> <outfile> "
          "<keyframe-interval> <error-resilient> <frames to encode>\n"
          "See comments in simple_encoder.c for more information.\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static VpxVideoWriter *writer;

static VpxVideoWriter *open_writer(const char *file_name, uint32_t fourcc, int width, int height, int fps) {
  VpxVideoInfo info = { 0, 0, 0, { 0, 0 } };
  
  info.codec_fourcc = fourcc;
  info.frame_width = width;
  info.frame_height = height;
  info.time_base.numerator = 1;
  info.time_base.denominator = fps;

  return vpx_video_writer_open(file_name, kContainerIVF, &info);
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

  for (plane = 0; plane < 3; ++plane) {
    unsigned char *buf = img->planes[plane];
    const int stride = img->stride[plane];
    const int w = vpx_img_plane_width(img, plane) *
                  ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = vpx_img_plane_height(img, plane);
    int y;

    for (y = 0; y < h; ++y) {
      memcpy(buf, img_buf, w);
      img_buf += w;
      buf += stride;
    }
  }

  return 1;
}

int vpx_encode_frame(vpx_codec_ctx_t *codec, vpx_image_t *img, int frame_index, int flags) {
  int got_pkts = 0;
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t *pkt = NULL;
  const vpx_codec_err_t res =
      vpx_codec_encode(codec, img, frame_index, 1, flags, VPX_DL_REALTIME);
  if (res != VPX_CODEC_OK) die_codec(codec, "Failed to encode frame");

  while ((pkt = vpx_codec_get_cx_data(codec, &iter)) != NULL) {
    got_pkts = 1;

    if (pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
      const int keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
      if (!vpx_video_writer_write_frame(writer, pkt->data.frame.buf,
                                        pkt->data.frame.sz,
                                        pkt->data.frame.pts)) {
        die_codec(codec, "Failed to write compressed frame");
      }
      printf(keyframe ? "K" : ".");
      fflush(stdout);
    }
  }

  return got_pkts;
}


void vpx_open(vpx_codec_ctx_t *codec, vpx_codec_iface_t *codec_interface, int width, int height, int fps, int bitrate, vpx_codec_er_flags_t err_resilient, vpx_image_t *raw) {
  vpx_codec_enc_cfg_t cfg;
  vpx_codec_err_t res;

  res = vpx_codec_enc_config_default(codec_interface, &cfg, 0);
  if (res) die("Failed to get default codec config.");

  cfg.g_w = width;
  cfg.g_h = height;
  cfg.g_timebase.num = 1;
  cfg.g_timebase.den = fps;
  cfg.rc_target_bitrate = bitrate;
  cfg.g_error_resilient = err_resilient;
  cfg.rc_end_usage = VPX_CBR;

  if (vpx_codec_enc_init(codec, codec_interface, &cfg, 0))
    die_codec(codec, "Failed to initialize encoder");

  if (vpx_codec_control(codec, VP8E_SET_CPUUSED, 8))
    die_codec(codec, "Failed to initialize cpuused");

  if (!vpx_img_alloc(raw, VPX_IMG_FMT_I420, width, height, 1)) {
    die("Failed to allocate image.");
  }
}

void vpx_close(vpx_codec_ctx_t *codec, vpx_image_t *raw) {
  // Flush encoder.
  while (vpx_encode_frame(codec, NULL, -1, 0)) {
  }

  vpx_img_free(raw);
  if (vpx_codec_destroy(codec)) die_codec(codec, "Failed to destroy codec.");

  vpx_video_writer_close(writer);
}

void vpx_init(const char *codec_arg, const char *outfile_arg, int width, int height, int fps) {
  const VpxInterface *encoder = get_vpx_encoder_by_name(codec_arg);
  if (!encoder) die("Unsupported codec.");
  
  writer = open_writer(outfile_arg, encoder->fourcc, width, height, fps);
  if (!writer) die("Failed to open %s for writing.", outfile_arg);

  printf("Using %s\n", vpx_codec_iface_name(encoder->codec_interface()));
}

