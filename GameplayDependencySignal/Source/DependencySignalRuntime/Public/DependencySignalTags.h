// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

#define UE_API DEPENDENCYSIGNALRUNTIME_API

namespace DependencySignalTags
{
	UE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root);
} // namespace DependencySignalTags

#undef UE_API