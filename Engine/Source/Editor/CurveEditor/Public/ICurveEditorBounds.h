// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

struct ICurveEditorBounds
{
	virtual ~ICurveEditorBounds(){}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const = 0;
	virtual void GetOutputBounds(double& OutMin, double& OutMax) const = 0;

	virtual void SetInputBounds(double InMin, double InMax) = 0;
	virtual void SetOutputBounds(double InMin, double InMax) = 0;
};


struct FStaticCurveEditorBounds : ICurveEditorBounds
{
	double InputMin, InputMax;
	double OutputMin, OutputMax;

	FStaticCurveEditorBounds()
	{
		InputMin = 0;
		InputMax = 1;

		OutputMin = 0;
		OutputMax = 1;
	}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const override final
	{
		OutMin = InputMin;
		OutMax = InputMax;
	}

	virtual void GetOutputBounds(double& OutMin, double& OutMax) const override final
	{
		OutMin = OutputMin;
		OutMax = OutputMax;
	}

	virtual void SetInputBounds(double InMin, double InMax) override final
	{
		InputMin = InMin;
		InputMax = InMax;
	}

	virtual void SetOutputBounds(double InMin, double InMax) override final
	{
		OutputMin = InMin;
		OutputMax = InMax;
	}
};