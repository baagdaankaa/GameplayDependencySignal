// Copyright Bohdan Sofronii. All Rights Reserved.

#include "Providers/DependencySignalProviderResolver.h"

#include "Providers/DependencySignalProviderBase.h"

FDependencySignalProviderResolver::FDependencySignalProviderResolver(
	const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses)
	: FDependencySignalProviderResolver(ProviderClasses, nullptr)
{
}

FDependencySignalProviderResolver::FDependencySignalProviderResolver(
	const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses, UWorld* InWorld)
	: World(InWorld)
{
	Providers.Reserve(ProviderClasses.Num());
	Algo::TransformIf(
		ProviderClasses, Providers,
		[](const TSubclassOf<UDependencySignalProviderBase>& ProviderClass) {
			return ProviderClass != nullptr;
		},
		[](const TSubclassOf<UDependencySignalProviderBase>& ProviderClass) {
			return ProviderClass->GetDefaultObject<UDependencySignalProviderBase>();
		});
}

FDependencySignalProviderResolver::~FDependencySignalProviderResolver()
{
	World.Reset();
	Providers.Empty();
}

void FDependencySignalProviderResolver::SetWorld(UWorld* InWorld)
{
	World = InWorld;
	check(World.IsValid());
}

UObject* FDependencySignalProviderResolver::GetDependency(const FGameplayTag& Tag, UObject* SourceObject) const
{
	// Use UWorld if SourceObject is invalid. To try getting a valid dependency.
	SourceObject = IsValid(SourceObject) ? SourceObject : World.Get();
	check(IsValid(SourceObject));

	for (const TWeakObjectPtr<UDependencySignalProviderBase>& Provider : Providers)
	{
		if (Provider.IsValid() && Provider->Evaluate(Tag))
		{
			return Provider->Provide(Tag, SourceObject);
		}
	}

	return nullptr;
}