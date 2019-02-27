/*********************************************************************************
 * Copyright (c) 2015-2018, Mercer Road Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <string.h>
#include "tokengen.h"

static unsigned char *hsha256_hash(const char *data, const unsigned char *secret, unsigned int secret_len);
static char *base64_encode(const unsigned char *buf, int len);
static void replacestr(char *line, const char *search, const char *replace);
static void vx_snprintf(char *dest, unsigned int size, const char *fmt, ...);

char *vx_generate_token(const char *issuer, time_t expiration, const char *vxa, unsigned long long serial, const char *subject, const char *from_uri, const char *to_uri, const unsigned char *secret, size_t secret_len) {
    /// The header is always empty.
    const char *header = "{}";

    /// Create payload from arguments.
    char payload[512];
    if (NULL == subject && NULL == to_uri) {
        vx_snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"exp\":%lu,\"vxa\":\"%s\",\"vxi\":%llu,\"f\":\"%s\"}", issuer, (unsigned long)expiration, vxa, serial, from_uri);
    } else if (NULL == subject) {
        vx_snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"exp\":%lu,\"vxa\":\"%s\",\"vxi\":%llu,\"f\":\"%s\",\"t\":\"%s\"}", issuer, (unsigned long)expiration, vxa, serial, from_uri, to_uri);
    } else {
        vx_snprintf(payload, sizeof(payload), "{\"iss\":\"%s\",\"exp\":%lu,\"vxa\":\"%s\",\"vxi\":%llu,\"sub\":\"%s\",\"f\":\"%s\",\"t\":\"%s\"}", issuer, (unsigned long)expiration, vxa, serial, subject, from_uri, to_uri);
    }

    /// Base64 URL-safe encode the header and payload.
    char *b64_header = vx_base64_url_encode(header, (int)strlen(header));
    char *b64_payload = vx_base64_url_encode(payload, (int)strlen(payload));

    char sign_me[1024];
    memset(sign_me, 0, sizeof(sign_me));
    vx_snprintf(sign_me, sizeof(sign_me), "%s.%s", b64_header, b64_payload);

    /// Use HMAC SHA-256 to hash header.payload with the secret as the key.
    unsigned char *signature = hsha256_hash(sign_me, secret, (int)secret_len);

    /// Base64 URL-safe encode the signature.
    char *b64_signature = vx_base64_url_encode(signature, SHA256_DIGEST_LENGTH);
    free(signature);

    /// Construct token and free provisional resources.
    /// It's the responsibility of the caller to free this.
    char *token = (char *)malloc(1024);
    vx_snprintf(token, 1024, "%s.%s.%s", b64_header, b64_payload, b64_signature);
    free(b64_header);
    free(b64_payload);
    free(b64_signature);

    return token;
}

char *vx_base64_url_encode(const unsigned char *buf, int len) {
    char *sp = base64_encode(buf, len);
    /// Substitute URL-safe characters.
    replacestr(sp, "+", "-");
    replacestr(sp, "/", "_");
    /// Remove padding at the end.
    replacestr(sp, "=", "");
    return sp;
}

/*
 * Uses OpenSSL to HMAC encode data with a secret using SHA-256 encryption
 */
static unsigned char *hsha256_hash(const char *data, const unsigned char *secret, unsigned int secret_len) {
    const EVP_MD *md = EVP_sha256();
    unsigned char *md_value = (unsigned char *)malloc(EVP_MD_size(md));
    unsigned int md_len;
    md_len = EVP_MD_size(md);

    HMAC(md, secret, secret_len, data, strlen(data), md_value, &md_len);

   // EVP_cleanup();

    return md_value;
}

/*
 * Uses OpenSSL to Base64 encode the sequence of octets (not URL-safe)
 */
static char *base64_encode(const unsigned char *buf, int len) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    char *b64_buf;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, buf, len);
    (void) BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    b64_buf = (char *)malloc(bptr->length + 1);
    memcpy(b64_buf, bptr->data, bptr->length);
    b64_buf[bptr->length] = 0;

    BIO_free_all(b64);

    return b64_buf;
}

/*
 * Passes through line once, replacing instances of search with replace.
 * This function makes a lot of assumptions about the inputs and was made
 * solely for use in the vx_base64_url_encode function.
 */
static void replacestr(char *line, const char *search, const char *replace) {
    char *sp;

    if (NULL == (sp = strstr(line, search))) {
        return;
    }
    int search_len = (int)strlen(search);
    int replace_len = (int)strlen(replace);
    int tail_len = (int)strlen(sp + search_len);

    memmove(sp + replace_len, sp + search_len, tail_len + 1);
    memcpy(sp, replace, replace_len);

    replacestr(sp + replace_len, search, replace);
}

/*
 * snprintf that guarantees null termination at byte size-1
 */
static void vx_snprintf(char *dest, unsigned int size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dest, size, fmt, ap);
    va_end(ap);
    dest[size - 1] = '\0';
}
