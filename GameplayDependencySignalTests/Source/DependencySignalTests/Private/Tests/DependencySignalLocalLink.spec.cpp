#if WITH_AUTOMATION_TESTS

	#include "DependencySignalDispatcher.h"
	#include "DependencySignalTags.h"
	#include "Misc/AutomationTest.h"
	#include "GameplayTagContainer.h"
	#include "Helpers/DependencySignalTestHelpers.h"
	#include "UObject/UObjectGlobals.h"

using namespace DependencySignal::Tests;

BEGIN_DEFINE_SPEC(FDependencySignalLocalLinkSpec, "DependencySignal.LocalLink",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::EngineFilter)

/** Registers a local session listener that records every state it is notified with. */
FDependencySignalListenerHandle RegisterLocalListener(const FGameplayTagContainer& Tags, TArray<bool>& OutCalls);

TStrongObjectPtr<UDepTestObject> WorldContext;
TSharedPtr<FDependencySignalDispatcher> Dispatcher;
TStrongObjectPtr<UDepTestObject> SourceA;
TStrongObjectPtr<UDepTestObject> SourceB;

END_DEFINE_SPEC(FDependencySignalLocalLinkSpec)

void FDependencySignalLocalLinkSpec::Define()
{
	BeforeEach([&]() {
		FGameplayTagContainer RootTags;
		RootTags.AddTagFast(DependencySignalTags::Root);
		RootTags.AddTagFast(FNativeGameplayTags::Get().TestRoot);

		WorldContext = TStrongObjectPtr(NewObject<UDepTestObject>());
		Dispatcher = MakeShared<FDependencySignalDispatcher>(RootTags, WorldContext.Get());
		SourceA = TStrongObjectPtr(NewObject<UDepTestObject>());
		SourceB = TStrongObjectPtr(NewObject<UDepTestObject>());
	});

	AfterEach([&]() {
		Dispatcher.Reset();
		SourceA.Reset();
		SourceB.Reset();
		WorldContext.Reset();
	});

	Describe("Unlink", [this]() {
		It("Should invalidate the local tags of the link owner once", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			TArray<bool> Calls;
			FDependencySignalListenerHandle Handle = RegisterLocalListener(FGameplayTagContainer(TagA), Calls);

			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());
			TestTrue(TEXT("Local tag valid"), Dispatcher->IsValidDependency(TagA));
			TestTrue(TEXT("SourceA owns the link"), Dispatcher->GetLocalLinkOwner(TagA) == SourceA.Get());

			Dispatcher->UnlinkLocalSource(SourceA.Get());
			TestFalse(TEXT("Local tag invalid after unlink"), Dispatcher->IsValidDependency(TagA));
			TestNull(TEXT("No owner after unlink"), Dispatcher->GetLocalLinkOwner(TagA));
			TestTrue(TEXT("Calls: registration, valid, invalid"), Calls == TArray<bool>{ false, true, false });

			Dispatcher->UnlinkLocalSource(SourceA.Get());
			TestEqual(TEXT("A second unlink does nothing"), Calls.Num(), 3);

			Handle.Unregister();
		});

		It("Should not touch the source's own scope", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			Dispatcher->Validate(TagA, true, SourceA.Get());
			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());
			Dispatcher->UnlinkLocalSource(SourceA.Get());

			TestFalse(TEXT("Local tag invalid"), Dispatcher->IsValidDependency(TagA));
			TestTrue(TEXT("Source scoped tag still valid"), Dispatcher->IsValidDependency(TagA, SourceA.Get()));
		});

		It("Should fire a single transition when parent and child are unlinked", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;
			const FGameplayTag& TagAB = FNativeGameplayTags::Get().Test_A_B;

			Dispatcher->ValidateLocalLink(TagAB, true, SourceA.Get());
			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());

			FGameplayTagContainer Tags;
			Tags.AddTag(TagA);
			Tags.AddTag(TagAB);

			TArray<bool> Calls;
			FDependencySignalListenerHandle Handle = RegisterLocalListener(Tags, Calls);

			Dispatcher->UnlinkLocalSource(SourceA.Get());
			TestTrue(TEXT("Calls: registration, invalid"), Calls == TArray<bool>{ true, false });
			TestNull(TEXT("Child link dropped too"), Dispatcher->GetLocalLinkOwner(TagAB));

			Handle.Unregister();
		});
	});

	Describe("Ownership", [this]() {
		It("Should keep the tag valid when a replaced owner unlinks", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());
			Dispatcher->ValidateLocalLink(TagA, true, SourceB.Get());
			TestTrue(TEXT("Last writer owns the link"), Dispatcher->GetLocalLinkOwner(TagA) == SourceB.Get());

			Dispatcher->UnlinkLocalSource(SourceA.Get());
			TestTrue(TEXT("Still valid after the old owner unlinks"), Dispatcher->IsValidDependency(TagA));

			Dispatcher->ValidateLocalLink(TagA, false, SourceA.Get());
			TestTrue(TEXT("Still valid after the old owner invalidates"), Dispatcher->IsValidDependency(TagA));

			Dispatcher->ValidateLocalLink(TagA, false, SourceB.Get());
			TestFalse(TEXT("Invalid after the owner invalidates"), Dispatcher->IsValidDependency(TagA));
		});

		It("Should ignore an invalidation from a source that never linked the tag", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			Dispatcher->Validate(TagA, true);
			Dispatcher->ValidateLocalLink(TagA, false, SourceA.Get());
			TestTrue(TEXT("Unowned local tag kept"), Dispatcher->IsValidDependency(TagA));
		});

		It("Should let a direct local write take the tag over", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());
			Dispatcher->Validate(TagA, true);
			TestNull(TEXT("Owner dropped by a direct write"), Dispatcher->GetLocalLinkOwner(TagA));

			Dispatcher->UnlinkLocalSource(SourceA.Get());
			TestTrue(TEXT("Still valid after the former owner unlinks"), Dispatcher->IsValidDependency(TagA));
		});

		It("Should invalidate unconditionally without a link owner", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());
			Dispatcher->ValidateLocalLink(TagA, false, nullptr);
			TestFalse(TEXT("Local tag invalid"), Dispatcher->IsValidDependency(TagA));
			TestNull(TEXT("No owner"), Dispatcher->GetLocalLinkOwner(TagA));
		});
	});

	Describe("InvalidateSource", [this]() {
		It("Should invalidate every non root tag of the source", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;
			const FGameplayTag& TagAB = FNativeGameplayTags::Get().Test_A_B;
			const FGameplayTag& TagB = FNativeGameplayTags::Get().Test_B;
			const FGameplayTag& Root = FNativeGameplayTags::Get().TestRoot;

			Dispatcher->Validate(TagA, true, SourceA.Get());
			Dispatcher->Validate(TagAB, true, SourceA.Get());
			Dispatcher->Validate(TagB, true, SourceA.Get());
			Dispatcher->Validate(TagB, true, SourceB.Get());

			Dispatcher->InvalidateSource(SourceA.Get());

			TestFalse(TEXT("TagA invalid"), Dispatcher->IsValidDependency(TagA, SourceA.Get()));
			TestFalse(TEXT("TagA.TagB invalid"), Dispatcher->IsValidDependency(TagAB, SourceA.Get()));
			TestFalse(TEXT("TagB invalid"), Dispatcher->IsValidDependency(TagB, SourceA.Get()));
			TestTrue(TEXT("Root still valid"), Dispatcher->IsValidDependency(Root, SourceA.Get()));
			TestTrue(TEXT("Other source untouched"), Dispatcher->IsValidDependency(TagB, SourceB.Get()));
		});
	});

	Describe("Garbage collection", [this]() {
		It("Should invalidate the local links of a collected owner on prune", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;

			Dispatcher->ValidateLocalLink(TagA, true, SourceA.Get());

			SourceA.Reset();
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			TestTrue(TEXT("Still valid before the prune"), Dispatcher->IsValidDependency(TagA));

			Dispatcher->PruneStaleSources();
			TestFalse(TEXT("Invalid after the prune"), Dispatcher->IsValidDependency(TagA));
		});
	});
}

FDependencySignalListenerHandle FDependencySignalLocalLinkSpec::RegisterLocalListener(
	const FGameplayTagContainer& Tags, TArray<bool>& OutCalls)
{
	return Dispatcher->Register(FDependencySignalRequest()
			.AddTags(Tags)
			.BindLambda([&OutCalls](const bool bIsValid, UObject*) {
				OutCalls.Add(bIsValid);
			})
			.End());
}

#endif // WITH_AUTOMATION_TESTS
