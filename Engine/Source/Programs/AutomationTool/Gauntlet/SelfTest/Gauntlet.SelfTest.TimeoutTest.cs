// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet.SelfTest
{
	class TimeoutTest : BaseNode
	{
		DateTime StartTime;

		public override float MaxDuration
		{
			get		{	return 4;	}
		}

		public override bool LaunchTest()
		{
			StartTime = DateTime.Now;
			return base.LaunchTest();
		}

		public override void OnTick()
		{
			// Fail us if we run too long
			if ((DateTime.Now - StartTime).TotalSeconds > MaxDuration * 2)
			{
				MarkComplete(TestResult.Failed);
			}
		}

		public override void OnComplete(bool WasCancelled)
		{
			// we pass, if we're canceled :)
			if (WasCancelled)
			{
				MarkComplete(TestResult.Passed);
			}
			else
			{
				MarkComplete(TestResult.Failed);
			}
		}
	}
}
