#pragma once
/* Copyright (c) 2014-2018 by Mercer Road Corp
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
#include "Vxc.h"
#include <string>

namespace VivoxClientApi {
    #define USE_SYSTEM_DEVICE_NAME "Default System Device"

    ///
    /// This class holds a typesafe reference to an audio device id, for either audio input devices, or audio output devices.
    ///
    /// The device names and ids are UTF-8 encoded. 
    ///
    class AudioDeviceId {
    public:
        AudioDeviceId();

        explicit AudioDeviceId(const std::string &device_id, const std::string &device_name);
        explicit AudioDeviceId(const char *device_id, const char *device_name);

        bool operator == (const AudioDeviceId &RHS) const;
        bool operator != (const AudioDeviceId &RHS) const;
        AudioDeviceId & operator =(const AudioDeviceId &RHS);
        bool operator < (const AudioDeviceId &RHS) const;
        bool IsValid() const;

        const std::string & GetAudioDeviceId() const;

        ///
        /// This is the value that is shown to the user.
        ///
        const std::string & GetAudioDeviceDisplayName() const;

        const char *ToString() const { return m_displayName.c_str(); }
    private:
        std::string m_deviceId;
        std::string m_displayName;
    };
}
