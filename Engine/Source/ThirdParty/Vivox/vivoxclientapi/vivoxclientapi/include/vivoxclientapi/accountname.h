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

namespace VivoxClientApi
{
    ///
    /// Utility class for holding type safe references to Vivox account names.
    ///
    /// Vivox account names are the user portion of a SIP URI in the form of sip:user@domain
    ///
    class AccountName {
    public:
        AccountName();
        explicit AccountName(const char *name);

        bool operator == (const AccountName &RHS) const; 
        AccountName & operator =(const AccountName &RHS);
        bool operator < (const AccountName &RHS) const;
        bool IsValid() const;
        bool IsAnonymous() const { return 0 == m_name[0]; }
        const char *ToString() const { return m_name; }
    private:
        char m_name[64];
    };
}


