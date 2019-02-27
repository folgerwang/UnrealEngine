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
#include "vivoxclientapi/uri.h"
#include <string.h>
#include <memory.h>
#include <stdlib.h>

namespace VivoxClientApi {

    Uri::Uri() {
        m_data[0] = 0;
        m_protocol = ProtocolTypeNone;
    }
    Uri::Uri(const char *uri) {
        m_data[0] = 0;
        m_protocol = ProtocolTypeNone;
		if (uri == NULL) {
			return;
		}
        if (strstr(uri, "https://") == uri) {
            m_protocol = ProtocolTypeHttps;
        }
        else if (strstr(uri, "sip:") == uri) {
            m_protocol = ProtocolTypeSip;
        }
        else if (strstr(uri, "http://") == uri) {
            m_protocol = ProtocolTypeHttp;
        }
        else {
            return;
        }
        size_t len = strlen(uri);
        if (len < (sizeof(m_data) - 1)) {
            memcpy(m_data, uri, len + 1);
        }
        else {
            m_protocol = ProtocolTypeNone;
        }
    }

    bool Uri::IsValid() const { return m_protocol != ProtocolTypeNone; }
    void Uri::Clear() { m_protocol = ProtocolTypeNone; m_data[0] = 0; }

    bool Uri::operator ==(const Uri &uri) const {
        return !strcmp(uri.m_data, m_data);
    }
    bool Uri::operator !=(const Uri &uri) const {
        return !this->operator ==(uri);
    }
    Uri & Uri::operator =(const Uri &uri) {
        if (this == &uri)
            return *this;
        m_protocol = uri.m_protocol;
        memcpy(m_data, uri.m_data, sizeof(m_data));
        return *this;
    }

    bool Uri::operator < (const Uri &RHS) const {
        return strcmp(m_data, RHS.m_data) < 0;
    }

}


