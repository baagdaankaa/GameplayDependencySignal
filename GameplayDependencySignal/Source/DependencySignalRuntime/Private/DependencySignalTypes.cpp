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

FDependencySignalRequest& FDependencySignalRequest::SetAnySource(const bool bAny)
{
	bAnySource = bAny;
	return *this;
}

FDependencySignalRequest& FDependencySignalRequest::MarkAsAnySource()
{
	return SetAnySource(true);
}

bool FDependencySignalListenerHandle::Unregister()
{
	if (!IsValid())
	{
		return false;
	}

	if (const TSharedPtr<FDependencySignalDispatcher> PinnedDispatcher = Dispatcher.Pin())
	{
		PinnedDispatcher->Unregister(*this);
	}

	Reset();
	return true;
}

void FDependencySignalListenerHandle::Reset()
{
	Dispatcher.Reset();
	Id = 0;
}

FScopedDependencySignalListener::FScopedDependencySignalListener(FDependencySignalListenerHandle&& InHandle)
	: Handle(MoveTemp(InHandle))
{
	InHandle = FDependencySignalListenerHandle();
}

FScopedDependencySignalListener::FScopedDependencySignalListener(FScopedDependencySignalListener&& Other)
	: Handle(Other.Release())
{
}

FScopedDependencySignalListener::~FScopedDependencySignalListener()
{
	Handle.Unregister();
}

FScopedDependencySignalListener& FScopedDependencySignalListener::operator=(FScopedDependencySignalListener&& Other)
{
	if (this != &Other)
	{
		*this = Other.Release();
	}

	return *this;
}

FScopedDependencySignalListener& FScopedDependencySignalListener::operator=(FDependencySignalListenerHandle&& InHandle)
{
	if (Handle != InHandle)
	{
		Handle.Unregister();
	}

	Handle = MoveTemp(InHandle);
	InHandle = FDependencySignalListenerHandle();
	return *this;
}

void FScopedDependencySignalListener::Reset()
{
	Handle.Unregister();
}

FDependencySignalListenerHandle FScopedDependencySignalListener::Release()
{
	FDependencySignalListenerHandle Released = MoveTemp(Handle);
	Handle = FDependencySignalListenerHandle();
	return Released;
}