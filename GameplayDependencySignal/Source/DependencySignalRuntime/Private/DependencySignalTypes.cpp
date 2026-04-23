// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalTypes.h"

#include "DependencySignalDispatcher.h"

FDependencySignalRequest& FDependencySignalRequest::AddTag(const FGameplayTag& Tag)
{
	DependencyTags.AddTag(Tag);
	return *this;
}

FDependencySignalRequest& FDependencySignalRequest::AddTags(const FGameplayTagContainer& TagContainer)
{
	DependencyTags.AppendTags(TagContainer);
	return *this;
}

FDependencySignalRequest& FDependencySignalRequest::AddSource(const UObject* InSourceObject)
{
	SourceObject = TWeakObjectPtr(InSourceObject);
	return *this;
}

bool FDependencySignalListenerHandle::Unregister()
{
	if (Dispatcher.IsValid())
	{
		Dispatcher.Pin()->Unregister(*this);
	}

	Reset();
	return true;
}

void FDependencySignalListenerHandle::Reset()
{
	Dispatcher.Reset();
	Id = 0;
}