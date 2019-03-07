/* Copyright (c) 2012-2018 by Vivox Inc.
*
* Permission to use, copy, modify or distribute this software in binary or source form 
* for any purpose is allowed only under explicit prior consent in writing from Vivox Inc.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND VIVOX DISCLAIMS
* ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL VIVOX
* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

#pragma once

#include "VxcExports.h"
#ifdef __ANDROID__
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

VIVOXSDK_DLLEXPORT int vx_jni_set_java_vm(JavaVM * jvm);
VIVOXSDK_DLLEXPORT JavaVM *vx_jni_get_java_vm(void);
VIVOXSDK_DLLEXPORT jclass  vx_jni_get_class(JNIEnv* env, const char* class_name);

#ifdef __cplusplus
}
#endif
#endif
