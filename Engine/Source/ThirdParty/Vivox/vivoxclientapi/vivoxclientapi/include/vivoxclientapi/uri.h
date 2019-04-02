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
    /// This class holds a typesafe reference to a URI. 
    ///
    /// The maximum length of the URI is 255 bytes
    ///
    class Uri {
    public:
        typedef enum {
            ProtocolTypeNone = -1,
            ProtocolTypeHttp = 0,
            ProtocolTypeHttps = 1,
            ProtocolTypeSip = 2
        } ProtocolType;

        Uri();
        explicit Uri(const char *uri);

        bool IsValid() const;
        void Clear();

        bool operator ==(const Uri &uri) const;
        bool operator !=(const Uri &uri) const;
        Uri & operator =(const Uri &uri);

        bool operator < (const Uri &RHS) const;

        const char *ToString() const { return m_data; }
    private:
        ProtocolType m_protocol;
        char m_data[256];
    };
}


