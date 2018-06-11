// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nDisplayLauncher.Config
{
	public class Screen : ConfigItem, IDataErrorInfo
	{

		public string id { get; set; }
		public string locationX { get; set; }
		public string locationY { get; set; }
		public string locationZ { get; set; }
		public string rotationP { get; set; }
		public string rotationY { get; set; }
		public string rotationR { get; set; }
		public string sizeX { get; set; }
		public string sizeY { get; set; }
		public SceneNode parentWall { get; set; }

		public Screen()
		{
			id = "ScreenId";
			locationX = "0";
			locationY = "0";
			locationZ = "0";
			rotationP = "0";
			rotationR = "0";
			rotationY = "0";
			sizeX = "0";
			sizeY = "0";
			parentWall = null;
		}

		public Screen(string _id, string _locationX, string _locationY, string _locationZ, string _rotationP, string _rotationY, string _rotationR, string _sizeX, string _sizeY, SceneNode _parentWall)
		{
			id = _id;
			locationX = _locationX;
			locationY = _locationY;
			locationZ = _locationZ;
			rotationP = _rotationP;
			rotationR = _rotationR;
			rotationY = _rotationY;
			sizeX = _sizeX;
			sizeY = _sizeY;
			parentWall = _parentWall;
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
						error = "Screen ID should contain only letters, numbers and _";
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
				if (columnName == "sizeX" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(sizeX) || (Convert.ToDouble(sizeX) < 0))
					{
						error = "The X size parameter should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "sizeY" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(sizeY) || (Convert.ToDouble(sizeY) < 0))
					{
						error = "The Y size parameter should be a floating point number";
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
			bool isValid = ValidationRules.IsName(id) && ValidationRules.IsFloat(locationX.ToString()) && ValidationRules.IsFloat(locationY.ToString())
				&& ValidationRules.IsFloat(locationZ.ToString()) && ValidationRules.IsFloat(rotationP.ToString()) && ValidationRules.IsFloat(rotationY.ToString())
				&& ValidationRules.IsFloat(rotationR.ToString()) && (ValidationRules.IsFloat(sizeX) || (Convert.ToDouble(sizeX) > 0)) && (ValidationRules.IsFloat(sizeY) || (Convert.ToDouble(sizeY) > 0));
			if (!isValid)
			{
				AppLogger.Add("ERROR! Errors in Screen [" + id + "]");
				string a = this[validationName];

			}

			return isValid;
		}

		//Create String of screen parameters for config file
		public override string CreateCfg()
		{
			string stringCfg = "[screen] ";
			stringCfg = string.Concat(stringCfg, "id=", id, " loc=\"X=", locationX, ",Y=", locationY, ",Z=", locationZ,
				"\" rot=\"P=", rotationP, ",Y=", rotationY, ",R=", rotationR, "\" size=\"X=", sizeX, ",Y=", sizeY, "\"");
			if (parentWall != null)
			{
				stringCfg = string.Concat(stringCfg, " parent=", parentWall.id);
			}

			stringCfg = string.Concat(stringCfg, "\n");
			return stringCfg;
		}
	}
}
