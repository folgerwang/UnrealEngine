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
    /// The generic type for all error and status codes.
    /// 
    typedef int VCSStatusCode;

	class VCSStatus
	{
	public:
		VCSStatus();
		explicit VCSStatus(VCSStatusCode status);
		VCSStatus(VCSStatusCode status, char *statusString);
		VCSStatus(const VCSStatus &other);
		VCSStatus(VCSStatus &&other);
		~VCSStatus();

		VCSStatus& operator=(const VCSStatus &other);
		VCSStatus& operator=(VCSStatus &&other);
		bool operator==(const VCSStatus &other) const;
		bool operator!=(const VCSStatus &other) const;

		bool IsError() const { return m_status != 0; }
		VCSStatusCode GetStatusCode() const { return m_status; }
		const char* ToString() const;

	private:
		VCSStatusCode m_status;
		char* m_statusString;
	};

    ///
    /// Given a specific error, will return a human readable string for that error.
    /// If there is no string available for that error, it will return and empty string ("")
    ///
	const char *GetErrorString(const VCSStatus& status);
    const char *GetErrorString(VCSStatusCode status);
    
    ///
    /// Retrieves the version number of the SDK
    ///
    const char *GetVersion();

}
