// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;

namespace nDisplayLauncher.Config
{
	public class TrackerInput : BaseInput, IDataErrorInfo
	{
		public string locationX { get; set; }
		public string locationY { get; set; }
		public string locationZ { get; set; }
		public string rotationP { get; set; }
		public string rotationY { get; set; }
		public string rotationR { get; set; }
		public string front { get; set; }
		public string right { get; set; }
		public string up { get; set; }

		public TrackerInput()
		{
			id = "TrackerInputId";
			address = "TrackerInputName@127.0.0.1";
			type = InputDeviceType.tracker;
			locationX = "0";
			locationY = "0";
			locationZ = "0";
			rotationP = "0";
			rotationY = "0";
			rotationR = "0";
			front = "X";
			right = "Y";
			up = "-Z";
		}

		public TrackerInput(string _id, string _address, string _locationX, string _locationY, string _locationZ, string _rotationP, string _rotationY, string _rotationR, string _front, string _right, string _up)
		{
			id = _id;
			address = _address;
			type = InputDeviceType.tracker;
			locationX = _locationX;
			locationY = _locationY;
			locationZ = _locationZ;
			rotationP = _rotationP;
			rotationY = _rotationY;
			rotationR = _rotationR;
			front = _front;
			right = _right;
			up = _up;
		}

		//Implementation IDataErrorInfo methods for validation
		//ayamashev: check this 'new'
		public new string this[string columnName]
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
				if (columnName == "locationX" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(locationX.ToString()))
					{
						error = "Location X should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "locationY" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(locationY.ToString()))
					{
						error = "Location Y should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "locationZ" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(locationZ.ToString()))
					{
						error = "Location Z should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "rotationP" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(rotationP.ToString()))
					{
						error = "Pitch should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "rotationY" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(rotationY.ToString()))
					{
						error = "Yaw should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "rotationR" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(rotationR.ToString()))
					{
						error = "Roll should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}

				MainWindow.ConfigModifyIndicator();
				return error;
			}
		}
		//ayamashev: check this 'new'
		public new string Error
		{
			get { throw new NotImplementedException(); }
		}

		public new bool Validate()
		{
			bool isValid = ValidationRules.IsName(id) && ValidationRules.IsAddress(address) && ValidationRules.IsFloat(locationX.ToString())
				&& ValidationRules.IsFloat(locationY.ToString()) && ValidationRules.IsFloat(locationZ.ToString()) && ValidationRules.IsFloat(rotationP.ToString())
				&& ValidationRules.IsFloat(rotationY.ToString()) && ValidationRules.IsFloat(rotationR.ToString());
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
			stringCfg = string.Concat(stringCfg, "id=", id, " type=", type.ToString(), " addr=", address,
				" loc=\"X=", locationX, ",Y=", locationY, ",Z=", locationZ, "\"",
				" rot=\"P=", rotationP, ",Y=", rotationY, ",R=", rotationR, "\"",
				" front=", front, " right=", right, " up=", up, "\n");

			return stringCfg;
		}
	}
}