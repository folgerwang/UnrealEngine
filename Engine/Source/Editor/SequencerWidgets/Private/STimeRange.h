// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "ITimeSlider.h"
#include "Misc/QualifiedFrameTime.h"

class STimeRange : public ITimeSlider
{
public:
	SLATE_BEGIN_ARGS(STimeRange)
		: _ShowWorkingRange(true), _ShowViewRange(false), _ShowPlaybackRange(false)
	{}
		/** Whether to show the working range */
		SLATE_ARGUMENT( bool, ShowWorkingRange )
		/** Whether to show the view range */
		SLATE_ARGUMENT( bool, ShowViewRange )
		/** Whether to show the playback range */
		SLATE_ARGUMENT( bool, ShowPlaybackRange )
		/* Content to display inside the time range */
		SLATE_DEFAULT_SLOT( FArguments, CenterContent )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 * 
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController, TSharedRef<INumericTypeInterface<double>> NumericTypeInterface );

protected:
	double GetSpinboxDelta() const;
	
protected:

	double PlayStartTime() const;
	double PlayEndTime() const;

	TOptional<double> MinPlayStartTime() const;
	TOptional<double> MaxPlayStartTime() const;
	TOptional<double> MinPlayEndTime() const;
	TOptional<double> MaxPlayEndTime() const;

	void OnPlayStartTimeCommitted(double NewValue, ETextCommit::Type InTextCommit);
	void OnPlayEndTimeCommitted(double NewValue, ETextCommit::Type InTextCommit);

	void OnPlayStartTimeChanged(double NewValue);
	void OnPlayEndTimeChanged(double NewValue);

protected:

	double ViewStartTime() const;
	double ViewEndTime() const;
	
	TOptional<double> MaxViewStartTime() const;
	TOptional<double> MinViewEndTime() const;

	void OnViewStartTimeCommitted(double NewValue, ETextCommit::Type InTextCommit);
	void OnViewEndTimeCommitted(double NewValue, ETextCommit::Type InTextCommit);

	void OnViewStartTimeChanged(double NewValue);
	void OnViewEndTimeChanged(double NewValue);

protected:

	double WorkingStartTime() const;
	double WorkingEndTime() const;

	TOptional<double> MaxWorkingStartTime() const;
	TOptional<double> MinWorkingEndTime() const;

	void OnWorkingStartTimeCommitted(double NewValue, ETextCommit::Type InTextCommit);
	void OnWorkingEndTimeCommitted(double NewValue, ETextCommit::Type InTextCommit);

	void OnWorkingStartTimeChanged(double NewValue);
	void OnWorkingEndTimeChanged(double NewValue);

private:
	TSharedPtr<ITimeSliderController> TimeSliderController;
};
