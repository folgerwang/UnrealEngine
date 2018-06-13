// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace nDisplayLauncher.Config
{
	public class SceneNodeView
	{
		public List<SceneNodeView> children { get; set; }

		public SceneNode node { get; set; }

		public bool isSelected { get; set; }
		public bool isExpanded { get; set; }

		public SceneNodeView()
		{
			isSelected = false;
			isExpanded = false;
			children = new List<SceneNodeView>();
			node = new SceneNode();
		}
		public SceneNodeView(SceneNode item)
		{
			isSelected = false;
			isExpanded = false;
			children = new List<SceneNodeView>();
			node = item;
		}

		//Return child Scene node View if it equal argument scene node 
		public SceneNodeView FindNodeInChildren(SceneNodeView item)
		{
			SceneNodeView output = null;
			if (this.node == item.node.parent)
			{
				output = this;
			}
			else
			{
				foreach (SceneNodeView child in this.children.ToList())
				{
					if (child.node == item.node.parent)
					{
						output = child;
					}
					else
					{
						if (child.children.Count > 0)
						{
							output = child.FindNodeInChildren(item);
						}
					}
				}
			}
			return output;
		}
	}
}
