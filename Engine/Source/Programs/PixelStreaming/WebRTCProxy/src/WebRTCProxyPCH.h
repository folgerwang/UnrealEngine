// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <algorithm>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <locale>
#include <inttypes.h>
#include <atomic>

//#include "crazygaze/spas/spas.h"

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
#pragma warning(push)
//#pragma warning(disable : ALL_CODE_ANALYSIS_WARNINGS)
#pragma warning(disable: 26439 ) //  warning C26439: This kind of function may not throw. Declare it 'noexcept' (f.6).
#pragma warning(disable: 6255) // warning C6255: _alloca indicates failure by raising a stack overflow exception.  Consider using _malloca instead.
#pragma warning(disable: 26451) // warning C26451: Arithmetic overflow: Using operator '*' on a 4 byte value and then casting the result to a 8 byte value. Cast the value to the wider type before calling operator '*' to avoid overflow (io.2).
#pragma warning(disable: 26495) // warning C26495: Variable 'webrtc::StringRtpHeaderExtension::value_' is uninitialized. Always initialize a member variable (type.6).
#pragma warning(disable: 26434) // warning C26434: Function 'cricket::VideoCodec::operator!=' hides a non-virtual function 'cricket::Codec::operator!=' (c.128).
#pragma warning(disable: 26444) // warning C26444: Avoid unnamed objects with custom construction and destruction (es.84).
#pragma warning(disable: 4244) // warning C4244: 'argument': conversion from 'const int' to 'float', possible loss of data
#endif
//
// WebRTC headers
//
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_encoder_factory_template.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/test/fakeconstraints.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api\video_codecs\video_encoder.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/i420_buffer.h"

#include "rtc_base/thread.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/json.h"
#include "rtc_base/logging.h"
#include "rtc_base/flags.h"
#include "rtc_base/checks.h"
#include "rtc_base/ssladapter.h"
#include "rtc_base/win32socketinit.h"
#include "rtc_base/win32socketserver.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/physicalsocketserver.h"
#include "rtc_base/signalthread.h"
#include "rtc_base/sigslot.h"
#include "rtc_base/atomicops.h"

// #REFACTOR : Possibly remove this one once we make use of cross-platform sockets
#include "rtc_base/win32.h"
#include "rtc_base/win32socketserver.h"

#include "rtc_base/asynctcpsocket.h"

#include "media/base/videocapturer.h"
#include "media/engine/webrtcvideocapturerfactory.h"
#include "media/engine/internaldecoderfactory.h"
#include "media/base/h264_profile_level_id.h"
#include "media/engine/webrtcvideoencoderfactory.h"
#include "media/base/adaptedvideotracksource.h"
#include "media/base/mediachannel.h"
#include "media/base/videocommon.h"

#include "modules/video_capture/video_capture_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_coding/codecs/h264/include/h264.h"

#include "common_video/h264/h264_bitstream_parser.h"
#include "common_video/h264/h264_common.h"

#include "media/base/videobroadcaster.h"

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
#pragma warning(pop)
#endif
