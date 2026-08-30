#if WITH_AUTOMATION_TESTS

	#include "Helpers/DependencySignalTestHelpers.h"
	#include "Misc/AutomationTest.h"

	#include "DependencySignalDispatcher.h"
	#include "DependencySignalTags.h"

using namespace DependencySignal::Tests;

BEGIN_DEFINE_SPEC(FDependencySignalValidationSpec, "DependencySignal.Validation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::EngineFilter)

struct FTestDependencyValidationConfig : public FTestDependencyConfigBase
{
	TOptional<bool> bFinalValidationResult;
};

using FTestConfig = FTestDependencyValidationConfig;

void CheckTagABValidation(const bool bIsABValidate, const bool bIsValid, const bool bIsValidA, const bool bIsValidAB,
	const UObject* InSourceObject);

void TestHierarchicalDependencyWith(const UObject* InSourceObject);
void TestValidation(const FGameplayTag& InTag, bool bShouldBeValid, const UObject* InSourceObject);

void LaunchConfigIt(const TFunction<void(FTestConfig&)>& SetupConfigFunc);

TStrongObjectPtr<UDepTestObject> WorldContext;
TSharedPtr<FDependencySignalDispatcher> Dispatcher;
TStrongObjectPtr<UObject> SourceObject;

END_DEFINE_SPEC(FDependencySignalValidationSpec)

void FDependencySignalValidationSpec::Define()
{
	BeforeEach([&]() {
		FGameplayTagContainer RootTags;
		RootTags.AddTagFast(DependencySignalTags::Root);
		RootTags.AddTagFast(FNativeGameplayTags::Get().TestRoot);

		WorldContext = TStrongObjectPtr(NewObject<UDepTestObject>());
		Dispatcher = MakeShared<FDependencySignalDispatcher>(RootTags, WorldContext.Get());
		TestTrue("Dependency dispatcher valid", Dispatcher.IsValid());
	});

	AfterEach([&]() {
		Dispatcher.Reset();
	});

	Describe("Initial dependency", [this] {
		It("Root tag should be VALID from the beginning", [&]() {
			AddInfo(FString::Printf(TEXT("Register root tag")));
			Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().TestRoot).End());
			TestTrue("Root valid", Dispatcher->IsValidDependency(FNativeGameplayTags::Get().TestRoot));
		});

		const auto& TestDependencyTag = [this](const FGameplayTag& Tag) {
			It(FString::Printf(TEXT("Tag [%s] should be INVALID after registration"), *Tag.GetTagName().ToString()),
				[this, CapturedTag = Tag]() {
					AddInfo(
						FString::Printf(TEXT("Register listener with [%s] tag"), *CapturedTag.GetTagName().ToString()));
					Dispatcher->Register(FDependencySignalRequest().AddTag(CapturedTag).End());
					TestValidation(CapturedTag, false, nullptr);
				});
		};

		TestDependencyTag(FNativeGameplayTags::Get().Test_A);
		TestDependencyTag(FNativeGameplayTags::Get().Test_B);
		TestDependencyTag(FNativeGameplayTags::Get().Test_C);
	});

	Describe("Hierarchical dependency", [this]() {
		Describe("Locally", [this]() {
			TestHierarchicalDependencyWith(nullptr);
		});

		BeforeEach([&]() {
			SourceObject = TStrongObjectPtr(NewObject<USceneComponent>());
			TestTrue("Source object valid", SourceObject.IsValid());
		});

		AfterEach([&]() {
			SourceObject.Reset();
		});

		Describe(FString::Printf(TEXT("With source object [%s]"), *GetNameSafe(SourceObject.Get())), [&]() {
			TestHierarchicalDependencyWith(SourceObject.Get());
		});
	});

	Describe("Callback", [this] {
		Describe("With one dependency", [this] {
			Describe("Should be called with validation state - False", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.bFinalValidationResult = false;
				});
			});

			Describe("Should be called with validation state - True", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.bFinalValidationResult = true;
				});
			});

			Describe("Should be called with validation state - True", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
					Config.bFinalValidationResult = true;
				});
			});
		});

		Describe("With two dependency", [this] {
			Describe("1", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_B);
					Config.bFinalValidationResult = false;
				});
			});

			Describe("2", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
					Config.PreValidationQueue.Add({ FNativeGameplayTags::Get().Test_B, true });
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_B);
					Config.bFinalValidationResult = true;
				});
			});

			Describe("3", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_B);
					Config.bFinalValidationResult = false;
				});
			});

			Describe("4", [this] {
				LaunchConfigIt([](FTestConfig& Config) {
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A);
					Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_B);
					Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
					Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_B, true });
					Config.bFinalValidationResult = true;
				});
			});
		});

		Describe("With child dependency", [this] {
			LaunchConfigIt([](FTestConfig& Config) {
				Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A_B);
				Config.bFinalValidationResult = false;
			});

			LaunchConfigIt([](FTestConfig& Config) {
				Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A_B);
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A_B, true });
				Config.bFinalValidationResult = true;
			});

			LaunchConfigIt([](FTestConfig& Config) {
				Config.RegistrationTags.AddTagFast(FNativeGameplayTags::Get().Test_A_B);
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A_B, true });
				Config.ValidationQueue.Add({ FNativeGameplayTags::Get().Test_A, true });
				Config.bFinalValidationResult = true;
			});
		});
	});
}

void FDependencySignalValidationSpec::CheckTagABValidation(const bool bIsABValidate, const bool bIsValid,
	const bool bIsValidA, const bool bIsValidAB, const UObject* InSourceObject)
{
	check(Dispatcher.IsValid());

	AddInfo(FString::Printf(TEXT("Call %s %s"), bIsABValidate ? TEXT("TagA") : TEXT("TagA_B"),
		bIsValid ? TEXT("validation") : TEXT("invalidation")));
	Dispatcher->Validate(bIsABValidate ? FNativeGameplayTags::Get().Test_A_B : FNativeGameplayTags::Get().Test_A,
		bIsValid, InSourceObject);

	TestValidation(FNativeGameplayTags::Get().Test_A, bIsValidA, InSourceObject);
	TestValidation(FNativeGameplayTags::Get().Test_A_B, bIsValidAB, InSourceObject);
}

void FDependencySignalValidationSpec::TestHierarchicalDependencyWith(const UObject* InSourceObject)
{
	Describe("Validations", [&, InSourceObject]() {
		It("Should be validated", [&, InSourceObject]() {
			TestValidation(FNativeGameplayTags::Get().Test_A, false, InSourceObject);

			AddInfo("Call TagA validation");
			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true, InSourceObject);

			TestValidation(FNativeGameplayTags::Get().Test_A, true, InSourceObject);
		});

		BeforeEach([&, InSourceObject]() {
			TestValidation(FNativeGameplayTags::Get().Test_A, false, InSourceObject);
			TestValidation(FNativeGameplayTags::Get().Test_A_B, false, InSourceObject);
		});

		It("Should be validated. In Order -> TagA (valid) -> TagA_B (valid)", [this, InSourceObject]() {
			// clang-format off
			CheckTagABValidation(
				/* bIsABValidate */ false,
				/* bIsValid */ true,
				/* bIsValidA */ true,
				/* bIsValidAB */ false, 
				InSourceObject);

			CheckTagABValidation(
				/* bIsABValidate */ true,
				/* bIsValid */ true,
				/* bIsValidA */ true,
				/* bIsValidAB */ true, 
				InSourceObject);
			// clang-format on
		});

		It("Should be validated. In Order -> TagA_B (valid) -> TagA (valid)", [this, InSourceObject]() {
			// clang-format off
			CheckTagABValidation(
				/* bIsABValidate */ true,
				/* bIsValid */ true,
				/* bIsValidA */ false,
				/* bIsValidAB */ false, 
				InSourceObject);

			CheckTagABValidation(
				/* bIsABValidate */ false,
				/* bIsValid */ true,
				/* bIsValidA */ true,
				/* bIsValidAB */ true, 
				InSourceObject);
			// clang-format on
		});
	});

	Describe("Invalidations", [this, InSourceObject]() {
		It("Should be invalidated", [this, InSourceObject]() {
			TestValidation(FNativeGameplayTags::Get().Test_A, false, InSourceObject);

			AddInfo("Call TagA validation");
			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true, InSourceObject);

			TestValidation(FNativeGameplayTags::Get().Test_A, true, InSourceObject);

			AddInfo("Call TagA invalidation");
			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, false, InSourceObject);

			TestValidation(FNativeGameplayTags::Get().Test_A, false, InSourceObject);
		});

		BeforeEach([&, InSourceObject]() {
			TestValidation(FNativeGameplayTags::Get().Test_A, false, InSourceObject);
			TestValidation(FNativeGameplayTags::Get().Test_A_B, false, InSourceObject);
		});

		It("Should be invalidated. In Order -> TagA (valid) -> TagA_B (valid) -> TagA_B (invalid) -> TagA (invalid)",
			[this, InSourceObject]() {
				// clang-format off
				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ true, 
					InSourceObject);

					// Invalidation

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ false,
					/* bIsValidA */ true,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);
				// clang-format on
			});

		It("Should be invalidated. In Order -> TagA (valid) -> TagA_B (valid) -> TagA (invalid) -> TagA_B (invalid)",
			[this, InSourceObject]() {
				// clang-format off
				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ true, 
					InSourceObject);

					// Invalidation

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);
				// clang-format on
			});

		It("Should be invalidated. In Order -> TagA_B (valid) -> TagA (valid) -> TagA (invalid) -> TagA_B (invalid)",
			[this, InSourceObject]() {
				// clang-format off
				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ true,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ true, 
					InSourceObject);

					// Invalidation

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);
				// clang-format on
			});

		It("Should be invalidated. In Order -> TagA_B (valid) -> TagA (valid) -> TagA_B (invalid) -> TagA (invalid)",
			[this, InSourceObject]() {
				// clang-format off
				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ true,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ true, 
					InSourceObject);

					// Invalidation

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ false,
					/* bIsValidA */ true,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);
				// clang-format on
			});

		It("Should be invalidated. In Order -> TagA (valid) -> TagA_B (valid) -> TagA (invalid) -> TagA (valid)",
			[this, InSourceObject]() {
				// clang-format off
				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ false, 
					InSourceObject);

				CheckTagABValidation(
					/* bIsABValidate */ true,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ true, 
					InSourceObject);

					// Invalidation

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ false,
					/* bIsValidA */ false,
					/* bIsValidAB */ false, 
					InSourceObject);

					// Validation

				CheckTagABValidation(
					/* bIsABValidate */ false,
					/* bIsValid */ true,
					/* bIsValidA */ true,
					/* bIsValidAB */ true, 
					InSourceObject);
				// clang-format on
			});
	});
}

void FDependencySignalValidationSpec::TestValidation(
	const FGameplayTag& InTag, const bool bShouldBeValid, const UObject* InSourceObject)
{
	const FString SourceStr = !IsValid(InSourceObject)
		? TEXT("Locally")
		: *FString::Printf(TEXT("with [%s] as Source"), *GetNameSafe(InSourceObject));

	if (bShouldBeValid)
	{
		TestTrue(
			FString::Printf(TEXT("Dependency [%s] should be VALID %s"), *InTag.GetTagName().ToString(), *SourceStr),
			Dispatcher->IsValidDependency(InTag, InSourceObject));
	}
	else
	{
		TestFalse(
			FString::Printf(TEXT("Dependency [%s] should be INVALID %s"), *InTag.GetTagName().ToString(), *SourceStr),
			Dispatcher->IsValidDependency(InTag, InSourceObject));
	}
}

void FDependencySignalValidationSpec::LaunchConfigIt(const TFunction<void(FTestConfig&)>& SetupConfigFunc)
{
	FTestConfig Config;

	SetupConfigFunc(Config);

	It(FString::Printf(TEXT("Should be valid state of [%s] -> Order [%s]"),
		   *GetRegistrationTagsString(Config.RegistrationTags), *GetValidationsSequenceString(Config.ValidationQueue)),
		[this, Config] {
			TOptional<bool> bCallbackValidationResult;

			TestFalse(TEXT("bCallbackValidationResult should be unset"), bCallbackValidationResult.IsSet());
			TestTrue(TEXT("bFinalValidationResult should be set"), Config.bFinalValidationResult.IsSet());

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
			Dispatcher->Register(FDependencySignalRequest()
					.AddTags(Config.RegistrationTags)
					.BindLambda([&bCallbackValidationResult](const bool bIsValid, UObject*) {
						bCallbackValidationResult = bIsValid;
					})
					.End());
			TestTrue(TEXT("bCallbackValidationResult should be set"), bCallbackValidationResult.IsSet());

			for (const FTestDependency& Entry : Config.ValidationQueue)
			{
				const FString SourceObjectStr = Entry.SourceObject.IsValid()
					? *FString::Printf(TEXT("for %s"), *Entry.SourceObject->GetName())
					: TEXT("");

				AddInfo(FString::Printf(TEXT("Manual %s [%s] dependency%s"),
					Entry.IsValid ? TEXT("validation") : TEXT("invalidation"), *Entry.GetTagString(),
					*SourceObjectStr));

				Dispatcher->Validate(Entry.Tag, Entry.IsValid, Entry.SourceObject.Get());
			}

			const bool bIsValidResults = bCallbackValidationResult.IsSet() && Config.bFinalValidationResult.IsSet();

			TestTrue(TEXT("Results valid"), bIsValidResults);

			if (bIsValidResults)
			{
				TestEqual(TEXT("CallbackValidationResult equal FinalValidationResult"),
					bCallbackValidationResult.GetValue(), Config.bFinalValidationResult.GetValue());
			}
		});
}
#endif // WITH_AUTOMATION_TESTS