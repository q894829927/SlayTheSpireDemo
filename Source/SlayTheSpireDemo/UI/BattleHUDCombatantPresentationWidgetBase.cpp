#include "BattleHUDCombatantPresentationWidgetBase.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
	// The authored Idle lasts 6.6666 seconds. Sampling 120 frames gives an
	// 18fps texture sequence instead of the visibly stepped 24-frame bake.
	constexpr int32 IroncladIdleFrameCount = 120;
	constexpr int32 IroncladHitFrameCount = 8;

	TSoftObjectPtr<UTexture2D> MakeIroncladFramePath(const TCHAR* Folder, const TCHAR* Prefix, int32 Index)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/SlayTheSpireDemo/UI/Textures/Ironclad/%s/%s_%02d.%s_%02d"),
			Folder,
			Prefix,
			Index,
			Prefix,
			Index);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(ObjectPath));
	}

	TSoftObjectPtr<UTexture2D> MakeIroncladCorpsePath()
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/SlayTheSpireDemo/UI/Textures/Ironclad/corpse.corpse")));
	}
}

int32 UBattleHUDCombatantPresentationWidgetBase::GetAnimationFrameIndex(
	float ElapsedSeconds,
	float DurationSeconds,
	int32 FrameCount,
	bool bLoop)
{
	if (FrameCount <= 0)
	{
		return INDEX_NONE;
	}

	if (DurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	const float SafeElapsed = FMath::Max(0.0f, ElapsedSeconds);
	const float NormalizedTime = bLoop
		? FMath::Fmod(SafeElapsed, DurationSeconds) / DurationSeconds
		: FMath::Clamp(SafeElapsed / DurationSeconds, 0.0f, 0.999999f);
	return FMath::Clamp(
		FMath::FloorToInt(NormalizedTime * static_cast<float>(FrameCount)),
		0,
		FrameCount - 1);
}

void UBattleHUDCombatantPresentationWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsValid(Img_Character))
	{
		Img_Character = Cast<UImage>(GetWidgetFromName(TEXT("Img_Character")));
	}

	bAnimationWidgetReady = IsValid(Img_Character);
	if (bAnimationWidgetReady)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Img_Character->GetBrush().GetResourceObject()))
		{
			FallbackCharacterTexture = Texture;
		}
		BaseCharacterTransform = Img_Character->GetRenderTransform();
		BaseCharacterOpacity = Img_Character->GetRenderOpacity();
	}

	EnsureAnimationAssetsLoaded();
	CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
	AnimationElapsedSeconds = 0.0f;
}

void UBattleHUDCombatantPresentationWidgetBase::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAnimationWidgetReady
		|| !bEnableNativeCharacterAnimation
		|| !ShouldUseNativeAnimationProfile())
	{
		return;
	}

	if (!bAnimationAssetsLoaded)
	{
		EnsureAnimationAssetsLoaded();
	}

	AnimationElapsedSeconds += FMath::Max(0.0f, InDeltaTime);

	switch (CurrentAnimation)
	{
	case EBattleHUDCombatantAnimation::Hit:
		if (AnimationElapsedSeconds >= FMath::Max(HitAnimationDuration, KINDA_SMALL_NUMBER))
		{
			CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
			AnimationElapsedSeconds = 0.0f;
		}
		break;
	case EBattleHUDCombatantAnimation::Attack:
		if (AnimationElapsedSeconds >= FMath::Max(AttackAnimationDuration, KINDA_SMALL_NUMBER))
		{
			// Attack reuses the authored Idle frames. Carry the source timeline
			// into Idle so returning from the lunge does not snap back to frame 0.
			AnimationElapsedSeconds = FMath::Fmod(
				AnimationElapsedSeconds,
				FMath::Max(IdleAnimationDuration, KINDA_SMALL_NUMBER));
			CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
		}
		break;
	default:
		break;
	}

	ApplyAnimationFrame();
}

void UBattleHUDCombatantPresentationWidgetBase::PlayCombatantAnimation(
	EBattleHUDCombatantAnimation Animation)
{
	CurrentAnimation = Animation;
	AnimationElapsedSeconds = 0.0f;

	if (!bEnableNativeCharacterAnimation || !ShouldUseNativeAnimationProfile())
	{
		return;
	}

	if (!bAnimationAssetsLoaded)
	{
		EnsureAnimationAssetsLoaded();
	}
	ApplyAnimationFrame();
}

void UBattleHUDCombatantPresentationWidgetBase::StopCombatantAnimation()
{
	CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
	AnimationElapsedSeconds = 0.0f;

	if (!bAnimationWidgetReady)
	{
		return;
	}

	ApplyCharacterTexture(FallbackCharacterTexture);
	Img_Character->SetRenderTransform(BaseCharacterTransform);
	Img_Character->SetRenderOpacity(BaseCharacterOpacity);
}

void UBattleHUDCombatantPresentationWidgetBase::EnsureAnimationAssetsLoaded()
{
	if (bAnimationAssetsLoaded)
	{
		return;
	}

	if (IdleAnimationFrames.IsEmpty())
	{
		IdleAnimationFrames.Reserve(IroncladIdleFrameCount);
		for (int32 Index = 0; Index < IroncladIdleFrameCount; ++Index)
		{
			IdleAnimationFrames.Add(MakeIroncladFramePath(TEXT("Idle"), TEXT("idle"), Index));
		}
	}
	if (HitAnimationFrames.IsEmpty())
	{
		HitAnimationFrames.Reserve(IroncladHitFrameCount);
		for (int32 Index = 0; Index < IroncladHitFrameCount; ++Index)
		{
			HitAnimationFrames.Add(MakeIroncladFramePath(TEXT("Hit"), TEXT("hit"), Index));
		}
	}
	if (CorpseTexture.IsNull())
	{
		CorpseTexture = MakeIroncladCorpsePath();
	}
	if (!IsValid(FallbackCharacterTexture))
	{
		FallbackCharacterTexture = Cast<UTexture2D>(StaticLoadObject(
			UTexture2D::StaticClass(),
			nullptr,
			TEXT("/Game/SlayTheSpireDemo/UI/Textures/T_Ironclad_Static.T_Ironclad_Static")));
	}

	for (TSoftObjectPtr<UTexture2D>& Frame : IdleAnimationFrames)
	{
		if (!Frame.IsValid())
		{
			Frame.LoadSynchronous();
		}
	}
	for (TSoftObjectPtr<UTexture2D>& Frame : HitAnimationFrames)
	{
		if (!Frame.IsValid())
		{
			Frame.LoadSynchronous();
		}
	}
	if (!CorpseTexture.IsValid())
	{
		CorpseTexture.LoadSynchronous();
	}

	bAnimationAssetsLoaded = true;
}

bool UBattleHUDCombatantPresentationWidgetBase::ShouldUseNativeAnimationProfile() const
{
	return CombatantView.bPlayer || bAnimateEnemyCharacter;
}

TArray<TSoftObjectPtr<UTexture2D>>&
UBattleHUDCombatantPresentationWidgetBase::GetFramesForAnimation(
	EBattleHUDCombatantAnimation Animation)
{
	return Animation == EBattleHUDCombatantAnimation::Hit
		? HitAnimationFrames
		: IdleAnimationFrames;
}

const TArray<TSoftObjectPtr<UTexture2D>>&
UBattleHUDCombatantPresentationWidgetBase::GetFramesForAnimation(
	EBattleHUDCombatantAnimation Animation) const
{
	return Animation == EBattleHUDCombatantAnimation::Hit
		? HitAnimationFrames
		: IdleAnimationFrames;
}

void UBattleHUDCombatantPresentationWidgetBase::ApplyAnimationFrame()
{
	if (!bAnimationWidgetReady)
	{
		return;
	}

	if (CurrentAnimation == EBattleHUDCombatantAnimation::Defeat
		|| CurrentAnimation == EBattleHUDCombatantAnimation::Death)
	{
		ApplyCharacterTexture(CorpseTexture.Get());
		ApplyCharacterTransform(0.0f, 0.98f, -8.0f);
		Img_Character->SetRenderOpacity(FMath::Clamp(DefeatOpacity, 0.0f, 1.0f));
		return;
	}

	const TArray<TSoftObjectPtr<UTexture2D>>& Frames = GetFramesForAnimation(CurrentAnimation);
	const bool bLoop = CurrentAnimation == EBattleHUDCombatantAnimation::Idle
		|| CurrentAnimation == EBattleHUDCombatantAnimation::Victory;
	// Attack has no authored frame set, so it uses Idle frames while the
	// separate AttackAnimationDuration controls only the lunge transform.
	// Keeping the Idle source cadence prevents the attack from skipping most
	// of the 120-frame sequence in a 0.30 second window.
	const float FrameDuration = CurrentAnimation == EBattleHUDCombatantAnimation::Hit
		? HitAnimationDuration
		: IdleAnimationDuration;
	const int32 FrameIndex = GetAnimationFrameIndex(
		AnimationElapsedSeconds,
		FrameDuration,
		Frames.Num(),
		bLoop);
	if (FrameIndex != INDEX_NONE && Frames.IsValidIndex(FrameIndex))
	{
		ApplyCharacterTexture(Frames[FrameIndex].Get());
	}
	else
	{
		ApplyCharacterTexture(FallbackCharacterTexture);
	}

	float TranslationAlpha = 0.0f;
	float ScaleMultiplier = 1.0f;
	if (CurrentAnimation == EBattleHUDCombatantAnimation::Attack)
	{
		const float Progress = FMath::Clamp(
			AnimationElapsedSeconds / FMath::Max(AttackAnimationDuration, KINDA_SMALL_NUMBER),
			0.0f,
			1.0f);
		TranslationAlpha = Progress < 0.5f ? Progress * 2.0f : (1.0f - Progress) * 2.0f;
	}
	else if (CurrentAnimation == EBattleHUDCombatantAnimation::Victory)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(AnimationElapsedSeconds * 5.0f);
		ScaleMultiplier = FMath::Lerp(1.0f, FMath::Max(1.0f, VictoryPulseScale), Pulse);
	}

	ApplyCharacterTransform(TranslationAlpha, ScaleMultiplier, 0.0f);
	Img_Character->SetRenderOpacity(BaseCharacterOpacity);
}

void UBattleHUDCombatantPresentationWidgetBase::ApplyCharacterTexture(UTexture2D* Texture)
{
	if (IsValid(Img_Character) && IsValid(Texture))
	{
		Img_Character->SetBrushFromTexture(Texture, false);
	}
}

void UBattleHUDCombatantPresentationWidgetBase::ApplyCharacterTransform(
	float TranslationAlpha,
	float ScaleMultiplier,
	float Angle)
{
	if (!IsValid(Img_Character))
	{
		return;
	}

	FWidgetTransform Transform = BaseCharacterTransform;
	Transform.Translation += AttackTranslation * TranslationAlpha;
	Transform.Scale *= ScaleMultiplier;
	Transform.Angle += Angle;
	Img_Character->SetRenderTransform(Transform);
}

void UBattleHUDCombatantPresentationWidgetBase::SetPresentationData(
	const FBattleHUDCombatantView& InCombatantView,
	bool bInTargetSelectionActive,
	bool bInLegalTarget,
	int32 InTargetId,
	bool bInTargetHighlighted
)
{
	CombatantView = InCombatantView;
	bTargetSelectionActive = bInTargetSelectionActive;
	bLegalTarget = bInTargetSelectionActive && bInLegalTarget && InTargetId != INDEX_NONE;
	TargetId = bLegalTarget ? InTargetId : INDEX_NONE;
	bTargetHighlighted = bLegalTarget || bInTargetHighlighted;

	BP_OnPresentationChanged();
	PublishTransientPreviewState();

	// Hover/focus may remain stationary while a new StateRevision arrives.
	// Republish the latest coherent View so the active inspector cannot go stale.
	if (IsTransientInspectionActive())
	{
		OnInspectRequested.Broadcast(this);
	}
}

void UBattleHUDCombatantPresentationWidgetBase::SetPointerInspectionActive(bool bActive)
{
	const bool bWasActive = IsTransientInspectionActive();
	bPointerInspectionActive = bActive;
	PublishTransientInspectionState(bWasActive);
	PublishTransientPreviewState();
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestPinnedInspection()
{
	if (bTargetSelectionActive)
	{
		return false;
	}

	OnInspectPinRequested.Broadcast(this);
	return true;
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestPrimaryInteraction()
{
	return bTargetSelectionActive && RequestLegalTarget();
}

bool UBattleHUDCombatantPresentationWidgetBase::IsTransientInspectionActive() const
{
	return bPointerInspectionActive || bFocusInspectionActive;
}

void UBattleHUDCombatantPresentationWidgetBase::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	SetFocusInspectionActive(true);
}

void UBattleHUDCombatantPresentationWidgetBase::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);
	SetFocusInspectionActive(false);
}

void UBattleHUDCombatantPresentationWidgetBase::NativeDestruct()
{
	StopCombatantAnimation();
	const bool bWasActive = IsTransientInspectionActive();
	bPointerInspectionActive = false;
	bFocusInspectionActive = false;
	PublishTransientInspectionState(bWasActive);
	PublishTransientPreviewState();
	Super::NativeDestruct();
}

void UBattleHUDCombatantPresentationWidgetBase::SetFocusInspectionActive(bool bActive)
{
	const bool bWasActive = IsTransientInspectionActive();
	bFocusInspectionActive = bActive;
	PublishTransientInspectionState(bWasActive);
	PublishTransientPreviewState();
}

void UBattleHUDCombatantPresentationWidgetBase::PublishTransientInspectionState(bool bWasActive)
{
	const bool bIsActive = IsTransientInspectionActive();
	if (bIsActive == bWasActive)
	{
		return;
	}

	if (bIsActive)
	{
		OnInspectRequested.Broadcast(this);
	}
	else
	{
		OnInspectCleared.Broadcast(this);
	}
}

void UBattleHUDCombatantPresentationWidgetBase::PublishTransientPreviewState()
{
	const int32 DesiredPreviewTargetId =
		IsTransientInspectionActive()
		&& bTargetSelectionActive
		&& bLegalTarget
		&& TargetId != INDEX_NONE
			? TargetId
			: INDEX_NONE;

	if (DesiredPreviewTargetId == PublishedPreviewTargetId)
	{
		return;
	}

	if (PublishedPreviewTargetId != INDEX_NONE)
	{
		PublishedPreviewTargetId = INDEX_NONE;
		OnPreviewCleared.Broadcast();
	}

	if (DesiredPreviewTargetId != INDEX_NONE)
	{
		PublishedPreviewTargetId = DesiredPreviewTargetId;
		OnPreviewRequested.Broadcast(DesiredPreviewTargetId);
	}
}

void UBattleHUDCombatantPresentationWidgetBase::ClearTransientInspection()
{
	const bool bWasActive = IsTransientInspectionActive();
	bPointerInspectionActive = false;
	bFocusInspectionActive = false;

	if (bWasActive)
	{
		OnInspectCleared.Broadcast(this);
	}
	PublishTransientPreviewState();
}

bool UBattleHUDCombatantPresentationWidgetBase::RequestLegalTarget()
{
	if (!bTargetSelectionActive || !bLegalTarget || TargetId == INDEX_NONE)
	{
		return false;
	}

	const int32 RequestedTargetId = TargetId;

	// A committed target choice ends current transient inspection and Preview.
	// Clearing both before the synchronous target request guarantees the A3
	// surface is gone before authoritative RequestPlayCard/A2 playback begins.
	ClearTransientInspection();
	OnTargetRequested.Broadcast(RequestedTargetId);
	return true;
}
