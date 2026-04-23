// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Providers/DependencySignalProviderBase.h"
#include "DependencySignalProvider_Controller.generated.h"

UCLASS()
class UDependencySignalProvider_Controller : public UDependencySignalProviderBase
{
	GENERATED_BODY()

protected:
	virtual bool Evaluate_Implementation(const FGameplayTag& Tag) const override;
	virtual UObject* Provide_Implementation(const FGameplayTag& Tag, UObject* SourceObject) const override;
};