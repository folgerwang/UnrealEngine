// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Windows;
using System.Windows.Data;

using nDisplayLauncher.Log;
using nDisplayLauncher.Cluster;
using nDisplayLauncher.Cluster.Events;
using nDisplayLauncher.Settings;


namespace nDisplayLauncher
{
	public partial class MainWindow
	{
		ObservableCollection<ClusterEvent> ClusterEvents = new ObservableCollection<ClusterEvent>();

		private void InitializeEvents()
		{
			List<string> SavedEvents = RegistrySaver.ReadStringsFromRegistry(RegistrySaver.RegCategoryClusterEvents);
			foreach (string SavedEventStr in SavedEvents)
			{
				ClusterEvent RestoredEvent = new ClusterEvent();
				RestoredEvent.DeserializeFromString(SavedEventStr);
				ClusterEvents.Add(RestoredEvent);
			}

			ctrlListClusterEvents.ItemsSource = ClusterEvents;

			CollectionView view = (CollectionView)CollectionViewSource.GetDefaultView(ctrlListClusterEvents.ItemsSource);
			view.SortDescriptions.Add(new SortDescription("Category", ListSortDirection.Ascending));
			view.SortDescriptions.Add(new SortDescription("Type", ListSortDirection.Ascending));
			view.SortDescriptions.Add(new SortDescription("Name", ListSortDirection.Ascending));
		}

		private void ctrlBtnEventNew_Click(object sender, RoutedEventArgs e)
		{
			ClusterEventWindow Wnd = new ClusterEventWindow();
			Wnd.WindowStartupLocation = WindowStartupLocation.CenterOwner;
			Wnd.Owner = this;

			Wnd.AvailableCategories = GetAvailableCategories();
			Wnd.AvailableTypes = GetAvailableTypes();
			Wnd.AvailableNames = GetAvailableNames();

			bool? RetVal = Wnd.ShowDialog();
			if (RetVal.HasValue && RetVal == true)
			{
				Dictionary<string, string> ArgMap = new Dictionary<string, string>();
				ClusterEvent NewEvt = new ClusterEvent(Wnd.SelectedCategory, Wnd.SelectedType, Wnd.SelectedName, Wnd.GetArgDictionary());
				NewEvt.RebuildJsonStringForGui();
				ClusterEvents.Add(NewEvt);

				RegistrySaver.AddRegistryValue(RegistrySaver.RegCategoryClusterEvents, NewEvt.SerializeToString());

				AppLogger.Log("New cluster event stored: " + NewEvt.ToString());
			}
			else
			{
				// Nothing to do
			}
		}

		private void ctrlBtnEventModify_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlListClusterEvents.SelectedItems.Count > 0)
			{
				List<ClusterEvent> ItemsToModify = new List<ClusterEvent>();

				foreach (ClusterEvent Evt in ctrlListClusterEvents.SelectedItems)
				{
					ItemsToModify.Add(Evt);
				}

				foreach (ClusterEvent Evt in ItemsToModify)
				{
					int Idx = ClusterEvents.IndexOf(Evt);
					if (Idx >= 0)
					{
						ClusterEventWindow Wnd = new ClusterEventWindow();
						Wnd.WindowStartupLocation = WindowStartupLocation.CenterOwner;
						Wnd.Owner = this;

						Wnd.AvailableCategories = GetAvailableCategories();
						Wnd.AvailableTypes = GetAvailableTypes();
						Wnd.AvailableNames = GetAvailableNames();
						Wnd.SelectedCategory = Evt.Category;
						Wnd.SelectedType = Evt.Type;
						Wnd.SelectedName = Evt.Name;
						Wnd.SetArgDictionary(Evt.Parameters);

						bool? RetVal = Wnd.ShowDialog();
						if (RetVal.HasValue && RetVal == true)
						{
							RegistrySaver.RemoveRegistryValue(RegistrySaver.RegCategoryClusterEvents, ClusterEvents[Idx].SerializeToString());
							ClusterEvents[Idx] = new ClusterEvent(Wnd.SelectedCategory, Wnd.SelectedType, Wnd.SelectedName, Wnd.GetArgDictionary());
							RegistrySaver.AddRegistryValue(RegistrySaver.RegCategoryClusterEvents, ClusterEvents[Idx].SerializeToString());
						}
					}
				}

				UpdateJsonInfo();
			}
		}

		private void ctrlBtnEventDelete_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlListClusterEvents.SelectedItems.Count > 0)
			{
				List<ClusterEvent> ItemsToDelete = new List<ClusterEvent>();

				foreach (ClusterEvent Evt in ctrlListClusterEvents.SelectedItems)
				{
					ItemsToDelete.Add(Evt);
				}

				foreach (ClusterEvent Evt in ItemsToDelete)
				{
					ClusterEvents.Remove(Evt);
					RegistrySaver.RemoveRegistryValue(RegistrySaver.RegCategoryClusterEvents, Evt.SerializeToString());
				}
			}
		}

		private void ctrlBtnEventSend_Click(object sender, RoutedEventArgs e)
		{
			if (ctrlListClusterEvents.SelectedItems.Count <= 0)
			{
				AppLogger.Log("No event selected");
				return;
			}

			TheLauncher.ProcessCommand(Launcher.ClusterCommandType.SendEvent, new object[] { ctrlListClusterEvents.SelectedItems.Cast<ClusterEvent>().ToList() });
		}

		private void UpdateJsonInfo()
		{
			foreach (ClusterEvent Evt in ClusterEvents)
			{
				Evt.RebuildJsonStringForGui();
			}
		}

		private HashSet<string> GetAvailableCategories()
		{
			HashSet<string> Data = new HashSet<string>();
			foreach(ClusterEvent Evt in ClusterEvents)
			{
				Data.Add(Evt.Category);
			}

			return Data;
		}

		private HashSet<string> GetAvailableTypes()
		{
			HashSet<string> Data = new HashSet<string>();
			foreach (ClusterEvent Evt in ClusterEvents)
			{
				Data.Add(Evt.Type);
			}

			return Data;
		}

		private HashSet<string> GetAvailableNames()
		{
			HashSet<string> Data = new HashSet<string>();
			foreach (ClusterEvent Evt in ClusterEvents)
			{
				Data.Add(Evt.Name);
			}

			return Data;
		}
	}
}
