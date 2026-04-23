// Copyright Bohdan Sofronii. All Rights Reserved.

#include "DependencySignalTags.h"

namespace DependencySignalTags
{
	UE_DEFINE_GAMEPLAY_TAG(Root, "DependencySignal");

	UE_DEFINE_GAMEPLAY_TAG(Controller, "DependencySignal.Controller");
	UE_DEFINE_GAMEPLAY_TAG(Controller_Pawn, "DependencySignal.Controller.Pawn");
	UE_DEFINE_GAMEPLAY_TAG(Controller_PlayerState, "DependencySignal.Controller.PlayerState");
} // namespace DependencySignalTags