// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CmdLine.h"
#include "StringUtils.h"
#include "Logging.h"

std::string FCmdLine::Empty;

bool FCmdLine::Parse(int Argc, char* Argv[], bool InCaseSensitive)
{
	this->CaseSensitive = InCaseSensitive;

	if (Argc <= 1)
	{
		return true;
	}

	for (int I = 1; I < Argc; I++)
	{
		const char* Arg = Argv[I];
		if (*Arg == '-')
		{
			Arg++;
		}
		else
		{
			EG_LOG(LogDefault, Error, "Invalid parameter ('%s'). Parameters need to be prefixed with '-'.", Arg);
			// We need parameters to have the '-' prefix
			return false;
		}

		const char* Separator = std::find(Arg, Arg + strlen(Arg), '=');
		if (Separator == Arg + strlen(Arg))
		{
			Params.emplace_back(Arg, "");
		}
		else
		{
			std::string Name(Arg, Separator);
			std::string Value(++Separator);
			Params.emplace_back(std::move(Name), std::move(Value));
		}
	}

	return true;
}

bool FCmdLine::Has(const char* Name) const
{
	for (auto& P : Params)
	{
		if (Equals(P.Name, Name))
		{
			return true;
		}
	}
	return false;
}

const std::string& FCmdLine::Get(const char* Name) const
{
	for (auto& P : Params)
	{
		if (Equals(P.Name, Name))
		{
			return P.Value;
		}
	}
	return Empty;
}

std::pair<bool, int> FCmdLine::GetAsInt(const char* Name, int DefaultValue) const
{
	std::pair<bool, int> Res{false, DefaultValue};
	Res.first = Has(Name);
	if (Res.first)
	{
		Res.second = std::atoi(Get(Name).c_str());
	}

	return Res;
}

int FCmdLine::GetCount() const
{
	return static_cast<int>(Params.size());
}

bool FCmdLine::Equals(const std::string& A, const char* B) const
{
	if (CaseSensitive)
	{
		return A == B ? true : false;
	}
	else
	{
		return CiEquals(A, std::string(B));
	}
}
