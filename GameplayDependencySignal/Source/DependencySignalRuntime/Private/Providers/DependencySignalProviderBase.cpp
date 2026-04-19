// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Providers/DependencySignalProviderBase.h"

bool UDependencySignalProviderBase::Evaluate_Implementation(const FGameplayTag& Tag) const
{
	unimplemented();
	return false;
}

UObject* UDependencySignalProviderBase::Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const
{
	unimplemented();
	return nullptr;
}