// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Text.RegularExpressions;


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
