// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GauntletModule.h"
#include "GauntletTestController.generated.h"


/**
 *	Base class for games to implement test controllers that use the Gauntlet native
 *	framework. This is a very thin class that is created automatically based on 
 *	command line params (-gauntlet=MyControllerName) and provides easily overridden
 *	functions that represent state changes and ticking
 *
 *	In essence your derived class should implement logic that starts and monitors
 *	a test, then calls EndTest(Result) when the desired criteria are met (or not!)
 */
UCLASS()
class GAUNTLET_API UGauntletTestController : public UObject
{
	GENERATED_BODY()

	friend class FGauntletModule;

public:

	/** Default constructur */
	UGauntletTestController(const FObjectInitializer& ObjectInitializer);
	virtual ~UGauntletTestController();

	// Overridable delegates for some of the most useful test points

	/**
	 *	Called when the controller is first initialized
	 */
	virtual void	OnInit() {}

	/**
	 *	Called prior to a map change
	 */
	virtual void	OnPreMapChange() {}

	/**
	 *	Called after a map change. GetCurrentMap() will now return the new map
	 */
	virtual void	OnPostMapChange(UWorld* World) {}

	/**
	 *	Called periodically to let the controller check and control state
	 */
	virtual void	OnTick(float TimeDelta) {}

	/**
	 *	Called when a state change is applied to the module. States are game-driven.
	 *	GetCurrentState() == OldState until this function returns
	 */
	virtual void	OnStateChange(FName OldState, FName NewState) {}

protected:

	/**
	 *	Returns the current state applied to Gauntlet
	 */
	FName			GetCurrentState() const;

	/**
	 *	Return the time since OnStateChange was called with the current state
	 */
	double			GetTimeInCurrentState() const;

	/**
	 *	Return the name of the current persistent map
	 */
	FString			GetCurrentMap() const;

	/**
	 *	Called to end the current test with the provided exit code. This will cause the
	 *	app to exit with the provided return code.
	 */
	void			EndTest(int32 ExitCode = 0);

	/**
	 * Returns the gauntlet module running this test
	 */
	FGauntletModule* GetGauntlet();

private:

	FGauntletModule*  ParentModule;
};




