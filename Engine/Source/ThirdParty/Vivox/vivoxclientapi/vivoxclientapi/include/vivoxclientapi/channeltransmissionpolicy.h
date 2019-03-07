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
    /// This class is used to control which channels are being transmitted into when the user
    /// has joined multiple channels at once.
    ///
    /// There are three options:
    ///
    /// 1. The user can transmit into a specific channel
    /// 2. The user can transmit into all channels.
    /// 3. The user can transmit into no channels.
    /// 
    class ChannelTransmissionPolicy {
    public:
        enum vx_channel_transmission_policy {
            vx_channel_transmission_policy_specific_channel = 0,
            vx_channel_transmission_policy_all = 1,
            vx_channel_transmission_policy_none = 2
        };
        ChannelTransmissionPolicy() {
            m_channelTransmissonPolicy = vx_channel_transmission_policy_all;
        }
        ChannelTransmissionPolicy(const Uri &uri) {
            m_uri = uri;
            m_channelTransmissonPolicy = vx_channel_transmission_policy_specific_channel;
        }
        vx_channel_transmission_policy GetChannelTransmissionPolicy() const {
            return m_channelTransmissonPolicy;
        }
        Uri GetSpecificTransmissionChannel() const {
            return m_uri;
        }
        void SetChannelTransmissionPolicy(const vx_channel_transmission_policy &policy) {
            m_channelTransmissonPolicy = policy;
        }
        void SetSpecificTransmissionChannel(const Uri &uri) {
            m_uri = uri;
        }

    public:
        void SetTransmissionToSpecificChannel(const Uri &uri) {
            m_uri = uri;
            m_channelTransmissonPolicy = vx_channel_transmission_policy_specific_channel;
        }
        void SetTransmissionToAll() {
            m_uri = Uri();
            m_channelTransmissonPolicy = vx_channel_transmission_policy_all;
        }
        void SetTransmissionToNone() {
            m_uri = Uri();
            m_channelTransmissonPolicy = vx_channel_transmission_policy_none;
        }

    private:
        Uri m_uri;
        vx_channel_transmission_policy m_channelTransmissonPolicy;
    };
}
