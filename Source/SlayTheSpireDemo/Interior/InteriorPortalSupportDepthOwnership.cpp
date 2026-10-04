#include "InteriorPortalSupportDepthOwnership.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
	IConsoleVariable* CustomDepthMode()
	{
		static IConsoleVariable* Mode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
		return Mode;
	}
	IConsoleVariable* CustomDepthJitter()
	{
		static IConsoleVariable* Jitter = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepthTemporalAAJitter"));
		return Jitter;
	}
}

FInteriorPortalSupportDepthOwnership::~FInteriorPortalSupportDepthOwnership() { Reset(); }

bool FInteriorPortalSupportDepthOwnership::Matches(const FBinding& Binding)
{
	const UPrimitiveComponent* Support = Binding.Support.Get();
	return IsValid(Support) && Support->bRenderCustomDepth
		&& Support->CustomDepthStencilValue == Binding.Stencil
		&& Support->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_Default;
}

void FInteriorPortalSupportDepthOwnership::Release(const FBinding& Binding)
{
	// A later owner may have changed this component; never overwrite its state.
	if (Binding.bOwnsComponentState && Matches(Binding))
	{
		UPrimitiveComponent* Support = Binding.Support.Get();
		Support->SetRenderCustomDepth(false);
		Support->SetCustomDepthStencilValue(Binding.PreviousStencil);
		Support->SetCustomDepthStencilWriteMask(Binding.PreviousWriteMask);
	}
}

void FInteriorPortalSupportDepthOwnership::RestoreDepthMode()
{
	if (IConsoleVariable* Mode = CustomDepthMode(); bOwnsDepthMode && Mode && Mode->GetInt() == 3
		&& uint32(Mode->GetFlags() & ECVF_SetByMask) == DepthModePriority)
	{
		// Preserve the original console priority as well as its value.
		Mode->Set(PreviousDepthMode, EConsoleVariableFlags(DepthModePriority));
	}
	bOwnsDepthMode = false;
	if (IConsoleVariable* Jitter = CustomDepthJitter(); bOwnsDepthJitter && Jitter && Jitter->GetInt() == 1
		&& uint32(Jitter->GetFlags() & ECVF_SetByMask) == DepthJitterPriority)
	{
		Jitter->Set(PreviousDepthJitter, EConsoleVariableFlags(DepthJitterPriority));
	}
	bOwnsDepthJitter = false;
}

void FInteriorPortalSupportDepthOwnership::Reset()
{
	for (const FBinding& Binding : Bindings) { Release(Binding); }
	Bindings.Reset(); ActiveWorld.Reset(); RestoreDepthMode();
}

void FInteriorPortalSupportDepthOwnership::Refresh(UWorld* World,
	UPrimitiveComponent* Blue, UPrimitiveComponent* Orange)
{
	check(IsInGameThread());
	if (!IsValid(World)) { Reset(); return; }
	if (ActiveWorld.Get() != World) { Start(World); }
	TArray<UPrimitiveComponent*, TInlineAllocator<2>> Desired;
	for (UPrimitiveComponent* Support : {Blue, Orange})
	{
		if (IsValid(Support) && Support->GetWorld() == World) { Desired.AddUnique(Support); }
	}
	for (int32 Index = Bindings.Num() - 1; Index >= 0; --Index)
	{
		if (!Desired.Contains(Bindings[Index].Support.Get()))
		{
			Release(Bindings[Index]); Bindings.RemoveAt(Index);
		}
	}
	if (Desired.IsEmpty()) { return; }
	IConsoleVariable* Mode = CustomDepthMode();
	if (!Mode) { return; }
	if (Mode->GetInt() != 3) { return; } // External changes revoke the lease.
	IConsoleVariable* Jitter = CustomDepthJitter();
	if (!Jitter) { return; }
	if (Jitter->GetInt() != 1) { return; }
	// Validate active leases too: a newly spawned outline/custom-depth user may
	// introduce an alias after allocation. It must never acquire our aperture.
	int32 Used[256] = {};
	for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
	{
		if (It->bRenderCustomDepth && It->GetWorld() == World)
		{
			++Used[FMath::Clamp(It->CustomDepthStencilValue, 0, 255)];
		}
	}
	for (FBinding& Binding : Bindings) { Binding.bIdentityUnique = Used[Binding.Stencil] == 1; }
	for (UPrimitiveComponent* Support : Desired)
	{
		if (Bindings.ContainsByPredicate([Support](const FBinding& B) { return B.Support.Get() == Support; }))
		{
			continue; // Stable identity, including a lease revoked by another owner.
		}
		// Counts, not discovery order, determine conflicts. Allocate only when
		// the attachment changes, selecting the smallest unused byte.
		FBinding Binding;
		Binding.Support = Support;
		Binding.PreviousStencil = Support->CustomDepthStencilValue;
		Binding.PreviousWriteMask = Support->CustomDepthStencilWriteMask;
		if (Support->bRenderCustomDepth)
		{
			const int32 Existing = Support->CustomDepthStencilValue;
			if (Existing <= 0 || Existing > 255 || Used[Existing] != 1
				|| Support->CustomDepthStencilWriteMask != ERendererStencilMask::ERSM_Default) { continue; }
			Binding.Stencil = Existing; // Reuse without changing its existing owner.
		}
		else
		{
			for (int32 Id = 1; Id <= 255; ++Id) { if (Used[Id] == 0) { Binding.Stencil = Id; break; } }
			if (Binding.Stencil == 0) { continue; }
			Binding.bOwnsComponentState = true;
			Support->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
			Support->SetCustomDepthStencilValue(Binding.Stencil);
			Support->SetRenderCustomDepth(true);
			++Used[Binding.Stencil];
		}
		Bindings.Add(Binding);
	}
}

void FInteriorPortalSupportDepthOwnership::Start(UWorld* World)
{
	check(IsInGameThread());
	Reset();
	if (!IsValid(World)) { return; }
	ActiveWorld = World;
	if (IConsoleVariable* Mode = CustomDepthMode(); Mode && Mode->GetInt() != 3)
	{
		PreviousDepthMode = Mode->GetInt();
		DepthModePriority = uint32(Mode->GetFlags() & ECVF_SetByMask);
		// UE's notification recreates cached mesh commands. Pay this cost once
		// at backend startup/stop, NEVER each time a portal moves or is cleared.
		// SetWithCurrentPriority implicitly promotes constructor defaults in UE
		// 5.8. Explicit equal priority preserves those defaults and still notifies.
		Mode->Set(3, EConsoleVariableFlags(DepthModePriority));
		bOwnsDepthMode = true;
	}
	if (IConsoleVariable* Jitter = CustomDepthJitter(); Jitter && Jitter->GetInt() != 1)
	{
		PreviousDepthJitter = Jitter->GetInt();
		DepthJitterPriority = uint32(Jitter->GetFlags() & ECVF_SetByMask);
		Jitter->Set(1, EConsoleVariableFlags(DepthJitterPriority));
		bOwnsDepthJitter = true;
	}
}

uint32 FInteriorPortalSupportDepthOwnership::GetStencil(const UPrimitiveComponent* Support) const
{
	const IConsoleVariable* Mode = CustomDepthMode();
	const IConsoleVariable* Jitter = CustomDepthJitter();
	if (!Mode || Mode->GetInt() != 3 || !Jitter || Jitter->GetInt() != 1) { return 0; }
	for (const FBinding& Binding : Bindings)
	{
		if (Binding.Support.Get() == Support && Binding.bIdentityUnique && Matches(Binding))
		{
			return uint32(Binding.Stencil);
		}
	}
	return 0;
}
