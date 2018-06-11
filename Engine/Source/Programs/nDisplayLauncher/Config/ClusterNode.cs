// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.ComponentModel;

namespace nDisplayLauncher.Config
{
	public class ClusterNode : ConfigItem, IDataErrorInfo
	{

		public string id { get; set; }
		public bool isMaster { get; set; }
		public string address { get; set; }
		public Screen screen { get; set; }
		public Viewport viewport { get; set; }
		public string camera { get; set; }
		public bool isWindowed { get; set; }
		public string winX { get; set; }
		public string winY { get; set; }
		public string resX { get; set; }
		public string resY { get; set; }
		public ClusterNode()
		{
			id = "ClusterNodeId";
			address = "127.0.0.1";
			screen = null;
			viewport = null;
			camera = string.Empty;
			isMaster = false;
			isWindowed = false;
			winX = string.Empty;
			winY = string.Empty;
			resX = string.Empty;
			resY = string.Empty;
		}

		public ClusterNode(string _id, string _address, Screen _screen, Viewport _viewport, string _camera, bool _isMaster)
		{
			id = _id;
			address = _address;
			screen = _screen;
			viewport = _viewport;
			camera = _camera;
			isMaster = _isMaster;
			isWindowed = false;
			winX = string.Empty;
			winY = string.Empty;
			resX = string.Empty;
			resY = string.Empty;
		}

		public ClusterNode(string _id, string _address, Screen _screen, Viewport _viewport, string _camera, bool _isMaster, bool _isWindowed, string _winX, string _winY, string _resX, string _resY)
		{
			id = _id;
			address = _address;
			screen = _screen;
			viewport = _viewport;
			camera = _camera;
			isMaster = _isMaster;
			isWindowed = _isWindowed;
			winX = _winX;
			winY = _winY;
			resX = _resX;
			resY = _resY;
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
						error = "Cluster node ID should contain only letters, numbers and _";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "address" || columnName == validationName)
				{
					if (!ValidationRules.IsIp(address))
					{
						error = "Cluster node address should be IP address";
						AppLogger.Add("ERROR! " + error);
					}
				}

				if (columnName == "winX" || columnName == validationName)
				{
					if (isWindowed == true)
					{
						if (!ValidationRules.IsInt(winX.ToString()))
						{
							error = "x should be an integer";
							AppLogger.Add("ERROR! " + error);
						}
					}
				}
				if (columnName == "winY" || columnName == validationName)
				{
					if (isWindowed == true)
					{
						if (!ValidationRules.IsInt(winY.ToString()))
						{
							error = "y should be an integer";
							AppLogger.Add("ERROR! " + error);
						}
					}
				}
				if (columnName == "resX" || columnName == validationName)
				{
					if (isWindowed == true)
					{
						if (!ValidationRules.IsInt(resX.ToString()) || Convert.ToInt32(resX) < 0)
						{
							error = "Width should be an integer";
							AppLogger.Add("ERROR! " + error);
						}
					}
				}

				if (columnName == "resY" || columnName == validationName)
				{
					if (isWindowed == true)
					{
						if (!ValidationRules.IsInt(resY.ToString()) || Convert.ToInt32(resY) < 0)
						{
							error = "Height should be an integer";
							AppLogger.Add("ERROR! " + error);
						}
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
			bool isValid = ValidationRules.IsName(id) && ValidationRules.IsIp(address);
			if (!isValid)
			{
				AppLogger.Add("ERROR! Errors in Cluster Node [" + id + "]");
				string a = this[validationName];

			}

			return isValid;
		}

		public override string CreateCfg()
		{
			string stringCfg = "[cluster_node] ";
			stringCfg = string.Concat(stringCfg, "id=", id, " addr=", address);
			if (screen != null)
			{
				stringCfg = string.Concat(stringCfg, " screen=", screen.id);
			}
			if (viewport != null)
			{

				stringCfg = string.Concat(stringCfg, " viewport=", viewport.id);
			}

			if (isWindowed)
			{
				if (string.IsNullOrEmpty(winX)) winX = "0";
				if (string.IsNullOrEmpty(winY)) winY = "0";
				if (string.IsNullOrEmpty(resX)) resX = "0";
				if (string.IsNullOrEmpty(resY)) resY = "0";
				stringCfg = string.Concat(stringCfg, " windowed=true ", " WinX=", winX, " WinY=", winY, " ResX=", resX, " ResY=", resY);
			}

			if (isMaster)
			{
				MainWindow Win = (MainWindow)Application.Current.MainWindow;
				string portCS = Win.CurrentConfig.portCs;
				string portSS = Win.CurrentConfig.portSs;
				stringCfg = string.Concat(stringCfg, " port_cs=", portCS, " port_ss=", portSS, " master=true");
			}
			stringCfg = string.Concat(stringCfg, "\n");
			return stringCfg;
		}

	}
}
