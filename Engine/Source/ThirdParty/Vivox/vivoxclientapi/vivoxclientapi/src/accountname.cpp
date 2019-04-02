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

#include "vivoxclientapi/accountname.h"
#include <string.h>
#include <assert.h>
#include "vivoxclientapi/types.h"

# if defined(WIN32) || defined(_XBOX_ONE)
#   define strcmp_without_case _strcmpi
#   define safe_strcpy strcpy_s
# else
#   define strcmp_without_case strcasecmp
#   define safe_strcpy strcpy
# endif

namespace VivoxClientApi {
    AccountName::AccountName() {
        m_name[0] = 0;
    }
    AccountName::AccountName(const char *name) {
        if (name == NULL) {
            m_name[0] = 0;
            return;
        }
        size_t len = strlen(name);
        m_name[0] = 0;
		if (len < sizeof(m_name) - 1)
		{
			memcpy(m_name, name, len + 1);
		}
    }
    bool AccountName::operator == (const AccountName &RHS) const {
        return strcmp_without_case(m_name, RHS.m_name) == 0;
    }
    AccountName & AccountName::operator =(const AccountName &RHS) {
        safe_strcpy(m_name, RHS.m_name);
        return *this;
    }
    bool AccountName::operator < (const AccountName &RHS) const {
        return strcmp_without_case(m_name, RHS.m_name) < 0;
    }
    bool AccountName::IsValid() const {
        return m_name[0] != 0;
    }
}