#pragma once

#include "Helpers/DependencySignalTestHelpers.h"

#include "DependencySignalTags.h"

namespace DependencySignal::Tests
{
	FNativeGameplayTags FNativeGameplayTags::Tags;

	FString FormatGameplayTag(const FGameplayTag& Tag)
	{
		return Tag.ToString()
			.Replace(*DependencySignalTags::Root.GetTag().ToString().Append("."), TEXT(""))
			.Replace(TEXT("."), TEXT("_"));
	}

	FString FTestDependency::GetTagString() const
	{
		return FormatGameplayTag(Tag);
	}

	FString FTestDependencyConfig::GetRegistrationTagsString() const
	{
		FString Str;
		FString Suffix = TEXT(", ");

		for (const FGameplayTag& Tag : RegistrationTags)
		{
			Str += FormatGameplayTag(Tag);
			Str += Suffix;
		}

		Str.RemoveFromEnd(Suffix);
		return Str;
	}

	FString FTestDependencyConfig::GetValidationsSequenceString() const
	{
		FString Str;
		FString Suffix = TEXT(" -> ");

		for (const FTestDependency& Queue : ValidationQueue)
		{
			Str += Queue.GetTagString();
			Str += Suffix;
		}

		Str.RemoveFromEnd(Suffix);
		return Str;
	}

	void FTestDependencyConfig::Reset()
	{
		RegistrationTags = FGameplayTagContainer::EmptyContainer;
		ValidationQueue.Empty();
		bFinalValidationResult.Reset();
	}

	bool FTestDependencyConfig::IsValid() const
	{
		return !RegistrationTags.IsEmpty() && !bFinalValidationResult.IsSet();
	}
} // namespace DependencySignal::Tests