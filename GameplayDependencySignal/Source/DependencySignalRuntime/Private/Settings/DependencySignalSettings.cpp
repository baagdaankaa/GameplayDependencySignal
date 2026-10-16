// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Settings/DependencySignalSettings.h"

#include "DependencySignalTags.h"
#include "Engine/World.h"

UDependencySignalSettings::UDependencySignalSettings()
{
	RootTags.AddTagFast(DependencySignalTags::Root);
}

const FGameplayTagContainer& UDependencySignalSettings::GetRootTags() const
{
	return RootTags;
}

bool UDependencySignalSettings::IsMapProhibited(const UWorld* World) const
{
	if (!World || ProhibitedMaps.IsEmpty())
	{
		return false;
	}

	const FString PackageName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());

	for (const TSoftObjectPtr<UWorld>& Map : ProhibitedMaps)
	{
		if (!Map.IsNull() && Map.GetLongPackageName() == PackageName)
		{
			return true;
		}
	}

	return false;
}