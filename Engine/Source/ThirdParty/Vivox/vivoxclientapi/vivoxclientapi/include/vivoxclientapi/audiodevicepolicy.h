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

namespace VivoxClientApi {
    ///
    /// This class holds a reference to an audio device policy object.
    ///
    /// The audio device policy allows the user to select a specific device or to use the device that is 
    /// set in the control panel.
    ///
    /// In the case that the user unplugs a device, and the user has specific selected that device using
    /// the vx_audio_device_policy_specific_device, there will be no fail over to the next available device.
    ///
    /// This policy is useful when people don't want their conversations broadcast over speakers under any circumstances.,
    ///
    class AudioDevicePolicy {
    public:
        enum vx_audio_device_policy {
            vx_audio_device_policy_specific_device = 0,
            vx_audio_device_policy_default_system = 1
        };
        AudioDevicePolicy() {
            m_audioDevicePolicy = vx_audio_device_policy_default_system;
            m_specificAudioDevice = AudioDeviceId(USE_SYSTEM_DEVICE_NAME, USE_SYSTEM_DEVICE_NAME);
        }
        AudioDevicePolicy(const AudioDeviceId &deviceName) {
            m_specificAudioDevice = deviceName;
            m_audioDevicePolicy = vx_audio_device_policy_specific_device;
        }
        vx_audio_device_policy GetAudioDevicePolicy() const {
            return m_audioDevicePolicy;
        }
        const AudioDeviceId & GetSpecificAudioDevice() const {
            return m_specificAudioDevice;
        }
        void SetSpecificAudioDevice(const AudioDeviceId &deviceName) {
            m_specificAudioDevice = deviceName;
            m_audioDevicePolicy = vx_audio_device_policy_specific_device;
        }
        void SetUseDefaultAudioDevice() {
            m_specificAudioDevice = AudioDeviceId(USE_SYSTEM_DEVICE_NAME, USE_SYSTEM_DEVICE_NAME);
            m_audioDevicePolicy = vx_audio_device_policy_default_system;
        }

        bool operator ==(const AudioDevicePolicy &RHS) const {
            return m_audioDevicePolicy == RHS.m_audioDevicePolicy &&
                m_specificAudioDevice == RHS.m_specificAudioDevice;
        }
    private:
        AudioDeviceId m_specificAudioDevice;
        vx_audio_device_policy m_audioDevicePolicy;
    };
}
