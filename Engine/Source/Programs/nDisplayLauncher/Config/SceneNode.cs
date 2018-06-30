// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace nDisplayLauncher.Config
{
	public class SceneNode : ConfigItem, IDataErrorInfo
	{
		public string id { get; set; }
		//public string nodeType { get; set; }
		public string locationX { get; set; }
		public string locationY { get; set; }
		public string locationZ { get; set; }
		public string rotationP { get; set; }
		public string rotationY { get; set; }
		public string rotationR { get; set; }
		public TrackerInput tracker { get; set; }
		public string trackerCh { get; set; }

		public SceneNode parent { get; set; }

		public SceneNode()
		{
			id = "SceneNodeId";
			locationX = "0";
			locationY = "0";
			locationZ = "0";
			rotationP = "0";
			rotationR = "0";
			rotationY = "0";
			trackerCh = "0";
			tracker = new TrackerInput();
			parent = null;
		}

		public SceneNode(string _id, string _locationX, string _locationY, string _locationZ, string _rotationP, string _rotationY, string _rotationR, TrackerInput _tracker, string _trackerCh, SceneNode _parent)
		{
			id = _id;
			locationX = _locationX;
			locationY = _locationY;
			locationZ = _locationZ;
			rotationP = _rotationP;
			rotationY = _rotationY;
			rotationR = _rotationR;
			tracker = _tracker;
			trackerCh = _trackerCh;
			parent = _parent;
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
						error = "Scene Nodes ID should contain only letters, numbers and _";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "locationX" || columnName == validationName)
				{
					if (!ValidationRules.IsFloatNullable(locationX.ToString()))
					{
						error = "Location X should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "locationY" || columnName == validationName)
				{
					if (!ValidationRules.IsFloatNullable(locationY.ToString()))
					{
						error = "Location Y should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "locationZ" || columnName == validationName)
				{
					if (!ValidationRules.IsFloatNullable(locationZ.ToString()))
					{
						error = "Location Z should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "rotationP" || columnName == validationName)
				{
					if (!ValidationRules.IsFloatNullable(rotationP.ToString()))
					{
						error = "Pitch should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "rotationY" || columnName == validationName)
				{
					if (!ValidationRules.IsFloatNullable(rotationY.ToString()))
					{
						error = "Yaw should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "rotationR" || columnName == validationName)
				{
					if (!ValidationRules.IsFloatNullable(rotationR.ToString()))
					{
						error = "Roll should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "trackerCh" || columnName == validationName)
				{
					if (!ValidationRules.IsIntNullable(trackerCh))
					{
						error = "Tracker channel should be a positive integer";
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
			bool isValid = ValidationRules.IsName(id) && ValidationRules.IsFloatNullable(locationX.ToString()) && ValidationRules.IsFloatNullable(locationY.ToString())
				&& ValidationRules.IsFloatNullable(locationZ.ToString()) && ValidationRules.IsFloatNullable(rotationP.ToString())
				 && ValidationRules.IsFloatNullable(rotationY.ToString()) && ValidationRules.IsFloatNullable(rotationR.ToString());
			if (!isValid)
			{
				AppLogger.Add("ERROR! Errors in Scene Node [" + id + "]");
				string a = this[validationName];

			}

			return isValid;
		}

		public override string CreateCfg()
		{
			string stringCfg = "[scene_node] ";
			stringCfg = string.Concat(stringCfg, "id=", id);
			if (!string.IsNullOrEmpty(locationX) && !string.IsNullOrEmpty(locationY) && !string.IsNullOrEmpty(locationZ)
				&& !string.IsNullOrEmpty(rotationP) && !string.IsNullOrEmpty(rotationY) && !string.IsNullOrEmpty(rotationR))
			{
				stringCfg = string.Concat(stringCfg, " loc=\"X=", locationX, ",Y=", locationY, ",Z=", locationZ,
				"\" rot=\"P=", rotationP, ",Y=", rotationY, ",R=", rotationR, "\"");
			}
			if (!string.IsNullOrEmpty(trackerCh) && tracker != null)
			{
				stringCfg = string.Concat(stringCfg, " tracker_id=", tracker.id, " tracker_ch=", trackerCh);
			}
			if (parent != null)
			{
				stringCfg = string.Concat(stringCfg, " parent=", parent.id);
			}
			stringCfg = string.Concat(stringCfg, "\n");
			return stringCfg;
		}


	}
}
