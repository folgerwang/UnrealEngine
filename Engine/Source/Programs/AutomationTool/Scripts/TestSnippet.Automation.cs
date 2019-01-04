// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using AutomationTool;

class TestSnippetUtils : CommandUtils
{
	public static void Test(params object[] Args)
	{
		LogInformation("Args[0]=" + Args[0].ToString());
	}
}