#if WITH_AUTOMATION_TESTS

	#include "DependencySignalDispatcher.h"
	#include "DependencySignalTags.h"
	#include "Misc/AutomationTest.h"
	#include "GameplayTagContainer.h"
	#include "Algo/AllOf.h"
	#include "Helpers/DependencySignalTestHelpers.h"
	#include "UObject/UObjectGlobals.h"

using namespace DependencySignal::Tests;

BEGIN_DEFINE_SPEC(FDependencySignalAnySourceSpec, "DependencySignal.AnySource",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::EngineFilter)

struct FTestAnyDependencyConfig : public FTestDependencyConfigBase
{
	TArray<FTestVerification> VerificationQueue;
};

using FTestConfig = FTestAnyDependencyConfig;

void LaunchConfigIt(const TFunction<void(FTestConfig&)>& SetupConfigFunc);

TStrongObjectPtr<UDepTestObject> WorldContext;
TSharedPtr<FDependencySignalDispatcher> Dispatcher;
TArray<TStrongObjectPtr<UObject>> SourceObjects;

END_DEFINE_SPEC(FDependencySignalAnySourceSpec)

void FDependencySignalAnySourceSpec::Define()
{
	BeforeEach([&]() {
		FGameplayTagContainer RootTags;
		RootTags.AddTagFast(DependencySignalTags::Root);
		RootTags.AddTagFast(FNativeGameplayTags::Get().TestRoot);

		WorldContext = TStrongObjectPtr(NewObject<UDepTestObject>());
		Dispatcher = MakeShared<FDependencySignalDispatcher>(RootTags, WorldContext.Get());
		TestTrue("Dependency dispatcher valid", Dispatcher.IsValid());

		for (int32 Index = 0; Index < 4; ++Index)
		{
			SourceObjects.Add(TStrongObjectPtr(NewObject<UDepTestObject>()));
		}

		TestTrue("Source objects created and valid",
			!SourceObjects.IsEmpty() && Algo::AllOf(SourceObjects, [](const TStrongObjectPtr<UObject>& InObject) {
				return InObject.IsValid();
			}));
	});

	AfterEach([&]() {
		Dispatcher.Reset();
		SourceObjects.Empty();
	});

	Describe("Registration with local any source should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
			// No source has any dependency state yet, so an any-source listener has nothing to report.
			// (Sources only appear once validated; see "Validation should work", whose first callback is Source0.)
		});
	});

	Describe("Registration with local any source with prevalidation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
			Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, nullptr });
			Config.VerificationQueue.Add({ true, nullptr });
		});
	});

	Describe("Prevalidation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_B);

			if (!SourceObjects.IsEmpty())
			{
				Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_B, true, SourceObjects[0].Get() });
				Config.VerificationQueue.Add({ true, SourceObjects[0].Get() });
			}
		});
	});

	Describe("Validation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);

			if (!SourceObjects.IsEmpty())
			{
				// during first registration calls with invalid state
				Config.VerificationQueue.Add({ false, SourceObjects[0].Get() });

				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObjects[0].Get() });
				Config.VerificationQueue.Add({ true, SourceObjects[0].Get() });
			}
		});
	});

	Describe("Invalidation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);

			if (!SourceObjects.IsEmpty())
			{
				// during first registration calls with invalid state
				Config.VerificationQueue.Add({ false, SourceObjects[0].Get() });

				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObjects[0].Get() });
				Config.VerificationQueue.Add({ true, SourceObjects[0].Get() });

				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, false, SourceObjects[0].Get() });
				Config.VerificationQueue.Add({ false, SourceObjects[0].Get() });
			}
		});
	});

	Describe("Multiple sources prevalidation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);

			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObject.Get() });
				Config.VerificationQueue.Add({ true, SourceObject.Get() });
			}
		});
	});

	Describe("Multiple sources validation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);

			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				// during first registration calls with invalid state
				Config.VerificationQueue.Add({ false, SourceObject.Get() });

				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObject.Get() });
				Config.VerificationQueue.Add({ true, SourceObject.Get() });
			}
		});
	});

	Describe("Multiple sources prevalidation and invalidation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_B);

			// validation
			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_B, true, SourceObject.Get() });
				Config.VerificationQueue.Add({ true, SourceObject.Get() });
			}

			// invalidation
			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_B, false, SourceObject.Get() });
				Config.VerificationQueue.Add({ false, SourceObject.Get() });
			}
		});
	});

	Describe("Multiple sources invalidation should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);

			// validation
			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				// during first registration calls with invalid state
				Config.VerificationQueue.Add({ false, SourceObject.Get() });

				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObject.Get() });
				Config.VerificationQueue.Add({ true, SourceObject.Get() });
			}

			// invalidation
			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, false, SourceObject.Get() });
				Config.VerificationQueue.Add({ false, SourceObject.Get() });
			}
		});
	});

	Describe("Prevalidation with parent dependency should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A_B);

			if (!SourceObjects.IsEmpty())
			{
				Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObjects[0].Get() });
				Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A_B, true, SourceObjects[0].Get() });

				Config.VerificationQueue.Add({ true, SourceObjects[0].Get() });

				// parent tag invalidation
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, false, SourceObjects[0].Get() });

				Config.VerificationQueue.Add({ false, SourceObjects[0].Get() });
			}
		});
	});

	Describe("Validation with parent dependency should work", [this]() {
		LaunchConfigIt([this](FTestConfig& Config) {
			Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A_B);

			if (!SourceObjects.IsEmpty())
			{
				// during first registration calls with invalid state
				Config.VerificationQueue.Add({ false, SourceObjects[0].Get() });

				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A_B, true, SourceObjects[0].Get() });

				// parent tag validation
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true, SourceObjects[0].Get() });

				Config.VerificationQueue.Add({ true, SourceObjects[0].Get() });

				// parent tag invalidation
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, false, SourceObjects[0].Get() });

				Config.VerificationQueue.Add({ false, SourceObjects[0].Get() });
			}
		});
	});

	Describe("Unregistered any-source listener should not fire afterwards", [this]() {
		It("Should remove per-source listeners and stop broadcasting after Unregister", [this] {
			int32 CallsAfterUnregister = 0;
			bool bUnregistered = false;

			FDependencySignalListenerHandle Handle = Dispatcher->Register(
				FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.MarkAsAnySource()
					.BindLambda([&CallsAfterUnregister, &bUnregistered](const bool, UObject*) {
						if (bUnregistered)
						{
							++CallsAfterUnregister;
						}
					})
					.End());
			TestTrue(TEXT("Successfully registered"), Handle.IsValid());

			// Validating per source fans the any-source listener out into per-source listeners.
			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true, SourceObject.Get());
			}
			TestTrue(TEXT("Per-source listeners created while registered"), Dispatcher->GetListenersNum() > 0);

			bUnregistered = true;
			Handle.Unregister();

			TestEqual(TEXT("No any listener after unregistration"), Dispatcher->GetAnyListenersNum(), 0);
			TestEqual(TEXT("No per-source listeners after unregistration"), Dispatcher->GetListenersNum(), 0);

			// Toggling state after unregistration must not invoke the removed listener.
			for (const TStrongObjectPtr<UObject>& SourceObject : SourceObjects)
			{
				Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, false, SourceObject.Get());
				Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true, SourceObject.Get());
			}
			TestEqual(TEXT("Listener not invoked after unregistration"), CallsAfterUnregister, 0);
		});
	});

	Describe("Any-source listener registered after a source exists", [this]() {
		It("Should notify that pre-existing source on registration and on later changes", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;
			UObject* Source = SourceObjects[0].Get();

			Dispatcher->Validate(TagA, true, Source);

			TArray<TPair<bool, UObject*>> Calls;
			FDependencySignalListenerHandle Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(TagA)
					.MarkAsAnySource()
					.BindLambda([&Calls](const bool bIsValid, UObject* InSource) {
						Calls.Emplace(bIsValid, InSource);
					})
					.End());

			// Regression: the per-source copy used to be built from the moved-from request (unbound delegate).
			TestEqual(TEXT("Registration reports the pre-existing source"), Calls.Num(), 1);

			Dispatcher->Validate(TagA, false, Source);
			TestEqual(TEXT("Later change on the pre-existing source is reported"), Calls.Num(), 2);

			if (Calls.Num() == 2)
			{
				TestTrue(TEXT("First call is valid for Source0"), Calls[0].Key && Calls[0].Value == Source);
				TestTrue(TEXT("Second call is invalid for Source0"), !Calls[1].Key && Calls[1].Value == Source);
			}

			Handle.Unregister();
		});

		It("Should attach a multi-tag listener to tags the source has not validated yet", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;
			const FGameplayTag& TagB = FNativeGameplayTags::Get().Test_B;
			UObject* Source = SourceObjects[0].Get();

			// The source exists but only knows about TagA.
			Dispatcher->Validate(TagA, true, Source);

			TOptional<bool> bLastState;
			FDependencySignalListenerHandle Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(TagA)
					.AddTag(TagB)
					.MarkAsAnySource()
					.BindLambda([&bLastState](const bool bIsValid, UObject*) {
						bLastState = bIsValid;
					})
					.End());
			TestTrue(TEXT("Registration reports TagB still missing"), bLastState.IsSet() && !bLastState.GetValue());

			Dispatcher->Validate(TagB, true, Source);
			TestTrue(TEXT("Validating TagB later reaches the listener"), bLastState.IsSet() && bLastState.GetValue());

			Handle.Unregister();
		});
	});

	Describe("Listener lifetime", [this]() {
		It("Should skip a garbage-flagged owner and drop it only after it is collected", [this] {
			const FGameplayTag& TagA = FNativeGameplayTags::Get().Test_A;
			UObject* Source = SourceObjects[0].Get();

			TStrongObjectPtr<UDepTestObject> Owner(NewObject<UDepTestObject>());
			int32 CallsNum = 0;

			FDependencySignalListenerHandle Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(TagA)
					.MarkAsAnySource()
					.BindWeakLambda(Owner.Get(), [&CallsNum](const bool, UObject*) {
						++CallsNum;
					})
					.End());

			Dispatcher->Validate(TagA, true, Source);
			TestEqual(TEXT("One per-source listener"), Dispatcher->GetListenersNum(), 1);
			TestEqual(TEXT("One any-source registration"), Dispatcher->GetAnyListenersNum(), 1);
			const int32 CallsBeforeGarbage = CallsNum;

			// Garbage-flagged but not collected yet: IsBound() is false, the listener must be skipped, not deleted.
			Owner->MarkAsGarbage();
			Dispatcher->Validate(TagA, false, Source);
			TestEqual(TEXT("No call to a garbage-flagged owner"), CallsNum, CallsBeforeGarbage);
			TestEqual(TEXT("Listener kept while owner is not collected"), Dispatcher->GetListenersNum(), 1);

			// Once the owner is collected the prune removes both the per-source listener and the registration.
			// In game the subsystem runs it after every GC; a bare dispatcher needs it called explicitly.
			Owner.Reset();
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			TestEqual(TEXT("Nothing pruned without an explicit prune"), Dispatcher->GetListenersNum(), 1);

			Dispatcher->PruneStaleSources();
			TestEqual(TEXT("Per-source listener pruned after GC"), Dispatcher->GetListenersNum(), 0);
			TestEqual(TEXT("Any-source registration pruned after GC"), Dispatcher->GetAnyListenersNum(), 0);

			Handle.Unregister();
		});
	});

	Describe("Reset should clear every registration", [this]() {
		It("Should empty both listener maps after Reset", [this] {
			FDependencySignalListenerHandle AnyHandle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.MarkAsAnySource()
					.BindLambda([](const bool, UObject*) {})
					.End());
			TestTrue(TEXT("Any-source listener registered"), AnyHandle.IsValid());

			if (!SourceObjects.IsEmpty())
			{
				FDependencySignalListenerHandle SourceHandle = Dispatcher->Register(FDependencySignalRequest()
						.AddTag(FNativeGameplayTags::Get().Test_A)
						.AddSource(SourceObjects[0].Get())
						.BindLambda([](const bool, UObject*) {})
						.End());
				TestTrue(TEXT("Source listener registered"), SourceHandle.IsValid());
			}

			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true, SourceObjects[0].Get());

			Dispatcher->Reset();

			TestEqual(TEXT("No listeners after Reset"), Dispatcher->GetListenersNum(), 0);
			TestEqual(TEXT("No any listeners after Reset"), Dispatcher->GetAnyListenersNum(), 0);
		});
	});
}

void FDependencySignalAnySourceSpec::LaunchConfigIt(const TFunction<void(FTestConfig&)>& SetupConfigFunc)
{
	FTestConfig PreConfig;
	SetupConfigFunc(PreConfig);

	It(FString::Printf(TEXT("Should be valid of [%s] in Order [%s]"),
		   *GetRegistrationTagsString(PreConfig.RegistrationTags),
		   *GetValidationsSequenceString(PreConfig.PreValidationQueue, PreConfig.ValidationQueue)),
		[this, SetupConfigFunc] {
			int32 IndexOfTest = 0;

			FTestConfig Config;
			SetupConfigFunc(Config);

			for (const FTestDependency& Entry : Config.PreValidationQueue)
			{
				const FString SourceObjectStr = Entry.SourceObject.IsValid()
					? *FString::Printf(TEXT("for %s"), *Entry.SourceObject->GetName())
					: TEXT("");

				AddInfo(FString::Printf(TEXT("Manual %s [%s] dependency%s"),
					Entry.IsValid ? TEXT("validation") : TEXT("invalidation"), *Entry.GetTagString(),
					*SourceObjectStr));

				Dispatcher->Validate(Entry.Tag, Entry.IsValid, Entry.SourceObject.Get());
			}

			AddInfo(
				FString::Printf(TEXT("Register listener with tag [%s]"), *Config.RegistrationTags.ToStringSimple()));

			FDependencySignalListenerHandle Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTags(Config.RegistrationTags)
					.MarkAsAnySource()
					.BindLambda([this, &IndexOfTest, VerificationQueue = Config.VerificationQueue](
									const bool bIsValid, UObject* SourceObject) {
						if (VerificationQueue.IsValidIndex(IndexOfTest))
						{
							const FTestVerification& TestVerification = VerificationQueue[IndexOfTest];

							TestTrue(TEXT("Callback validation state"), TestVerification.IsValid == bIsValid);
							TestTrue(TEXT("Callback source object"), TestVerification.SourceObject == SourceObject);
						}
						else
						{
							AddError(FString::Printf(TEXT("Invalid verification test index: %i | Tests Num: %i"),
								IndexOfTest, VerificationQueue.Num()));
						}

						IndexOfTest++;
					})
					.End());
			TestTrue(TEXT("Successfully registered"), Handle.IsValid());

			for (const FTestDependency& Entry : Config.ValidationQueue)
			{
				AddInfo(FString::Printf(TEXT("Manual %s [%s] dependency with source: %s"),
					Entry.IsValid ? TEXT("validation") : TEXT("invalidation"), *Entry.GetTagString(),
					*GetNameSafe(Entry.SourceObject.Get())));
				Dispatcher->Validate(Entry.Tag, Entry.IsValid, Entry.SourceObject.Get());
			}

			// The callback only verifies calls that happen; also fail when an expected callback never arrived.
			TestEqual(TEXT("All expected callbacks received"), IndexOfTest, Config.VerificationQueue.Num());

			Handle.Unregister();
			TestFalse(TEXT("Invalid handle after unregistration"), Handle.IsValid());
			TestEqual(TEXT("No any listener after unregistration"), Dispatcher->GetAnyListenersNum(), 0);
			TestEqual(TEXT("No per-source listeners after unregistration"), Dispatcher->GetListenersNum(), 0);
		});
}

#endif // WITH_AUTOMATION_TESTS