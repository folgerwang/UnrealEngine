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
#include "vivoxclientapi/audiodeviceid.h"
#include <string.h>

namespace VivoxClientApi {
    AudioDeviceId::AudioDeviceId() {
        m_deviceId = "";
        m_displayName = "";
    }
    AudioDeviceId::AudioDeviceId(const std::string &device_id, const std::string &display_name) {
        m_deviceId = device_id;
        m_displayName = display_name;
    }
    AudioDeviceId::AudioDeviceId(const char *device_id, const char *display_name) {
        m_deviceId = device_id;
        m_displayName = display_name;
    }
    const std::string & AudioDeviceId::GetAudioDeviceId() const {
        return m_deviceId;
    }
    const std::string & AudioDeviceId::GetAudioDeviceDisplayName() const {
        return m_displayName;
    }
    bool AudioDeviceId::operator == (const AudioDeviceId &RHS) const {
        return m_deviceId == RHS.m_deviceId;
    }
    bool AudioDeviceId::operator != (const AudioDeviceId &RHS) const {
        return !operator==(RHS);
    }
    AudioDeviceId & AudioDeviceId::operator =(const AudioDeviceId &RHS) {
        m_deviceId = RHS.m_deviceId;
        m_displayName = RHS.m_displayName;
        return *this;
    }
    bool AudioDeviceId::operator < (const AudioDeviceId &RHS) const {
        return m_deviceId < RHS.m_deviceId;
    }
    bool AudioDeviceId::IsValid() const {
        return m_deviceId != "";
    }
}
