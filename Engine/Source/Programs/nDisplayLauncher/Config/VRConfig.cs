// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ComponentModel;
using System.Collections.Specialized;
using System.Collections;
using System.Text.RegularExpressions;

namespace nDisplayLauncher.Config
{
	public class VRConfig : INotifyPropertyChanged, IDataErrorInfo
	{
		static readonly Dictionary<string, string> header = new Dictionary<string, string>
		{
			{ "main", "#####################################################################\n# Info:\n# Note:\n#\n#####################################################################\n" },
			{ "screen", "\n# List of screen configurations (transformations are in nDisplay space relative to the root node)\n"},
			{ "camera", "\n# List of cameras \n"},
			{ "viewport", "\n# List of viewport configurations \n"},
			{ "cluster_node", "\n# List of cluster nodes \n"},
			{ "stereo", "\n# eye_swap - false(L|R) <--> true(R|L) eye switch \n# eye_dist - interoccular distance (meters)\n"},
			{"scene_node", "\n# List of empty hierarchy nodes (transforms) \n" },
			{ "debug", "\n# lag_simulation - enable/disable lag simulation \n# lag_max_time   - maximum delay time for randome delay simulation \n"},
			{ "general", "\n# 0 - no swap sync (V-sync off) \n# 1 - software swap synchronization over network \n# 2 - NVIDIA hardware swap synchronization (nv swaplock)\n"},
			{ "input", "\n"}
		};

		private Dictionary<string, string> _swapSyncPolicy = new Dictionary<string, string>
		{
			{ "0", "no swap sync (V-sync off)" },
			{ "1", "software swap synchronization over network" },
			{ "2", "NVIDIA hardware swap synchronization (nv swaplock)" }
		};


		public Dictionary<string, string> swapSyncPolicy
		{
			get { return _swapSyncPolicy; }
			set { Set(ref _swapSyncPolicy, value, "swapSyncPolicy"); }
		}

		private string _cameraLocationX;
		public string cameraLocationX
		{
			get { return _cameraLocationX; }
			set { Set(ref _cameraLocationX, value, "cameraLocationX"); }
		}

		private string _cameraLocationY;
		public string cameraLocationY
		{
			get { return _cameraLocationY; }
			set { Set(ref _cameraLocationY, value, "cameraLocationY"); }
		}

		private string _cameraLocationZ;
		public string cameraLocationZ
		{
			get { return _cameraLocationZ; }
			set { Set(ref _cameraLocationZ, value, "cameraLocationZ"); }
		}

		//List of inputs
		private List<BaseInput> _inputs;
		public List<BaseInput> inputs
		{
			get { return _inputs; }
			set { Set(ref _inputs, value, "inputs"); }
		}

		//Selected input
		private BaseInput _selectedInput;
		public BaseInput selectedInput
		{
			get { return _selectedInput; }
			set { Set(ref _selectedInput, value, "selectedInput"); }
		}

		private TrackerInput _cameraTracker;
		public TrackerInput cameraTracker
		{
			get { return _cameraTracker; }
			set { Set(ref _cameraTracker, value, "cameraTracker"); }
		}

		private string _cameraTrackerCh;
		public string cameraTrackerCh
		{
			get { return _cameraTrackerCh; }
			set { Set(ref _cameraTrackerCh, value, "cameraTrackerCh"); }
		}

		//List of cluster nodes
		private List<ClusterNode> _clusterNodes;
		public List<ClusterNode> clusterNodes
		{
			get { return _clusterNodes; }
			set { Set(ref _clusterNodes, value, "clusterNodes"); }
		}
		//Selected cluster node
		private ClusterNode _selectedNode;
		public ClusterNode selectedNode
		{
			get { return _selectedNode; }
			set { Set(ref _selectedNode, value, "selectedNode"); }
		}


		//List of screens
		private List<Screen> _screens;
		public List<Screen> screens
		{
			get { return _screens; }
			set { Set(ref _screens, value, "screens"); }

		}
		//Selected screen
		private Screen _selectedScreen;
		public Screen selectedScreen
		{
			get { return _selectedScreen; }
			set { Set(ref _selectedScreen, value, "selectedScreen"); }
		}

		//List of viewports
		private List<Viewport> _viewports;
		public List<Viewport> viewports
		{
			get { return _viewports; }
			set { Set(ref _viewports, value, "viewports"); }
		}
		//Selected Viewport
		private Viewport _selectedViewport;
		public Viewport selectedViewport
		{
			get { return _selectedViewport; }
			set { Set(ref _selectedViewport, value, "selectedViewport"); }
		}

		//List of scene nodes
		private List<SceneNode> _sceneNodes;
		public List<SceneNode> sceneNodes
		{
			get { return _sceneNodes; }
			set { Set(ref _sceneNodes, value, "sceneNodes"); }
		}

		//List of scene nodes view
		private List<SceneNodeView> _sceneNodesView;
		public List<SceneNodeView> sceneNodesView
		{
			get { return _sceneNodesView; }
			set { Set(ref _sceneNodesView, value, "sceneNodesView"); }
		}

		//Selected SceneNodeView
		private SceneNodeView _selectedSceneNodeView;
		public SceneNodeView selectedSceneNodeView
		{
			get { return _selectedSceneNodeView; }
			set { Set(ref _selectedSceneNodeView, value, "selectedSceneNodeView"); }
		}


		//Master node settings
		private string _portCs;
		public string portCs
		{
			get { return _portCs; }
			set { Set(ref _portCs, value, "portCs"); }
		}

		private string _portSs;
		public string portSs
		{
			get { return _portSs; }
			set { Set(ref _portSs, value, "portSs"); }
		}

		//Stereo settings
		private string _eyeDist;
		public string eyeDist
		{
			get { return _eyeDist; }
			set { Set(ref _eyeDist, value, "eyeDist"); }
		}

		private bool _eyeSwap;
		public bool eyeSwap
		{
			get { return _eyeSwap; }
			set { Set(ref _eyeSwap, value, "eyeSwap"); }
		}

		//Debug settings
		private bool _lagSimulation;
		public bool lagSimulation
		{
			get { return _lagSimulation; }
			set { Set(ref _lagSimulation, value, "lagSimulation"); }
		}

		private string _lagMaxTime;
		public string lagMaxTime
		{
			get { return _lagMaxTime; }
			set { Set(ref _lagMaxTime, value, "lagMaxTime"); }
		}

		private bool _drawStats;
		public bool drawStats
		{
			get { return _drawStats; }
			set { Set(ref _drawStats, value, "drawStats"); }
		}

		private KeyValuePair<string, string> _selectedSwapSync;
		public KeyValuePair<string, string> selectedSwapSync
		{
			get { return _selectedSwapSync; }
			set { Set(ref _selectedSwapSync, value, "selectedSwapSync"); }
		}

		//Config name(FileName witout extention)
		public string name = "New Config";

		//String for validation all properties
		string validationName = "Object validation";

		public VRConfig()
		{
			clusterNodes = new List<ClusterNode>();
			viewports = new List<Viewport>();
			sceneNodes = new List<SceneNode>();
			sceneNodes = new List<SceneNode>();
			sceneNodesView = ConvertSceneNodeList(sceneNodes);
			screens = new List<Screen>();
			inputs = new List<BaseInput>();
			selectedSceneNodeView = new SceneNodeView(new SceneNode
			{
				id = string.Empty,
				locationX = string.Empty,
				locationY = string.Empty,
				locationZ = string.Empty,
				rotationP = string.Empty,
				rotationY = string.Empty,
				rotationR = string.Empty,
				tracker = new TrackerInput(),
				trackerCh = string.Empty,
				parent = null
			});


			cameraLocationX = "0";
			cameraLocationY = "0";
			cameraLocationZ = "0";
			cameraTracker = null;
			cameraTrackerCh = "0";

			//Stereo settings
			eyeDist = "0.064";
			eyeSwap = false;

			//Debug settings
			lagSimulation = false;
			lagMaxTime = "0";
			drawStats = false;

			//Master node settings
			portCs = "00001";
			portSs = "00000";

			//General settings
			string defaultSwapSync = "1";
			if (swapSyncPolicy.ContainsKey(defaultSwapSync))
			{
				selectedSwapSync = swapSyncPolicy.FirstOrDefault(x => x.Key == defaultSwapSync);
			}


		}
		//Implementation IDataErrorInfo methods
		public string this[string columnName]
		{
			get
			{
				string error = String.Empty;
				if (columnName == "cameraLocationX" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(cameraLocationX.ToString()))
					{
						error = "Camera location X should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "cameraLocationY" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(cameraLocationY.ToString()))
					{
						error = "Camera location Y should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "cameraLocationZ" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(cameraLocationZ.ToString()))
					{
						error = "Camera location Z should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "cameraTrackerCh" || columnName == validationName)
				{
					if (!ValidationRules.IsIntNullable(cameraTrackerCh))
					{
						error = "Camera tracker channel should be an integer";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "eyeDist" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(eyeDist.ToString()))
					{
						error = "Eye distance should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "lagMaxTime" || columnName == validationName)
				{
					if (!ValidationRules.IsFloat(lagMaxTime.ToString()))
					{
						error = "Maximum delay time should be a floating point number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "portCs" || columnName == validationName)
				{
					if (!(ValidationRules.IsInt(portCs) || Convert.ToInt32(portCs) > 0))
					{
						error = "Client port should be a positive number";
						AppLogger.Add("ERROR! " + error);
					}
				}
				if (columnName == "portSs" || columnName == validationName)
				{
					if (!(ValidationRules.IsInt(portSs) || Convert.ToInt32(portSs) > 0))
					{
						error = "Server port should be a positive number";
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

		public bool Validate()
		{
			bool isValid = ValidationRules.IsFloat(cameraLocationX.ToString()) && ValidationRules.IsFloat(cameraLocationY.ToString())
				&& ValidationRules.IsFloat(cameraLocationZ.ToString()) && ValidationRules.IsIntNullable(cameraTrackerCh) && ValidationRules.IsFloat(eyeDist)
				&& ValidationRules.IsFloat(lagMaxTime) && (ValidationRules.IsInt(portCs) || (Convert.ToDouble(portCs) > 0)) && (ValidationRules.IsInt(portSs) || (Convert.ToDouble(portSs) > 0));
			if (!isValid)
			{
				string a = this[validationName];
			}
			//Validate items in lists
			isValid = isValid && ValidateList<Screen>(ref _screens);
			isValid = isValid && ValidateList<ClusterNode>(ref _clusterNodes);
			isValid = isValid && ValidateList<Viewport>(ref _viewports);
			isValid = isValid && ValidateList<SceneNode>(ref _sceneNodes);
			isValid = isValid && ValidateList<BaseInput>(ref _inputs);

			//Check id uniqueness for listitems
			if (screens.GroupBy(n => n.id).Any(c => c.Count() > 1))
			{
				isValid = false;
				AppLogger.Add("ERROR! All Screen Id's should be unique! Rename duplicate Id's");
			}
			if (clusterNodes.GroupBy(n => n.id).Any(c => c.Count() > 1))
			{
				isValid = false;
				AppLogger.Add("ERROR! All Cluster node Id's should be unique! Rename duplicate Id's");
			}
			if (viewports.GroupBy(n => n.id).Any(c => c.Count() > 1))
			{
				isValid = false;
				AppLogger.Add("ERROR! All Viewport Id's should be unique! Rename duplicate Id's");
			}
			if (sceneNodes.GroupBy(n => n.id).Any(c => c.Count() > 1))
			{
				isValid = false;
				AppLogger.Add("ERROR! All Scene node Id's should be unique! Rename duplicate Id's");
			}
			if (inputs.GroupBy(n => n.id).Any(c => c.Count() > 1))
			{
				isValid = false;
				AppLogger.Add("ERROR! All Input Id's should be unique! Rename duplicate Id's");
			}

			//check master node
			if (!clusterNodes.Exists(x => x.isMaster))
			{
				isValid = false;
				AppLogger.Add("ERROR! Master Cluster Node is not selected! Check Master Node");
			}

			return isValid;
		}

		bool ValidateList<T>(ref List<T> list) where T : ConfigItem
		{
			bool isValid = true;
			foreach (T item in list)
			{
				isValid = isValid && item.Validate();
			}
			return isValid;
		}




		//Implementation of INotifyPropertyChanged method for TwoWay binding
		public event PropertyChangedEventHandler PropertyChanged;

		protected void OnNotifyPropertyChanged(string propertyName)
		{
			if (PropertyChanged != null)
			{
				{
					PropertyChanged(this, new PropertyChangedEventArgs(propertyName));
				}
			}
		}

		//Set property with OnNotifyPropertyChanged call
		protected void Set<T>(ref T field, T newValue, string propertyName)
		{
			field = newValue;
			OnNotifyPropertyChanged(propertyName);
		}

		public string CreateConfig()
		{
			string configFile = string.Empty;
			configFile = string.Concat(string.Empty, header["main"], header["cluster_node"], CreateListCfg<ClusterNode>(ref _clusterNodes),
				header["screen"], CreateListCfg<Screen>(ref _screens),
				header["viewport"], CreateListCfg<Viewport>(ref _viewports),
				header["camera"], "[camera] id=camera_static loc=\"X=", cameraLocationX, ",Y=", cameraLocationY, ",Z=", cameraLocationZ, "\"\n");
			if (cameraTracker != null && !string.IsNullOrEmpty(cameraTrackerCh))
			{
				configFile = string.Concat(configFile, "[camera] id=camera_dynamic loc=\"X=", cameraLocationX, ",Y=", cameraLocationY, ",Z=", cameraLocationZ, "\" tracker_id=", cameraTracker.id, " tracker_ch=", cameraTrackerCh, "\n");
			}
			configFile = string.Concat(configFile, header["scene_node"], CreateListCfg<SceneNode>(ref _sceneNodes),
				header["input"], CreateListCfg<BaseInput>(ref _inputs),
				header["stereo"], "[stereo] eye_swap=", eyeSwap.ToString(), " eye_dist=", eyeDist, "\n",
				header["debug"], "[debug] lag_simulation=", lagSimulation.ToString(), " lag_max_time=", lagMaxTime, " draw_stats=", drawStats.ToString(), "\n",
				header["general"], "[general] swap_sync_policy=", selectedSwapSync.Key, "\n"
				);
			return configFile;
		}

		//Create part of config for List items (IConfigItem)
		string CreateListCfg<T>(ref List<T> list) where T : ConfigItem
		{
			string stringCfg = string.Empty;
			foreach (T item in list)
			{
				stringCfg = string.Concat(stringCfg, item.CreateCfg());
			}
			return stringCfg;
		}


		//Inputs Parser
		public void InputsParse(string line)
		{
			string id = GetRegEx("id").Match(line).Value;
			string address = GetRegAddr("addr").Match(line).Value;
			string _type = GetRegEx("type").Match(line).Value;
			InputDeviceType type = (InputDeviceType)Enum.Parse(typeof(InputDeviceType), _type, true);
			if (type == InputDeviceType.tracker)
			{
				string loc = GetRegComplex("loc").Match(line).Value;
				string locX = GetRegProp("X").Match(loc).Value;
				string locY = GetRegProp("Y").Match(loc).Value;
				string locZ = GetRegProp("Z").Match(loc).Value;
				string rot = GetRegComplex("rot").Match(line).Value;
				string rotP = GetRegProp("P").Match(rot).Value;
				string rotY = GetRegProp("Y").Match(rot).Value;
				string rotR = GetRegProp("R").Match(rot).Value;
				string front = GetRegEx("front").Match(line).Value;
				string right = GetRegEx("right").Match(line).Value;
				string up = GetRegEx("up").Match(line).Value;
				inputs.Add(new TrackerInput(id, address, locX, locY, locZ, rotP, rotY, rotR, front, right, up));
			}
			else
			{
				inputs.Add(new BaseInput(id, type, address));
			}
		}

		//Cluster Node Parser
		public void ClusterNodeParse(string line)
		{
			string id = GetRegEx("id").Match(line).Value;
			string address = GetRegIp("addr").Match(line).Value;
			string screen = GetRegEx("screen").Match(line).Value;
			string viewport = GetRegEx("viewport").Match(line).Value;
			string camera = GetRegEx("camera").Match(line).Value.ToLower();
			string master = GetRegEx("master").Match(line).Value.ToLower();

			//window settings
			string windowed = GetRegEx("windowed").Match(line).Value.ToLower();
			string winX = string.Empty;
			string winY = string.Empty;
			string resX = string.Empty;
			string resY = string.Empty;
			bool isWindowed = false;
			if (windowed == "true")
			{
				isWindowed = true;
			}
			winX = GetRegEx("winX").Match(line).Value;
			winY = GetRegEx("winY").Match(line).Value;
			resX = GetRegEx("resX").Match(line).Value;
			resY = GetRegEx("resY").Match(line).Value;

			//Master node settings
			bool isMaster = false;
			if (master == "true")
			{
				isMaster = true;
				string port_cs = GetRegEx("port_cs").Match(line).Value;
				portCs = port_cs;
				string port_ss = GetRegEx("port_ss").Match(line).Value;
				portSs = port_ss;
			}
			Screen currentScreen = screens.Find(x => x.id == screen);
			Viewport currentViewport = viewports.Find(x => x.id == viewport);
			clusterNodes.Add(new ClusterNode(id, address, currentScreen, currentViewport, camera, isMaster, isWindowed, winX, winY, resX, resY));
		}

		//Scene Node Parser
		public void SceneNodeParse(string line)
		{
			string id = GetRegEx("id").Match(line).Value;
			string loc = GetRegComplex("loc").Match(line).Value;
			string locX = GetRegProp("X").Match(loc).Value;
			string locY = GetRegProp("Y").Match(loc).Value;
			string locZ = GetRegProp("Z").Match(loc).Value;
			string rot = GetRegComplex("rot").Match(line).Value;
			string rotP = GetRegProp("P").Match(rot).Value;
			string rotY = GetRegProp("Y").Match(rot).Value;
			string rotR = GetRegProp("R").Match(rot).Value;

			string _trackerId = GetRegEx("tracker_id").Match(line).Value;
			TrackerInput tracker = (TrackerInput)inputs.Find(x => x.id == _trackerId);
			string _trackerCh = GetRegEx("tracker_ch").Match(line).Value;

			string _parent = GetRegEx("parent").Match(line).Value;
			SceneNode parent = sceneNodes.Find(x => x.id == _parent);

			sceneNodes.Add(new SceneNode(id, locX, locY, locZ, rotP, rotY, rotR, tracker, _trackerCh, parent));
		}

		//Screen Parser
		public void ScreenParse(string line)
		{
			string id = GetRegEx("id").Match(line).Value;
			string loc = GetRegComplex("loc").Match(line).Value;
			string locX = GetRegProp("X").Match(loc).Value;
			string locY = GetRegProp("Y").Match(loc).Value;
			string locZ = GetRegProp("Z").Match(loc).Value;
			string rot = GetRegComplex("rot").Match(line).Value;
			string rotP = GetRegProp("P").Match(rot).Value;
			string rotY = GetRegProp("Y").Match(rot).Value;
			string rotR = GetRegProp("R").Match(rot).Value;
			string size = GetRegComplex("size").Match(line).Value;
			string sizeX = GetRegProp("X").Match(size).Value;
			string sizeY = GetRegProp("Y").Match(size).Value;
			string _parent = GetRegEx("parent").Match(line).Value;
			SceneNode parent = sceneNodes.Find(x => x.id == _parent);
			screens.Add(new Screen(id, locX, locY, locZ, rotP, rotY, rotR, sizeX, sizeY, parent));
		}

		//Viewport Parser
		public void ViewportParse(string line)
		{
			string id = GetRegEx("id").Match(line).Value;
			string x = GetRegEx("x").Match(line).Value;
			string y = GetRegEx("y").Match(line).Value;
			string width = GetRegEx("width").Match(line).Value;
			string height = GetRegEx("height").Match(line).Value;
			string _flip_h = GetRegEx("flip_h").Match(line).Value.ToLower();
			string _flip_v = GetRegEx("flip_v").Match(line).Value.ToLower();
			//string _isWindowed = GetRegEx("windowed").Match(line).Value.ToLower();
			bool flip_h = false;
			bool flip_v = false;
			//bool isWindowed = false;
			if (_flip_h == "true")
			{
				flip_h = true;
			}
			if (_flip_v == "true")
			{
				flip_v = true;
			}
			//if (_isWindowed == "true")
			//{
			//    isWindowed = true;
			//}
			viewports.Add(new Viewport(id, x, y, width, height, flip_h, flip_v));
		}

		//Camera Parser
		public void CameraParse(string line)
		{
			string loc = GetRegComplex("loc").Match(line).Value;
			string locX = GetRegProp("X").Match(loc).Value;
			string locY = GetRegProp("Y").Match(loc).Value;
			string locZ = GetRegProp("Z").Match(loc).Value;
			string cameraTrackerId = GetRegEx("tracker_id").Match(line).Value;
			string trackerCh = GetRegEx("tracker_ch").Match(line).Value;
			cameraLocationX = locX;
			cameraLocationY = locY;
			cameraLocationZ = locZ;
			cameraTracker = (TrackerInput)inputs.Find(x => x.id == cameraTrackerId);
			cameraTrackerCh = trackerCh;
		}

		//General Parser
		public void GeneralParse(string line)
		{
			string swapSync = GetRegEx("swap_sync_policy").Match(line).Value;
			selectedSwapSync = swapSyncPolicy.FirstOrDefault(x => x.Key == swapSync);
		}

		//Stereo Parser
		public void StereoParse(string line)
		{
			string eye_swap = GetRegEx("eye_swap").Match(line).Value;
			string eye_dist = GetRegEx("eye_dist").Match(line).Value;
			eyeSwap = Convert.ToBoolean(eye_swap);
			eyeDist = eye_dist;
		}

		//Debug Parser
		public void DebugParse(string line)
		{
			string lag_simulation = GetRegEx("lag_simulation").Match(line).Value;
			string lag_max_time = GetRegEx("lag_max_time").Match(line).Value;
			string draw_stats = GetRegEx("draw_stats").Match(line).Value;
			lagSimulation = Convert.ToBoolean(lag_simulation);
			lagMaxTime = lag_max_time;
			drawStats = Convert.ToBoolean(draw_stats);
		}

		//Gets double value of parameter from source string
		//private string GetDoubleFromString(string param, string source)
		//{
		//    string value = GetRegProp(param).Match(source).Value;
		//    return ParseToDoubleWithDefault(value);
		//}

		////Converting string value to double and sets 0 if error
		//private string ParseToDoubleWithDefault(string value)
		//{
		//    double parsedValue = 0;
		//    try
		//    {
		//        parsedValue = Convert.ToDouble(value);
		//    }
		//    catch (Exception exception)
		//    {
		//        //add log
		//    }
		//    return parsedValue;
		//}

		//Create Regex for string-num values 
		Regex GetRegEx(string key)
		{
			Regex reg = new Regex("(?<=(?i)(" + key + ")=)(-??\\w*\\.??\\w*)(?=\\s|$)");
			return reg;
		}
		////Create Regex for complex values(location, rotation, size) 
		Regex GetRegComplex(string key)
		{
			Regex reg = new Regex("(?<=(?i)(" + key + ")=\")(.*)(?=\"\\s|$)");
			return reg;
		}

		//Create Regex for IP address values 
		Regex GetRegIp(string key)
		{
			Regex reg = new Regex("(?<=(?i)(" + key + ")=)((([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]))(?=\\s|$)");
			return reg;
		}

		//Create Regex input address values 
		Regex GetRegAddr(string key)
		{
			Regex reg = new Regex("(?<=(?i)(" + key + ")=)(\\w*)@((([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]).){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]))(?=\\s|$)");
			return reg;
		}

		//Create Regex for location, rotation and size properties
		Regex GetRegProp(string key)
		{
			Regex reg = new Regex("(?<=(?i)(" + key + ")=)(-??\\w*.??\\w*)(?=,|\"|$)");
			return reg;
		}

		//Select first items in lists if existed
		public void SelectFirstItems()
		{
			selectedInput = inputs.FirstOrDefault();
			selectedNode = clusterNodes.FirstOrDefault();
			selectedSceneNodeView = sceneNodesView.FirstOrDefault();
			try
			{
				selectedSceneNodeView.isSelected = true;
			}
			catch (NullReferenceException)
			{

			}
			selectedScreen = screens.FirstOrDefault();
			selectedViewport = viewports.FirstOrDefault();

		}

		//Convert list of scene nodes to hierarhical list of scene node views
		public List<SceneNodeView> ConvertSceneNodeList(List<SceneNode> inputList)
		{
			List<SceneNodeView> outputList = new List<SceneNodeView>();
			foreach (SceneNode item in inputList)
			{
				outputList.Add(new SceneNodeView(item));
			}
			foreach (SceneNodeView item in outputList)
			{
				item.children = outputList.FindAll(x => (x.node.parent != null) && (x.node.parent.id == item.node.id));
			}

			return outputList.FindAll(x => x.node.parent == null);
		}

		public void DeleteSceneNode(SceneNodeView item)
		{
			if (item.children != null)
			{
				foreach (SceneNodeView child in item.children.ToList())
				{
					DeleteSceneNode(child);

				}
			}
			if (item.node.parent == null)
			{
				sceneNodesView.Remove(item);

			}
			else
			{
				SceneNodeView parentNode = FindParentNode(item);
				if (parentNode != null)
				{
					parentNode.children.Remove(item);
				}
			}
			AppLogger.Add("Scene node " + item.node.id + " deleted");
			sceneNodes.Remove(item.node);
		}

		public SceneNodeView FindParentNode(SceneNodeView item)
		{
			SceneNodeView parentNode = sceneNodesView.Find(x => x.node == item.node.parent);
			if (parentNode == null)
			{
				foreach (SceneNodeView node in sceneNodesView)
				{
					if (node.FindNodeInChildren(item) != null)
					{
						parentNode = node.FindNodeInChildren(item);
					}
				}
			}
			return parentNode;
		}
	}
}