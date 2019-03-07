#pragma once
/* Copyright (c) 2013-2018 by Mercer Road Corp
 *
 * Permission to use, copy, modify or distribute this software in binary or source form 
 * for any purpose is allowed only under explicit prior consent in writing from Mercer Road Corp
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND MERCER ROAD CORP DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL MERCER ROAD CORP
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
# if defined(VIVOXDOC) || defined(SWIG)
#   define VIVOXSDK_DLLEXPORT
# else
#   ifdef _MSC_VER
#       ifdef BUILD_SHARED
#           ifdef BUILDING_VIVOXSDK
#               define VIVOXSDK_DLLEXPORT __declspec(dllexport)
#           else
#               define VIVOXSDK_DLLEXPORT __declspec(dllimport)
#           endif
#       else
#           define VIVOXSDK_DLLEXPORT
#       endif
#   else
#       define VIVOXSDK_DLLEXPORT __attribute__ ((visibility("default")))
#   endif
# endif
