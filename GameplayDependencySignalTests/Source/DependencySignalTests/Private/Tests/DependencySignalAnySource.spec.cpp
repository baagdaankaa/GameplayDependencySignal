#if WITH_AUTOMATION_TESTS

	#include "DependencySignalDispatcher.h"
	#include "DependencySignalTags.h"
	#include "Misc/AutomationTest.h"
	#include "GameplayTagContainer.h"
	#include "Algo/AllOf.h"
	#include "Helpers/DependencySignalTestHelpers.h"

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
			Config.VerificationQueue.Add({ false, nullptr });
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

			Handle.Unregister();
			TestFalse(TEXT("Invalid handle after unregistration"), Handle.IsValid());
			TestEqual(TEXT("No any listener after unregistration"), Dispatcher->GetAnyListenersNum(), 0);
		});
}

#endif // WITH_AUTOMATION_TESTS