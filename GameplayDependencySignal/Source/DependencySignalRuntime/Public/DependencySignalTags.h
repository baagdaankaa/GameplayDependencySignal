// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

/**
 * Namespace containing all predefined gameplay tags for the dependency signal system.
 */
namespace DependencySignalTags
{
	UE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);

	UE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Controller);
	UE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Controller_Pawn);
	UE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Controller_PlayerState);
} // namespace DependencySignalTags

#undef UE_API