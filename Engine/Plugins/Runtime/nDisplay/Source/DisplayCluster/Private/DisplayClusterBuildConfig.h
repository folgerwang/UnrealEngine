// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// Enables automatic ID resolve by host address. This feature
// can be used only with single DisplayCluster instance per PC.
#define DISPLAY_CLUSTER_USE_AUTOMATIC_NODE_ID_RESOLVE

// Allows to run game with stereo in easy way. You don't have
// to have a config file and a lot of command line arguments.
// Simple argument list would be:
// -dc_cluster -dc_cfg=? -quad_buffer_stereo -opengl4
#define DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
