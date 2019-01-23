// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "EngineUtils.h"

#include "DisplayClusterGlobals.h"
#include "DisplayClusterStrings.h"

#include "Cluster/IPDisplayClusterClusterManager.h"
#include "Config/IPDisplayClusterConfigManager.h"
#include "Config/DisplayClusterConfigTypes.h"
#include "Misc/DisplayClusterTypesConverter.h"

#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"


class AActor;


namespace DisplayClusterHelpers
{
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Common String helpers
	//////////////////////////////////////////////////////////////////////////////////////////////
	namespace str
	{
		static constexpr auto StrFalse = TEXT("false");
		static constexpr auto StrTrue  = TEXT("true");

		static inline auto BoolToStr(bool bVal)
		{
			return (bVal ? StrTrue : StrFalse);
		}

		template<typename T>
		static FString ArrayToStr(const TArray<T>& Data)
		{
			static const auto Quotes = TEXT("\"");
			FString tmp = Quotes;
			const uint32 Count = Data.Num();

			for (uint32 i = 0; i < Count; ++i)
			{
				tmp += FDisplayClusterTypesConverter::ToString(Data[i]);
				if (i < (Count - 1))
				{
					tmp += DisplayClusterStrings::strArrayValSeparator;
				}
			}

			tmp += Quotes;

			return tmp;
		}

		static void DustCommandLineValue(FString& val, bool bTrimQuotes = true)
		{
			val.RemoveFromStart(DisplayClusterStrings::strKeyValSeparator);
			
			if (bTrimQuotes)
			{
				val = val.TrimQuotes();
			}

			val.TrimStartAndEndInline();
		}

		template<typename T>
		static bool ExtractCommandLineValue(const FString& line, const FString& argName, T& argVal)
		{
			FString tmp;

			// This is fix for quoted arguments. Normally this should be performed in the FParse::Value
			// but we need to make it work right now. So use this workaround;
			const FString FixedArgName = argName + DisplayClusterStrings::strKeyValSeparator;

			if (FParse::Value(*line, *FixedArgName, tmp, false))
			{
				DustCommandLineValue(tmp, false);
				argVal = FDisplayClusterTypesConverter::FromString<T>(tmp);
				return true;
			}

			return false;
		}

		template<typename T>
		static bool ExtractCommandLineArray(const FString& line, const FString& argName, TArray<T>& argVal)
		{
			FString tmp;
			if (ExtractCommandLineValue(line, argName, tmp))
			{
				DustCommandLineValue(tmp, true);
				tmp.ParseIntoArray(argVal, DisplayClusterStrings::strArrayValSeparator, true);
				return true;
			}

			return false;
		}

		static bool ExtractParam(const FString& source, const FString& param, FString& value, bool bTrimQuotes = true)
		{
			// Extract device address
			if (!FParse::Value(*source, *param, value, false))
			{
				return false;
			}

			DisplayClusterHelpers::str::DustCommandLineValue(value, bTrimQuotes);

			return true;
		}

#if 0
		bool GetPair(FString& line, FString& pair)
		{
			if (line.IsEmpty())
				return false;

			if (line.Split(FString(" "), &pair, &line) == false)
			{
				pair = line;
				line.Empty();
				return true;
			}

			line = line.Trim().TrimTrailing();
			pair = pair.Trim().TrimTrailing();

			return true;
		}

		bool GetKeyVal(FString& line, FString& key, FString& val)
		{
			FString pair;
			if (GetPair(line, pair) == false)
				return false;

			if (pair.Split(FString(DisplayClusterStrings::cfg::spec::KeyValSeparator), &key, &val) == false)
				return false;

			key = key.Trim().TrimTrailing();
			val = val.Trim().TrimTrailing();

			return true;
		}
#endif
	};


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Network helpers
	//////////////////////////////////////////////////////////////////////////////////////////////
	namespace net
	{
		static bool GenIPv4Endpoint(const FString& addr, const int32 port, FIPv4Endpoint& ep)
		{
			FIPv4Address ipAddr;
			if (!FIPv4Address::Parse(addr, ipAddr))
				return false;

			ep = FIPv4Endpoint(ipAddr, port);
			return true;
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Array helpers
	//////////////////////////////////////////////////////////////////////////////////////////////	struct str
	namespace arrays
	{
		// Max element in array
		template<typename T>
		T max(const T* data, int size)
		{
			T result = data[0];
			for (int i = 1; i < size; i++)
				if (result < data[i])
					result = data[i];
			return result;
		}

		// Max element's index in array
		template<typename T>
		size_t max_idx(const T* data, int size)
		{
			size_t idx = 0;
			T result = data[0];
			for (int i = 1; i < size; i++)
				if (result < data[i])
				{
					result = data[i];
					idx = i;
				}
			return idx;
		}

		// Min element in array
		template<typename T>
		T min(const T* data, int size)
		{
			T result = data[0];
			for (int i = 1; i < size; i++)
				if (result > data[i])
					result = data[i];
			return result;
		}

		// Min element's index in array
		template<typename T>
		size_t min_idx(const T* data, int size)
		{
			size_t idx = 0;
			T result = data[0];
			for (int i = 1; i < size; i++)
				if (result > data[i])
				{
					result = data[i];
					idx = i;
				}
			return idx;
		}

		// Helper for array size
		template <typename T, size_t n>
		constexpr size_t array_size(const T(&)[n])
		{
			return n;
		}
	}


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Game helpers
	//////////////////////////////////////////////////////////////////////////////////////////////
	namespace game
	{
		template<typename T>
		static void FindAllActors(UWorld* World, TArray<T*>& Out)
		{
			for (TActorIterator<AActor> It(World, T::StaticClass()); It; ++It)
			{
				T* Actor = Cast<T>(*It);
				if (Actor && !Actor->IsPendingKill())
				{
					Out.Add(Actor);
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// Config helpers
	//////////////////////////////////////////////////////////////////////////////////////////////
	namespace config
	{
		static bool GetLocalClusterNode(FDisplayClusterConfigClusterNode& LocalClusterNode)
		{
			if (!GDisplayCluster || (GDisplayCluster->GetOperationMode() == EDisplayClusterOperationMode::Disabled))
			{
				return false;
			}

			const IPDisplayClusterClusterManager* const ClusterMgr = GDisplayCluster->GetPrivateClusterMgr();
			if (!ClusterMgr)
			{
				return false;
			}

			const FString LocalNodeId = ClusterMgr->GetNodeId();
			const IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
			if (!ConfigMgr)
			{
				return false;
			}

			return ConfigMgr->GetClusterNode(LocalNodeId, LocalClusterNode);
		}

		static bool GetLocalWindow(FDisplayClusterConfigWindow& LocalWindow)
		{
			FDisplayClusterConfigClusterNode LocalClusterNode;
			if (!GetLocalClusterNode(LocalClusterNode))
			{
				return false;
			}

			const IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
			if (!ConfigMgr)
			{
				return false;
			}

			return ConfigMgr->GetWindow(LocalClusterNode.WindowId, LocalWindow);
		}

		static TArray<FDisplayClusterConfigViewport> GetLocalViewports()
		{
			TArray<FDisplayClusterConfigViewport> LocalViewports;

			FDisplayClusterConfigWindow LocalWindow;
			if (!GetLocalWindow(LocalWindow))
			{
				return LocalViewports;
			}

			const IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
			if (!ConfigMgr)
			{
				return LocalViewports;
			}

			LocalViewports = ConfigMgr->GetViewports().FilterByPredicate([&LocalWindow](const FDisplayClusterConfigViewport& ItemViewport)
			{
				return LocalWindow.ViewportIds.ContainsByPredicate([ItemViewport](const FString& ItemId)
				{
					return ItemViewport.Id.Compare(ItemId, ESearchCase::IgnoreCase) == 0;
				});
			});

			return LocalViewports;
		}

		static TArray<FDisplayClusterConfigScreen> GetLocalScreens()
		{
			TArray<FDisplayClusterConfigScreen>   LocalScreens;
			TArray<FDisplayClusterConfigViewport> LocalViewports = GetLocalViewports();

			const IPDisplayClusterConfigManager* const ConfigMgr = GDisplayCluster->GetPrivateConfigMgr();
			if (!ConfigMgr)
			{
				return LocalScreens;
			}

			LocalScreens = ConfigMgr->GetScreens().FilterByPredicate([&LocalViewports](const FDisplayClusterConfigScreen& ItemScreen)
			{
				return LocalViewports.ContainsByPredicate([&ItemScreen](const FDisplayClusterConfigViewport& Viewport)
				{
					return ItemScreen.Id.Compare(Viewport.ScreenId, ESearchCase::IgnoreCase) == 0;
				});
			});

			return LocalScreens;
		}

		static bool IsLocalScreen(const FString& ScreenId)
		{
			return nullptr != GetLocalScreens().FindByPredicate([ScreenId](const FDisplayClusterConfigScreen& Screen)
			{
				return Screen.Id == ScreenId;
			});
		}
	}
};
