// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalRuntime.h"

DEFINE_LOG_CATEGORY(LogDependencySignal)

#define LOCTEXT_NAMESPACE "FDependencySignalRuntimeModule"

void FDependencySignalRuntimeModule::StartupModule()
{
}

void FDependencySignalRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDependencySignalRuntimeModule, DependencySignalRuntime)