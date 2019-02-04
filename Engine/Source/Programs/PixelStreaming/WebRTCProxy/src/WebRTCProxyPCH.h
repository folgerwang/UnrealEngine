// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
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

#include "Windows/AllowWindowsPlatformAtomics.h"

#if EG_PLATFORM == EG_PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4582) // error C4582: 'webrtc::RTCError::string_message_': constructor is not implicitly called
#pragma warning(disable: 4583) // error C4583 : 'rtc::Optional<std::string>::value_' : destructor is not implicitly called
#pragma warning(disable : 4265) // warning C4265: 'webrtc::VideoCaptureExternal': class has virtual functions, but destructor is not virtual
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
#include "api/video_codecs/video_encoder.h"
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

#include "Windows/HideWindowsPlatformAtomics.h"
