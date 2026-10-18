#if WITH_AUTOMATION_TESTS

	#include "Helpers/DependencySignalTestHelpers.h"
	#include "Misc/AutomationTest.h"

	#include "DependencySignalDispatcher.h"
	#include "DependencySignalTags.h"

using namespace DependencySignal::Tests;

BEGIN_DEFINE_SPEC(FDependencySignalRegistrationSpec, "DependencySignal.Registration",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext
		| EAutomationTestFlags::EngineFilter)

TStrongObjectPtr<UDepTestObject> WorldContext;
TSharedPtr<FDependencySignalDispatcher> Dispatcher;
TStrongObjectPtr<UObject> SourceObject;

FDependencySignalListenerHandle Handle;

END_DEFINE_SPEC(FDependencySignalRegistrationSpec)

void FDependencySignalRegistrationSpec::Define()
{
	BeforeEach([this]() {
		FGameplayTagContainer RootTags;
		RootTags.AddTagFast(DependencySignalTags::Root);
		RootTags.AddTagFast(FNativeGameplayTags::Get().TestRoot);

		WorldContext = TStrongObjectPtr(NewObject<UDepTestObject>());
		Dispatcher = MakeShared<FDependencySignalDispatcher>(RootTags, WorldContext.Get());
		TestTrue("Dependency dispatcher valid", Dispatcher.IsValid());
	});

	BeforeEach([this]() {
		TestFalse("Listener handle invalid", Handle.IsValid());
		TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
	});

	AfterEach([this]() {
		Dispatcher.Reset();
	});

	Describe("Handle", [this] {
		Describe("After registration", [this] {
			AfterEach([this]() {
				TestTrue("Listener handle valid", Handle.IsValid());
				TestEqual("One listener in dispatcher", Dispatcher->GetListenersNum(), 1);

				Handle.Unregister();
			});

			It("Should be VALID with one tag", [this]() {
				AddInfo("Register dependency listener");
				Handle =
					Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
			});

			It("Should be VALID with two tags using single tag", [this]() {
				AddInfo("Register dependency listener");
				Handle = Dispatcher->Register(FDependencySignalRequest()
						.AddTag(FNativeGameplayTags::Get().Test_A)
						.AddTag(FNativeGameplayTags::Get().Test_B)
						.End());
			});

			It("Should be VALID with two tags using tag container", [this]() {
				FGameplayTagContainer Container;
				Container.AddTag(FNativeGameplayTags::Get().Test_A);
				Container.AddTag(FNativeGameplayTags::Get().Test_B);

				AddInfo("Register dependency listener");
				Handle = Dispatcher->Register(FDependencySignalRequest().AddTags(Container).End());
			});
		});

		Describe("After unregistration", [this] {
			BeforeEach([this]() {
				AddInfo("Register dependency listener");
				Handle =
					Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
				TestTrue("Listener handle valid", Handle.IsValid());
			});

			AfterEach([this]() {
				TestFalse("Listener handle invalid", Handle.IsValid());
				TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
			});

			It("Should be INVALID using Handle", [this]() {
				AddInfo("Unregister listener handle through Handle");
				Handle.Unregister();
			});

			It("Should be INVALID using Dispatcher", [this]() {
				AddInfo("Unregister listener handle through Dispatcher");
				Dispatcher->Unregister(Handle);
			});
		});

		It("Should be INVALID with two tags using Handle", [this]() {
			AddInfo("Register dependency listener");
			FDependencySignalListenerHandle Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.AddTag(FNativeGameplayTags::Get().Test_B)
					.End());
			TestTrue("Listener handle valid", Handle.IsValid());

			AddInfo("Unregister listener handle through Handle");
			Handle.Unregister();

			TestFalse("Listener handle invalid", Handle.IsValid());
			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
		});
	});

	Describe("With Source object", [this] {
		BeforeEach([this]() {
			SourceObject = TStrongObjectPtr(NewObject<USceneComponent>());
			TestTrue("Source object valid", SourceObject.IsValid());
		});

		AfterEach([this]() {
			TestTrue("Listener handle valid", Handle.IsValid());
			TestEqual("One Listener in dispatcher", Dispatcher->GetListenersNum(), 1);

			AddInfo("Unregister listener handle through Handle");
			Handle.Unregister();

			TestFalse("Listener handle invalid", Handle.IsValid());
			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);

			SourceObject.Reset();
		});

		It("Should be VALID registration with one tag", [this]() {
			AddInfo("Register dependency listener");
			Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.AddSource(SourceObject.Get())
					.End());
		});

		It("Should be VALID registration with two tags", [this]() {
			AddInfo("Register dependency listener");
			Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.AddTag(FNativeGameplayTags::Get().Test_B)
					.AddSource(SourceObject.Get())
					.End());
		});

		It("Should be VALID registration with three tags", [this]() {
			AddInfo("Register dependency listener");
			Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.AddTag(FNativeGameplayTags::Get().Test_B)
					.AddTag(FNativeGameplayTags::Get().Test_C)
					.AddSource(SourceObject.Get())
					.End());
		});
	});

	Describe("Rejected requests", [this] {
		It("Should return an INVALID handle for a request without tags", [this]() {
			AddExpectedMessage(TEXT("has no dependency tags"), EAutomationExpectedMessageFlags::Contains, 1);

			bool bCalled = false;
			FDependencySignalListenerHandle EmptyHandle = Dispatcher->Register(FDependencySignalRequest()
					.BindLambda([&bCalled](const bool, UObject*) {
						bCalled = true;
					})
					.End());

			TestFalse("Listener handle invalid", EmptyHandle.IsValid());
			TestFalse("Callback not invoked", bCalled);
			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
		});

		It("Should return false when unregistering an INVALID handle", [this]() {
			FDependencySignalListenerHandle InvalidHandle;
			TestFalse("Unregister of an invalid handle", InvalidHandle.Unregister());
		});

		It("Should return true when unregistering a VALID handle", [this]() {
			FDependencySignalListenerHandle ValidHandle =
				Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
			TestTrue("Unregister of a valid handle", ValidHandle.Unregister());
			TestFalse("Unregister twice", ValidHandle.Unregister());
		});
	});

	Describe("Scoped listener", [this] {
		It("Should unregister when it goes out of scope", [this]() {
			{
				FScopedDependencySignalListener Scoped =
					Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
				TestTrue("Scoped listener valid", Scoped.IsValid());
				TestEqual("One listener in dispatcher", Dispatcher->GetListenersNum(), 1);
			}

			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
		});

		It("Should unregister the previous listener on reassignment", [this]() {
			FScopedDependencySignalListener Scoped =
				Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
			const FDependencySignalListenerHandle FirstHandle = Scoped.Get();

			Scoped = Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_B).End());

			TestTrue("Scoped listener valid", Scoped.IsValid());
			TestTrue("Scoped listener owns the new handle", Scoped.Get() != FirstHandle);
			TestEqual("One listener in dispatcher", Dispatcher->GetListenersNum(), 1);

			Scoped.Reset();
			TestFalse("Scoped listener invalid after Reset", Scoped.IsValid());
			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
		});

		It("Should keep the listener after Release", [this]() {
			FDependencySignalListenerHandle Released;
			{
				FScopedDependencySignalListener Scoped =
					Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
				Released = Scoped.Release();
				TestFalse("Scoped listener invalid after Release", Scoped.IsValid());
			}

			TestTrue("Released handle valid", Released.IsValid());
			TestEqual("One listener in dispatcher", Dispatcher->GetListenersNum(), 1);

			Released.Unregister();
		});

		It("Should transfer ownership on move", [this]() {
			FScopedDependencySignalListener Source =
				Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());
			FScopedDependencySignalListener Target(MoveTemp(Source));

			TestFalse("Moved-from listener invalid", Source.IsValid());
			TestTrue("Moved-to listener valid", Target.IsValid());
			TestEqual("One listener in dispatcher", Dispatcher->GetListenersNum(), 1);

			Target.Reset();
			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);
		});

		It("Should be safe to destroy after the dispatcher", [this]() {
			FScopedDependencySignalListener Scoped =
				Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).End());

			Dispatcher.Reset();
			Scoped.Reset();
			TestFalse("Scoped listener invalid", Scoped.IsValid());

			// Restore a dispatcher so the shared AfterEach/BeforeEach checks still run.
			FGameplayTagContainer RootTags;
			RootTags.AddTagFast(DependencySignalTags::Root);
			RootTags.AddTagFast(FNativeGameplayTags::Get().TestRoot);
			Dispatcher = MakeShared<FDependencySignalDispatcher>(RootTags, WorldContext.Get());
		});
	});

	Describe("Reentrant callbacks", [this] {
		It("Should survive a listener that unregisters itself and reads its captures afterwards", [this]() {
			const TSharedRef<int32> CallsNum = MakeShared<int32>(0);
			const TSharedRef<FDependencySignalListenerHandle> SelfHandle =
				MakeShared<FDependencySignalListenerHandle>();

			*SelfHandle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.BindLambda([SelfHandle, CallsNum](const bool bIsValid, UObject*) {
						if (!bIsValid)
						{
							return;
						}

						SelfHandle->Unregister();

						// Captures must still be alive after the listener removed itself.
						++(*CallsNum);
						++(*CallsNum);
					})
					.End());
			TestTrue("Listener handle valid", SelfHandle->IsValid());

			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true);

			TestEqual("Callback ran to completion", *CallsNum, 2);
			TestFalse("Listener handle invalid", SelfHandle->IsValid());
			TestEqual("No one Listener in dispatcher", Dispatcher->GetListenersNum(), 0);

			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, false);
			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true);
			TestEqual("No calls after self unregistration", *CallsNum, 2);
		});

		It("Should survive a registration callback that unregisters another listener", [this]() {
			int32 FirstCallsNum = 0;
			FDependencySignalListenerHandle FirstHandle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.BindLambda([&FirstCallsNum](const bool, UObject*) {
						++FirstCallsNum;
					})
					.End());
			TestEqual("First listener notified on registration", FirstCallsNum, 1);

			FDependencySignalListenerHandle SecondHandle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.BindLambda([&FirstHandle](const bool, UObject*) {
						FirstHandle.Unregister();
					})
					.End());

			TestFalse("First listener unregistered", FirstHandle.IsValid());
			TestEqual("Only the second listener in dispatcher", Dispatcher->GetListenersNum(), 1);

			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true);
			TestEqual("First listener not notified after unregistration", FirstCallsNum, 1);

			SecondHandle.Unregister();
		});

		It("Should survive a callback that unregisters a listener notified later on the same tag", [this]() {
			int32 VictimCallsNum = 0;
			FDependencySignalListenerHandle VictimHandle;
			FDependencySignalListenerHandle KillerHandle;

			// Listeners on an entry are notified newest first, so register the victim before the killer.
			VictimHandle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.BindLambda([&VictimCallsNum](const bool bIsValid, UObject*) {
						VictimCallsNum += bIsValid ? 1 : 0;
					})
					.End());

			KillerHandle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.BindLambda([&VictimHandle](const bool bIsValid, UObject*) {
						if (bIsValid)
						{
							VictimHandle.Unregister();
						}
					})
					.End());

			Dispatcher->Validate(FNativeGameplayTags::Get().Test_A, true);

			TestEqual("Victim not notified after being unregistered", VictimCallsNum, 0);
			TestEqual("Only the killer in dispatcher", Dispatcher->GetListenersNum(), 1);

			KillerHandle.Unregister();
		});
	});
}

#endif // WITH_AUTOMATION_TESTS