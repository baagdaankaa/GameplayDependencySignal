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

	FString GetValidationsSequenceString(
		const TArray<FTestDependency>& InPreQueue, const TArray<FTestDependency>& InPostQueue)
	{
		FString Str;
		FString Suffix = TEXT(" -> ");

		const auto& FillString = [&](const TArray<FTestDependency>& InQueue, const FString& Prefix) {
			if (!InQueue.IsEmpty())
			{
				Str += Prefix + " ";
			}

			for (const FTestDependency& Queue : InQueue)
			{
				Str += Queue.GetTagString();
				Str += Suffix;
			}
		};

		FillString(InPreQueue, "Pre");
		FillString(InPostQueue, "Post");

		Str.RemoveFromEnd(Suffix);
		return Str;
	}
} // namespace DependencySignal::Tests