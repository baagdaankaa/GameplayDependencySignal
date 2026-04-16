// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDependencySignal, Log, All);

#define DS_UE_LOG(LogVerbosity, Format, ...) \
	UE_LOG(LogDependencySignal, LogVerbosity, TEXT("[%hs] " Format), __FUNCTION__, ##__VA_ARGS__)

class FDependencySignalRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
