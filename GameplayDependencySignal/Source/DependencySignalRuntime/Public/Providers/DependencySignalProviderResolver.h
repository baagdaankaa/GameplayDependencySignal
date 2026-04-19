// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

class UDependencySignalProviderBase;

struct FGameplayTag;

#define UE_API DEPENDENCYSIGNALRUNTIME_API

class UE_API FDependencySignalProviderResolver
{
public:
	FDependencySignalProviderResolver(const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses);
	FDependencySignalProviderResolver(
		const TArray<TSubclassOf<UDependencySignalProviderBase>>& ProviderClasses, UWorld* InWorld);

	~FDependencySignalProviderResolver();

	void SetWorld(UWorld* InWorld);

	UObject* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

	template <class T = UObject>
	T* GetDependency(const FGameplayTag& Tag, UObject* SourceObject = nullptr) const;

private:
	/** Weak reference to the world context used for dependency resolution. */
	TWeakObjectPtr<UWorld> World;

	TArray<TWeakObjectPtr<UDependencySignalProviderBase>> Providers;
};

template <class T>
T* FDependencySignalProviderResolver::GetDependency(const FGameplayTag& Tag, UObject* SourceObject) const
{
	return Cast<T>(GetDependency(Tag, SourceObject));
}

#undef UE_API
