#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#include <opus.h>

#include "core.h"
#include "util.h"

ECPContext ctx_c;
ECPSocket sock_c;
ECPConnHandler handler_c;

ECPNode node;
ECPConnection conn;
int open_done = 0;

snd_pcm_t *handle_plb;
snd_pcm_t *handle_cpt;
OpusEncoder *opus_enc;
OpusDecoder *opus_dec;
// 2.5, 5, 10, 20, 40 or 60 ms
snd_pcm_uframes_t alsa_frames = 160;
unsigned char *alsa_out_buf = NULL;
unsigned char *alsa_in_buf = NULL;

#define CTYPE_TEST  0
#define MTYPE_MSG   8

int a_open(char *dev_name, snd_pcm_t **handle, snd_pcm_hw_params_t **hw_params, snd_pcm_stream_t stream, snd_pcm_format_t format, unsigned int *nchannels, unsigned int *sample_rate, snd_pcm_uframes_t *frames, size_t *buf_size) {
	int bits, err = 0;
	unsigned int fragments = 2;
	unsigned int frame_size;

	err = snd_pcm_open(handle, dev_name, stream, 0);
	if (!err) err = snd_pcm_hw_params_malloc(hw_params);
	if (!err) err = snd_pcm_hw_params_any(*handle, *hw_params);
	if (!err) err = snd_pcm_hw_params_set_access(*handle, *hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (!err) err = snd_pcm_hw_params_set_format(*handle, *hw_params, format);
	if (!err) err = snd_pcm_hw_params_set_rate_near(*handle, *hw_params, sample_rate, 0);
	if (!err) err = snd_pcm_hw_params_set_channels_near(*handle, *hw_params, nchannels);
	if (!err) err = snd_pcm_hw_params_set_periods_near(*handle, *hw_params, &fragments, 0);
	if (!err) err = snd_pcm_hw_params_set_period_size_near(*handle, *hw_params, frames, 0);
	if (!err) err = snd_pcm_hw_params(*handle, *hw_params);
	if (err) return err;
	
	bits = snd_pcm_hw_params_get_sbits(*hw_params);
	if (bits < 0) return bits;
	
	frame_size = *nchannels * (bits / 8);
	*buf_size = frame_size * *frames;

	return ECP_OK;
}

int a_prepare(snd_pcm_t *handle, snd_pcm_hw_params_t *hw_params, unsigned char *buf, snd_pcm_uframes_t frames) {
	snd_pcm_drop(handle);
	snd_pcm_prepare(handle);

	if (snd_pcm_stream(handle) == SND_PCM_STREAM_PLAYBACK) {
		int i, err;
		unsigned int fragments;

		err = snd_pcm_hw_params_get_periods(hw_params, &fragments, NULL);
		if (err) return err;

		for (i=0; i<fragments; i++) snd_pcm_writei(handle, buf, frames);
	}
	return ECP_OK;
}

opus_int32 a_read(snd_pcm_t *handle, unsigned char *buf, snd_pcm_uframes_t frames, OpusEncoder *enc, unsigned char *opus_buf, opus_int32 opus_size) {
	snd_pcm_sframes_t frames_in;

	while ((frames_in = snd_pcm_readi(handle, buf, frames)) < 0) {
		if (frames_in == -EAGAIN)
			continue;
		snd_pcm_prepare(handle);
	}
	return opus_encode(enc, (opus_int16 *)buf, frames_in, opus_buf, opus_size);
}

snd_pcm_sframes_t a_write(OpusDecoder *dec, unsigned char *opus_buf, opus_int32 opus_size, snd_pcm_t *handle, unsigned char *buf, snd_pcm_uframes_t frames) {
	snd_pcm_sframes_t frames_in, frames_out;

	frames_in = opus_decode(dec, opus_buf, opus_size, (opus_int16 *)buf, frames, 0);	
	while ((frames_out = snd_pcm_writei(handle, buf, frames_in)) < 0) {
		if (frames_out == -EAGAIN)
			continue;
		snd_pcm_prepare(handle);
	}
	return frames_out;
}

int a_init(void) {
	char *dev_name  = "hw:1,0";
	unsigned int nchannels = 1;
	unsigned int sample_rate = 16000;

	snd_pcm_hw_params_t *hw_params_plb;
	snd_pcm_hw_params_t *hw_params_cpt;
	size_t buf_size;

	a_open(dev_name, &handle_plb, &hw_params_plb, SND_PCM_STREAM_PLAYBACK, SND_PCM_FORMAT_S16_LE, &nchannels, &sample_rate, &alsa_frames, &buf_size);
	alsa_out_buf = malloc(buf_size);
	memset(alsa_out_buf, 0, buf_size);
	a_prepare(handle_plb, hw_params_plb, alsa_out_buf, alsa_frames);

	a_open(dev_name, &handle_cpt, &hw_params_cpt, SND_PCM_STREAM_CAPTURE, SND_PCM_FORMAT_S16_LE, &nchannels, &sample_rate, &alsa_frames, &buf_size);
	alsa_in_buf = malloc(buf_size);
	memset(alsa_in_buf, 0, buf_size);
	a_prepare(handle_cpt, hw_params_cpt, alsa_in_buf, alsa_frames);

	int size;
	size = opus_encoder_get_size(nchannels);
	opus_enc = malloc(size);
	opus_encoder_init(opus_enc, sample_rate, nchannels, OPUS_APPLICATION_VOIP);

	size = opus_decoder_get_size(nchannels);
	opus_dec = malloc(size);
	opus_decoder_init(opus_dec, sample_rate, nchannels);
    
    return ECP_OK;
}

ssize_t handle_open_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
    uint32_t seq = 0;
    
    ecp_conn_handle_open(conn, sq, t, p, s, b);
    if (s < 0) {
        printf("OPEN ERR:%ld\n", s);
        return s;
    }
    
	a_init();
	open_done = 1;
    return s;
}

ssize_t handle_msg_c(ECPConnection *conn, ecp_seq_t sq, unsigned char t, unsigned char *p, ssize_t s, ECP2Buffer *b) {
	a_write(opus_dec, p, s, handle_plb, alsa_out_buf, alsa_frames);
    return s;
}


static void usage(char *arg) {
    fprintf(stderr, "Usage: %s <node.pub>\n", arg);
    exit(1);
}

int main(int argc, char *argv[]) {
    int rv;
    
    if (argc != 2) usage(argv[0]);
    
    rv = ecp_init(&ctx_c);
    printf("ecp_init RV:%d\n", rv);

    rv = ecp_conn_handler_init(&handler_c);
    handler_c.msg[ECP_MTYPE_OPEN] = handle_open_c;
    handler_c.msg[MTYPE_MSG] = handle_msg_c;
    ctx_c.handler[CTYPE_TEST] = &handler_c;
    
    rv = ecp_sock_create(&sock_c, &ctx_c, NULL);
    printf("ecp_sock_create RV:%d\n", rv);

    rv = ecp_sock_open(&sock_c, NULL);
    printf("ecp_sock_open RV:%d\n", rv);
    
    rv = ecp_start_receiver(&sock_c);
    printf("ecp_start_receiver RV:%d\n", rv);

    rv = ecp_util_node_load(&ctx_c, &node, argv[1]);
    printf("ecp_util_node_load RV:%d\n", rv);

    rv = ecp_conn_create(&conn, &sock_c, CTYPE_TEST);
    printf("ecp_conn_create RV:%d\n", rv);

    rv = ecp_conn_open(&conn, &node);
    printf("ecp_conn_open RV:%d\n", rv);

    while(!open_done) sleep(1);

	unsigned char payload[ECP_MAX_PLD];
	unsigned char *opus_buf;

	while(1) {
		ecp_pld_set_type(payload, MTYPE_MSG);
		opus_buf = ecp_pld_get_buf(payload, 0);
		opus_int32 len = a_read(handle_cpt, alsa_in_buf, alsa_frames, opus_enc, opus_buf, ECP_MAX_MSG);
		if (len < 0) continue;
	    ssize_t _rv = ecp_pld_send(&conn, payload, len);
	}
}