/* $OpenBSD: e_chacha20poly1305.c,v 1.15 2017/01/29 17:49:23 beck Exp $ */

/*
 * Copyright (c) 2015 Reyk Floter <reyk@openbsd.org>
 * Copyright (c) 2014, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <string.h>

#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)

#include <compat.h>
#include <chacha.h>
#include <poly1305.h>

#define POLY1305_TAG_LEN 16
#define CHACHA20_NONCE_LEN_OLD 8

/*
 * The informational RFC 7539, "ChaCha20 and Poly1305 for IETF Protocols",
 * introduced a modified AEAD construction that is incompatible with the
 * common style that has been already used in TLS.  The IETF version also
 * adds a constant (salt) that is prepended to the nonce.
 */
#define CHACHA20_CONSTANT_LEN 4
#define CHACHA20_IV_LEN 8
#define CHACHA20_NONCE_LEN (CHACHA20_CONSTANT_LEN + CHACHA20_IV_LEN)


static void
poly1305_update_with_length(poly1305_state *poly1305,
    const unsigned char *data, size_t data_len)
{
	size_t j = data_len;
	unsigned char length_bytes[8];
	unsigned i;

	for (i = 0; i < sizeof(length_bytes); i++) {
		length_bytes[i] = j;
		j >>= 8;
	}

	if (data != NULL)
		CRYPTO_poly1305_update(poly1305, data, data_len);
	CRYPTO_poly1305_update(poly1305, length_bytes, sizeof(length_bytes));
}

static void
poly1305_update_with_pad16(poly1305_state *poly1305,
    const unsigned char *data, size_t data_len)
{
	static const unsigned char zero_pad16[16];
	size_t pad_len;

	CRYPTO_poly1305_update(poly1305, data, data_len);

	/* pad16() is defined in RFC 7539 2.8.1. */
	if ((pad_len = data_len % 16) == 0)
		return;

	CRYPTO_poly1305_update(poly1305, zero_pad16, 16 - pad_len);
}

int
aead_chacha20_poly1305_seal(unsigned char *out, size_t *out_len, 
    size_t max_out_len, unsigned char key[32], unsigned char tag_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len)
{
	unsigned char poly1305_key[32];
	poly1305_state poly1305;
	const unsigned char *iv;
	const uint64_t in_len_64 = in_len;
	uint64_t ctr;

	/* The underlying ChaCha implementation may not overflow the block
	 * counter into the second counter word. Therefore we disallow
	 * individual operations that work on more than 2TB at a time.
	 * in_len_64 is needed because, on 32-bit platforms, size_t is only
	 * 32-bits and this produces a warning because it's always false.
	 * Casting to uint64_t inside the conditional is not sufficient to stop
	 * the warning. */
	if (in_len_64 >= (1ULL << 32) * 64 - 64) {
		// EVPerror(EVP_R_TOO_LARGE);
		return 0;
	}

	if (max_out_len < in_len + tag_len) {
		// EVPerror(EVP_R_BUFFER_TOO_SMALL);
		return 0;
	}

	if (nonce_len == CHACHA20_NONCE_LEN_OLD) {
		/* Google's draft-agl-tls-chacha20poly1305-04, Nov 2013 */

		memset(poly1305_key, 0, sizeof(poly1305_key));
		CRYPTO_chacha_20(poly1305_key, poly1305_key,
		    sizeof(poly1305_key), key, nonce, 0);

		CRYPTO_poly1305_init(&poly1305, poly1305_key);
		poly1305_update_with_length(&poly1305, ad, ad_len);
		CRYPTO_chacha_20(out, in, in_len, key, nonce, 1);
		poly1305_update_with_length(&poly1305, out, in_len);
	} else if (nonce_len == CHACHA20_NONCE_LEN) {
		/* RFC 7539, May 2015 */

		ctr = (uint64_t)(nonce[0] | nonce[1] << 8 |
		    nonce[2] << 16 | nonce[3] << 24) << 32;
		iv = nonce + CHACHA20_CONSTANT_LEN;

		memset(poly1305_key, 0, sizeof(poly1305_key));
		CRYPTO_chacha_20(poly1305_key, poly1305_key,
		    sizeof(poly1305_key), key, iv, ctr);

		CRYPTO_poly1305_init(&poly1305, poly1305_key);
		poly1305_update_with_pad16(&poly1305, ad, ad_len);
		CRYPTO_chacha_20(out, in, in_len, key, iv, ctr + 1);
		poly1305_update_with_pad16(&poly1305, out, in_len);
		poly1305_update_with_length(&poly1305, NULL, ad_len);
		poly1305_update_with_length(&poly1305, NULL, in_len);
	}

	if (tag_len != POLY1305_TAG_LEN) {
		unsigned char tag[POLY1305_TAG_LEN];
		CRYPTO_poly1305_finish(&poly1305, tag);
		memcpy(out + in_len, tag, tag_len);
		*out_len = in_len + tag_len;
		return 1;
	}

	CRYPTO_poly1305_finish(&poly1305, out + in_len);
	*out_len = in_len + POLY1305_TAG_LEN;
	return 1;
}

int
aead_chacha20_poly1305_open(unsigned char *out, size_t *out_len, 
    size_t max_out_len, unsigned char key[32], unsigned char tag_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len)
{
	unsigned char mac[POLY1305_TAG_LEN];
	unsigned char poly1305_key[32];
	const unsigned char *iv = nonce;
	poly1305_state poly1305;
	const uint64_t in_len_64 = in_len;
	size_t plaintext_len;
	uint64_t ctr = 0;

	if (in_len < tag_len) {
		// EVPerror(EVP_R_BAD_DECRYPT);
		return 0;
	}

	/* The underlying ChaCha implementation may not overflow the block
	 * counter into the second counter word. Therefore we disallow
	 * individual operations that work on more than 2TB at a time.
	 * in_len_64 is needed because, on 32-bit platforms, size_t is only
	 * 32-bits and this produces a warning because it's always false.
	 * Casting to uint64_t inside the conditional is not sufficient to stop
	 * the warning. */
	if (in_len_64 >= (1ULL << 32) * 64 - 64) {
		// EVPerror(EVP_R_TOO_LARGE);
		return 0;
	}

	plaintext_len = in_len - tag_len;

	if (max_out_len < plaintext_len) {
		// EVPerror(EVP_R_BUFFER_TOO_SMALL);
		return 0;
	}

	if (nonce_len == CHACHA20_NONCE_LEN_OLD) {
		/* Google's draft-agl-tls-chacha20poly1305-04, Nov 2013 */

		memset(poly1305_key, 0, sizeof(poly1305_key));
		CRYPTO_chacha_20(poly1305_key, poly1305_key,
		    sizeof(poly1305_key), key, nonce, 0);

		CRYPTO_poly1305_init(&poly1305, poly1305_key);
		poly1305_update_with_length(&poly1305, ad, ad_len);
		poly1305_update_with_length(&poly1305, in, plaintext_len);
	} else if (nonce_len == CHACHA20_NONCE_LEN) {
		/* RFC 7539, May 2015 */

		ctr = (uint64_t)(nonce[0] | nonce[1] << 8 |
		    nonce[2] << 16 | nonce[3] << 24) << 32;
		iv = nonce + CHACHA20_CONSTANT_LEN;

		memset(poly1305_key, 0, sizeof(poly1305_key));
		CRYPTO_chacha_20(poly1305_key, poly1305_key,
		    sizeof(poly1305_key), key, iv, ctr);

		CRYPTO_poly1305_init(&poly1305, poly1305_key);
		poly1305_update_with_pad16(&poly1305, ad, ad_len);
		poly1305_update_with_pad16(&poly1305, in, plaintext_len);
		poly1305_update_with_length(&poly1305, NULL, ad_len);
		poly1305_update_with_length(&poly1305, NULL, plaintext_len);
	}

	CRYPTO_poly1305_finish(&poly1305, mac);

	if (timingsafe_memcmp(mac, in + plaintext_len, tag_len) != 0) {
		// EVPerror(EVP_R_BAD_DECRYPT);
		return 0;
	}

	CRYPTO_chacha_20(out, in, plaintext_len, key, iv, ctr + 1);
	*out_len = plaintext_len;
	return 1;
}

#endif  /* !OPENSSL_NO_CHACHA && !OPENSSL_NO_POLY1305 */
