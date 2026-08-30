#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "DependencySignalTestHelpers.generated.h"

namespace DependencySignal::Tests
{
	struct FNativeGameplayTags : public FGameplayTagNativeAdder
	{
		virtual ~FNativeGameplayTags()
		{
		}

		FGameplayTag TestRoot;
		FGameplayTag Test_A;
		FGameplayTag Test_A_B;
		FGameplayTag Test_B;
		FGameplayTag Test_C;

		FORCEINLINE static const FNativeGameplayTags& Get()
		{
			return Tags;
		}

	protected:
		virtual void AddTags() override
		{
			UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

			TestRoot = Manager.AddNativeGameplayTag(TEXT("DependencySignal.Test"));
			Test_A = Manager.AddNativeGameplayTag(TEXT("DependencySignal.Test.TagA"));
			Test_A_B = Manager.AddNativeGameplayTag(TEXT("DependencySignal.Test.TagA.TagB"));
			Test_B = Manager.AddNativeGameplayTag(TEXT("DependencySignal.Test.TagB"));
			Test_C = Manager.AddNativeGameplayTag(TEXT("DependencySignal.Test.TagC"));
		}

	private:
		static FNativeGameplayTags Tags;
	};

	struct FTestDependency
	{
		FString GetTagString() const;

		FGameplayTag Tag;
		bool IsValid = false;

		TWeakObjectPtr<UObject> SourceObject;
	};

	struct FTestVerification
	{
		bool IsValid = false;
		TWeakObjectPtr<UObject> SourceObject;
	};

	struct FTestDependencyConfigBase
	{
		FGameplayTagContainer RegistrationTags;

		// used before dependency registration
		TArray<FTestDependency> PreValidationQueue;

		// used after dependency registration
		TArray<FTestDependency> ValidationQueue;
	};

	FString GetRegistrationTagsString(const FGameplayTagContainer& InTags);
	FString GetValidationsSequenceString(const TArray<FTestDependency>& InValidationQueue);

} // namespace DependencySignal::Tests

UCLASS()
class UDepTestObject : public UObject
{
	GENERATED_BODY()
};
