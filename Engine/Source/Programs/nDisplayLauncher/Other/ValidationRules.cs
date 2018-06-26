// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace nDisplayLauncher
{
	public static class ValidationRules
	{
		public static bool IsName(string value)
		{
			return Regex.IsMatch(value, "^[\\w]*$") && !string.IsNullOrEmpty(value);
		}

		public static bool IsNameNullable(string value)
		{
			return Regex.IsMatch(value, "^[\\w]*$");
		}

		public static bool IsFloat(string value)
		{
			return Regex.IsMatch(value, "^[-??\\d]*(?:\\.[0-9]*)?$") && !string.IsNullOrEmpty(value);
		}

		public static bool IsFloatNullable(string value)
		{
			return Regex.IsMatch(value, "^[-??\\d]*(?:\\.[0-9]*)?$");
		}

		public static bool IsInt(string value)
		{
			return Regex.IsMatch(value, "^[\\d]*$") && !string.IsNullOrEmpty(value);
		}

		public static bool IsIntNullable(string value)
		{
			return Regex.IsMatch(value, "^[\\d]*$");
		}

		public static bool IsIp(string value)
		{
			return Regex.IsMatch(value, "^[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$") && !string.IsNullOrEmpty(value);
		}

		//is strin value address word@IpAddress
		public static bool IsAddress(string value)
		{
			return Regex.IsMatch(value, "^\\w*@[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}$");
		}

	}
}
