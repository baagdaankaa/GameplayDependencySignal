// Copyright Bohdan Sofronii. All Rights Reserved.

#pragma once

/**
 * Primary template for dependency type traits — left undefined to catch missing specializations.
 *
 * TDependencySignalTraits maps a compile-time type tag to a runtime FGameplayTag and
 * defines the expected return type of the resolved dependency. This enables
 * type-safe, IDE-friendly dependency resolution without magic strings or manual casts.
 *
 * To define a new dependency type, use the DECLARE_DEPENDENCY_SIGNAL_TRAIT macro:
 * @code
 *   DECLARE_DEPENDENCY_SIGNAL_TRAIT(FDTDefaultController, DependencySignalTags::Controller, AController*)
 * @endcode
 *
 * Or specialize manually:
 * @code
 *   template<>
 *   struct TDependencySignalTraits<FMyCustomDependency>
 *   {
 *       static const FGameplayTag& GetTag()
 *       {
 *           static FGameplayTag InTag = DependencySignalTags::SomeTag;
 *           return InTag;
 *       }
 *       using ReturnType = UMyComponent*;
 *   };
 * @endcode
 *
 * @see DECLARE_DEPENDENCY_SIGNAL_TRAIT, FDependencySignalProviderResolver::GetDependency<DependencyType>()
 */
template <typename T>
struct TDependencySignalTraits;

// Helper trait to detect if TDependencySignalTraits<T> is properly specialized
template <typename T, typename = void>
struct THasDependencySignalTraits : TIntegralConstant<bool, false>
{
};

template <typename T>
struct THasDependencySignalTraits<T,
	std::void_t<typename TDependencySignalTraits<T>::ReturnType, decltype(TDependencySignalTraits<T>::GetTag())>> :
	TIntegralConstant<bool, true>
{
};

/**
 * Macro to declare a new dependency type trait with a single line.
 *
 * This generates:
 * 1. An empty tag struct (used as the template parameter)
 * 2. A TDependencySignalTraits specialization that binds the tag to a FGameplayTag and return type
 *
 * @param NewTypeName   Name for the new dependency tag struct (e.g., FDTDefaultController)
 * @param Tag           The FGameplayTag this type resolves to (e.g., DependencySignalTags::Controller)
 * @param ReturnT       The expected C++ return type (e.g., AController*, APawn*)
 *
 * Example:
 * @code
 *   DECLARE_DEPENDENCY_SIGNAL_TRAIT(FDTDefaultPawn, DependencySignalTags::Controller_Pawn, APawn*)
 *
 *   // Usage:
 *   APawn* Pawn = Provider.GetDependency<FDTDefaultPawn>(SourceObject);
 * @endcode
 */
#define DECLARE_DEPENDENCY_SIGNAL_TRAIT(NewTypeName, Tag, ReturnT)            \
	struct NewTypeName                                                        \
	{                                                                         \
	};                                                                        \
                                                                              \
	template <>                                                               \
	struct TDependencySignalTraits<NewTypeName>                               \
	{                                                                         \
		/** Returns the gameplay tag associated with this dependency type. */ \
		static const FGameplayTag& GetTag()                                   \
		{                                                                     \
			static FGameplayTag InTag = Tag;                                  \
			return InTag;                                                     \
		}                                                                     \
		/** The C++ type returned by the dependency resolver. */              \
		using ReturnType = ReturnT;                                           \
	};