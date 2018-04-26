using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace UnrealGameSyncWebAPI.Models
{
	

	public class TelemetryErrorData
	{
		public enum TelemetryErrorType
		{
			Crash,
		}
		public TelemetryErrorType Type;
		public string Text;
		public string UserName;
		public string Project;
		public DateTime Timestamp;
	}
}