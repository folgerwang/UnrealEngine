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

#pragma once

#ifdef __cplusplus
extern "C"{
#endif

/**
 * generates a Vivox Access Token
 * @param issuer standard issuer claim
 * @param expiration standard expiration time claim
 * @param vxa Vivox action, e.g. "join" or "kick" [subject]
 * @param serial number, to guarantee uniqueness within an epoch second
 * @param subject optional URI of the target of the action, needed for third-party call control like "kick"
 * @param from_uri SIP From URI
 * @param to_uri SIP To URI
 * @param secret token-signing key
 * @param secret_len length of secret
 * @returns malloc'd null-terminated buffer
 */
char *vx_generate_token(const char *issuer, time_t expiration, const char *vxa, unsigned long long serial, const char *subject, const char *from_uri, const char *to_uri, const unsigned char *secret, size_t secret_len);

/**
 * encodes sequence of octets into null-terminated URL-safe base64
 * @param buf buffer to encode
 * @param len length of buffer to encode
 * @returns malloc'd null-terminated buffer
 */
char *vx_base64_url_encode(const unsigned char *buf, int len);

#ifdef __cplusplus
}
#endif
