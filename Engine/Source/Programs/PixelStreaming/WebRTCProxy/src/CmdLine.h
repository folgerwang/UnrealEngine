// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCProxyCommon.h"


/**
 * Utility class to parse command line parameters.
 *
 * Arguments need to be prefixed with '-' and can have the following formats:
 * -SomeArg
 * -SomeOtherArg=Value
 */
class FCmdLine
{
public:
	struct FParam
	{
		template <class T1, class T2>
		FParam(T1&& Name, T2&& Value)
		    : Name(std::forward<T1>(Name))
		    , Value(std::forward<T2>(Value))
		{
		}
		std::string Name;
		std::string Value;
	};

	FCmdLine()
	{
	}

	/**
	 * Parse all the supplied parameters, as received in "main"
	 */
	bool Parse(int Argc, char* Argv[], bool CaseSensitive=false);

	/**
	 * Checks if the specified parameter is present, in any acceptable form, such
	 * ash "-arg" or "-arg=value"
	 */
	bool Has(const char* Name) const;

	/**
	 * Gets the value of the specified parameter.
	 * @return Value of the parameter or an empty string if the parameter doesn't
	 * exist or doesn't is not in the "-arg=value" form
	 * Use "Has" method first, to check if a parameter exists.
	 */
	const std::string& Get(const char* Name) const;

	/**
	 * Gets the value of the specified parameter, as an integer
	 *
	 * @param Name Parameter name
	 * @param DefaultValue If the parameter doesn't exist or is not in the "-arg=value" form, it will default to this
	 * @return
	 *	Pair where "first" is true if the parameter exists, false if not. "second" is the parameter's value or DefaultValue
	 */
	std::pair<bool, int> GetAsInt(const char* Name, int DefaultValue = 0) const;

	/**
	 * @return The number of parameters
	 */
	int GetCount() const;

private:
	bool Equals(const std::string& A, const char* B) const;

	std::vector<FParam> Params;
	static std::string Empty;
	bool CaseSensitive = true;
};
