// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nDisplayLauncher.Config
{
	public enum InputDeviceType
	{
		tracker,
		analog,
		buttons
	}

	public class BaseInput : ConfigItem, IDataErrorInfo
	{

		public string id { get; set; }
		public InputDeviceType type { get; set; }
		public string address { get; set; }

		public BaseInput()
		{
			id = "InputId";
			address = "InputName@127.0.0.1";
		}

		public BaseInput(string _id, InputDeviceType _type, string _address)
		{
			id = _id;
			type = _type;
			address = _address;
		}

		//Implementation IDataErrorInfo methods for validation
		public string this[string columnName]
		{
			get
			{
				string error = String.Empty;
				if (columnName == "id" || columnName == validationName)
				{
					if (!ValidationRules.IsName(id))
					{
						error = "Input ID should contain only letters, numbers and _";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "address" || columnName == validationName)
				{
					if (!ValidationRules.IsAddress(address))
					{
						error = "Input Address in format InputName@127.0.0.1";
						AppLogger.Add("ERROR! " + error);
					}
				}
				MainWindow.ConfigModifyIndicator();
				return error;
			}
		}
		public string Error
		{
			get { throw new NotImplementedException(); }
		}

		public override bool Validate()
		{
			bool isValid = ValidationRules.IsName(id) && ValidationRules.IsAddress(address);
			if (!isValid)
			{
				AppLogger.Add("ERROR! Errors in Input [" + id + "]");
				string a = this[validationName];

			}

			return isValid;
		}

		public override string CreateCfg()
		{
			string stringCfg = "[input] ";
			stringCfg = string.Concat(stringCfg, "id=", id, " type=", type.ToString(), " addr=", address, "\n");

			return stringCfg;
		}
	}
}