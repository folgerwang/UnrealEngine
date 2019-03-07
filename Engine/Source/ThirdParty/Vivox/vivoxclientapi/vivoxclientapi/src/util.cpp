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
#include "vivoxclientapi/util.h"
#include "VxcErrors.h"

namespace VivoxClientApi {

	VCSStatus::VCSStatus()
		: m_status(0)
		, m_statusString(NULL)
	{
	}
	VCSStatus::VCSStatus(VCSStatusCode status)
		: m_status(status)
		, m_statusString(NULL)
	{
	}
	VCSStatus::VCSStatus(VCSStatusCode status, char *statusString)
		: m_status(status)
		, m_statusString(vx_strdup(statusString))
	{
	}
	VCSStatus::VCSStatus(const VCSStatus &other)
	{
		m_status = other.m_status;
		m_statusString = other.m_statusString ? vx_strdup(other.m_statusString) : NULL;
	}
	VCSStatus::VCSStatus(VCSStatus &&other)
	{
		m_status = other.m_status;
		m_statusString = other.m_statusString;

		other.m_status = 0;
		other.m_statusString = NULL;
	}
	VCSStatus::~VCSStatus()
	{
		if (m_statusString)
		{
			vx_free(m_statusString);
		}
	}

	VCSStatus& VCSStatus::operator=(const VCSStatus &other)
	{
		char* errorString = other.m_statusString ? vx_strdup(other.m_statusString) : NULL;
		if (m_statusString)
		{
			vx_free(m_statusString);
		}
		m_statusString = errorString;
		return *this;
		
	}
	VCSStatus& VCSStatus::operator=(VCSStatus &&other)
	{
		if (&other != this)
		{
			if (m_statusString)
			{
				vx_free(m_statusString);
			}

			m_status = other.m_status;
			m_statusString = other.m_statusString;

			other.m_status = 0;
			other.m_statusString = NULL;
		}
		return *this;

	}
	bool VCSStatus::operator==(const VCSStatus &other) const
	{
		return m_status == other.m_status;
	}
	bool VCSStatus::operator!=(const VCSStatus &other) const
	{
		return m_status != other.m_status;
	}

	const char *VCSStatus::ToString() const
	{
		return m_statusString ? m_statusString : GetErrorString(m_status);
	}
	const char *GetErrorString(const VCSStatus& status)
	{
		return status.ToString();
	}
    const char *GetErrorString(VCSStatusCode status)
    {
        return vx_get_error_string(status);
    }
    const char *GetVersion()
    {
        return vx_get_sdk_version_info();
    }
}