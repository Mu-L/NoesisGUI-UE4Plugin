////////////////////////////////////////////////////////////////////////////////////////////////////
// NoesisGUI - http://www.noesisengine.com
// Copyright (c) 2013 Noesis Technologies S.L. All Rights Reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// Core includes
#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"
#include "Modules/ModuleInterface.h"
#include "Stats/Stats.h"
#if UE_VERSION_OLDER_THAN(5, 6, 0)
#include "Stats/Stats2.h"
#endif
#include "HAL/Platform.h"

// Noesis includes
#include "NsCore/Log.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNoesis, VeryVerbose, All);
DECLARE_LOG_CATEGORY_EXTERN(LogNoesisEngine, Error, All);

DECLARE_STATS_GROUP(TEXT("Noesis"), STATGROUP_Noesis, STATCAT_Advanced);

#define NOESISGUI_MODULE_NAME "NoesisRuntime"

NOESISRUNTIME_API extern void* NoesisAllocationCallbackUserData;
NOESISRUNTIME_API void* NoesisAlloc(void* UserData, size_t Size);
NOESISRUNTIME_API void* NoesisRealloc(void* UserData, void* Ptr, size_t Size);
NOESISRUNTIME_API void NoesisDealloc(void* UserData, void* Ptr);
NOESISRUNTIME_API size_t NoesisAllocSize(void* UserData, void* Ptr);

#if NO_LOGGING == 0
	#if PLATFORM_TCHAR_IS_UTF8CHAR
		#define NS_LOG(...) UE_LOG(LogNoesis, Warning, TEXT("%s"), *TAnsiStringBuilder<256>().Appendf(__VA_ARGS__))
	#else
		#define NS_LOG(...) UE_LOG(LogNoesis, Warning, TEXT("%S"), *TAnsiStringBuilder<256>().Appendf(__VA_ARGS__))
	#endif
#else
	#define NS_LOG(...)
#endif

DECLARE_MULTICAST_DELEGATE(FNoesisKeyboardRequest);

class NOESISRUNTIME_API INoesisRuntimeModuleInterface : public IModuleInterface
{
public:
	static INoesisRuntimeModuleInterface& Get();

	virtual void RegisterFont(const class UFontFace* Font) = 0;
	virtual void OnXamlChanged(class UNoesisXaml* Xaml) = 0;
	virtual void OnTextureChanged(class UTexture2D* Texture) = 0;
	virtual FNoesisKeyboardRequest& OnKeyboardRequested() = 0;
	virtual FNoesisKeyboardRequest& OnKeyboardDismissed() = 0;
};
