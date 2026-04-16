// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Settings/DependencySignalSettings.h"

#include "DependencySignalTags.h"

UDependencySignalSettings::UDependencySignalSettings()
{
	RootTags.AddTagFast(DependencySignalTags::Root);
}

const FGameplayTagContainer& UDependencySignalSettings::GetRootTags() const
{
	return RootTags;
}