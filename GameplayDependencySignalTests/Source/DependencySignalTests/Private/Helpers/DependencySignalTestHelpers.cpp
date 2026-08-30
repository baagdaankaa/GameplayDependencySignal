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

	FString GetRegistrationTagsString(const FGameplayTagContainer& InTags)
	{
		FString Str;
		FString Suffix = TEXT(", ");

		for (const FGameplayTag& Tag : InTags)
		{
			Str += FormatGameplayTag(Tag);
			Str += Suffix;
		}

		Str.RemoveFromEnd(Suffix);
		return Str;
	}

	FString GetValidationsSequenceString(const TArray<FTestDependency>& InValidationQueue)
	{
		FString Str;
		FString Suffix = TEXT(" -> ");

		for (const FTestDependency& Queue : InValidationQueue)
		{
			Str += Queue.GetTagString();
			Str += Suffix;
		}

		Str.RemoveFromEnd(Suffix);
		return Str;
	}
} // namespace DependencySignal::Tests