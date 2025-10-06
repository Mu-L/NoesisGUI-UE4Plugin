////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#if WITH_EDITOR

// RHI includes
#include "RHI.h"

// Engine includes
#include "Engine/World.h"

#include "NoesisSDK.h"

struct FNoesisThumbnailRenderer
{
	bool IsInitialized() const;

	void Initialize(Noesis::FrameworkElement* Content);

	void Render(UWorld* World, FIntRect ViewportRect, const FTextureRHIRef& BackBuffer) const;

	void Destroy();

	~FNoesisThumbnailRenderer();

private:
	Noesis::Ptr<Noesis::IView> View;
};


#endif
