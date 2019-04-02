// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;

using nDisplayLauncher.Log;


namespace nDisplayLauncher.Cluster.Config.Entity
{
	public class EntityViewport : EntityBase
	{
		public EntityViewport()
		{
		}

		public EntityViewport(string text)
		{
			try
			{
				InitializeFromText(text);
			}
			catch (Exception ex)
			{
				AppLogger.Log(ex.Message);
			}
		}

		public override void InitializeFromText(string text)
		{
		}
	}
}
