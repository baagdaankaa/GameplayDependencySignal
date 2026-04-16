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
				Handle = Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A));
			});

			It("Should be VALID with two tags using single tag", [this]() {
				AddInfo("Register dependency listener");
				Handle = Dispatcher->Register(FDependencySignalRequest()
						.AddTag(FNativeGameplayTags::Get().Test_A)
						.AddTag(FNativeGameplayTags::Get().Test_B));
			});

			It("Should be VALID with two tags using tag container", [this]() {
				FGameplayTagContainer Container;
				Container.AddTag(FNativeGameplayTags::Get().Test_A);
				Container.AddTag(FNativeGameplayTags::Get().Test_B);

				AddInfo("Register dependency listener");
				Handle = Dispatcher->Register(FDependencySignalRequest().AddTags(Container));
			});
		});

		Describe("After unregistration", [this] {
			BeforeEach([this]() {
				AddInfo("Register dependency listener");
				Handle = Dispatcher->Register(FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A));
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
					.AddTag(FNativeGameplayTags::Get().Test_B));
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
			Handle = Dispatcher->Register(
				FDependencySignalRequest().AddTag(FNativeGameplayTags::Get().Test_A).AddSource(SourceObject.Get()));
		});

		It("Should be VALID registration with two tags", [this]() {
			AddInfo("Register dependency listener");
			Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.AddTag(FNativeGameplayTags::Get().Test_B)
					.AddSource(SourceObject.Get()));
		});

		It("Should be VALID registration with three tags", [this]() {
			AddInfo("Register dependency listener");
			Handle = Dispatcher->Register(FDependencySignalRequest()
					.AddTag(FNativeGameplayTags::Get().Test_A)
					.AddTag(FNativeGameplayTags::Get().Test_B)
					.AddTag(FNativeGameplayTags::Get().Test_C)
					.AddSource(SourceObject.Get()));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS