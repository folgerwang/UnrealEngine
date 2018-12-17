// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet
{

	class TestException : System.Exception
	{
		public TestException(string Msg)
				:base(Msg)
			{
		}
		public TestException(string Format, params object[] Args)
				: base(string.Format(Format, Args))
		{
		}
	}
	
	class DeviceException : TestException
	{
		public DeviceException(string Msg)
				:base(Msg)
			{
		}
		public DeviceException(string Format, params object[] Args)
				: base(Format, Args)
		{
		}
	}	
}
