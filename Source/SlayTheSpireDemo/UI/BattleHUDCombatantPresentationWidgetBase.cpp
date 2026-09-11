#include "BattleHUDCombatantPresentationWidgetBase.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
	// The authored Idle lasts 6.6666 seconds. Sampling 120 frames gives an
	// 18fps texture sequence instead of the visibly stepped 24-frame bake.
	constexpr int32 IroncladIdleFrameCount = 120;
	constexpr int32 IroncladHitFrameCount = 8;
	constexpr int32 AwakenedOneIdleFrameCount = 48;
	constexpr int32 AwakenedOneHitFrameCount = 8;
	constexpr int32 AwakenedOneAttackFrameCount = 24;

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

	TSoftObjectPtr<UTexture2D> MakeAwakenedOneFramePath(const TCHAR* Folder, const TCHAR* Prefix, int32 Index)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/SlayTheSpireDemo/UI/Textures/AwakenedOne/%s/%s_%02d.%s_%02d"),
			Folder,
			Prefix,
			Index,
			Prefix,
			Index);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(ObjectPath));
	}

	TSoftObjectPtr<UTexture2D> MakeAwakenedOneStaticPath()
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			TEXT("/Game/SlayTheSpireDemo/UI/Textures/T_AwakenedOne_Static.T_AwakenedOne_Static")));
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

	if (!bAnimationAssetsLoaded && !bAnimationAssetsLoadAttempted)
	{
		EnsureAnimationAssetsLoaded();
	}

	AnimationElapsedSeconds += FMath::Max(0.0f, InDeltaTime);

	switch (CurrentAnimation)
	{
	case EBattleHUDCombatantAnimation::Hit:
		if (AnimationElapsedSeconds >= FMath::Max(GetAnimationDuration(CurrentAnimation), KINDA_SMALL_NUMBER))
		{
			CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
			AnimationElapsedSeconds = 0.0f;
		}
		break;
	case EBattleHUDCombatantAnimation::Attack:
		if (AnimationElapsedSeconds >= FMath::Max(GetAnimationDuration(CurrentAnimation), KINDA_SMALL_NUMBER))
		{
			if (IsUsingEnemyAnimationProfile() && !EnemyAttackAnimationFrames.IsEmpty())
			{
				CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
				AnimationElapsedSeconds = 0.0f;
				break;
			}

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

	if (!bAnimationAssetsLoaded && !bAnimationAssetsLoadAttempted)
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

UTexture2D* UBattleHUDCombatantPresentationWidgetBase::ResolveAnimationTexture(
	TSoftObjectPtr<UTexture2D>& Texture)
{
	if (!Texture.IsValid())
	{
		Texture.LoadSynchronous();
	}
	return Texture.Get();
}

UTexture2D* UBattleHUDCombatantPresentationWidgetBase::ResolveAnimatedFallbackTexture()
{
	if (IsValid(LastAppliedAnimationFrame))
	{
		return LastAppliedAnimationFrame.Get();
	}

	TArray<TSoftObjectPtr<UTexture2D>>& IdleFrames =
		GetFramesForAnimation(EBattleHUDCombatantAnimation::Idle);
	for (int32 Index = IdleFrames.Num() - 1; Index >= 0; --Index)
	{
		if (UTexture2D* Texture = ResolveAnimationTexture(IdleFrames[Index]))
		{
			LastAppliedAnimationFrame = Texture;
			return Texture;
		}
	}

	return nullptr;
}

void UBattleHUDCombatantPresentationWidgetBase::EnsureAnimationAssetsLoaded()
{
	if (bAnimationAssetsLoaded || bAnimationAssetsLoadAttempted)
	{
		return;
	}
	bAnimationAssetsLoadAttempted = true;

	int32 FailedAssetCount = 0;
	FString FirstFailedAssetPath;
	auto LoadAsset = [this, &FailedAssetCount, &FirstFailedAssetPath](TSoftObjectPtr<UTexture2D>& Asset)
	{
		if (IsValid(ResolveAnimationTexture(Asset)))
		{
			LoadedAnimationTextures.AddUnique(Asset.Get());
			return;
		}

		++FailedAssetCount;
		if (FirstFailedAssetPath.IsEmpty())
		{
			FirstFailedAssetPath = Asset.ToSoftObjectPath().ToString();
		}
	};
	auto LoadAssets = [&LoadAsset](TArray<TSoftObjectPtr<UTexture2D>>& Assets)
	{
		for (TSoftObjectPtr<UTexture2D>& Asset : Assets)
		{
			LoadAsset(Asset);
		}
	};

	if (IsUsingEnemyAnimationProfile())
	{
		if (EnemyIdleAnimationFrames.IsEmpty())
		{
			EnemyIdleAnimationFrames.Reserve(AwakenedOneIdleFrameCount);
			for (int32 Index = 0; Index < AwakenedOneIdleFrameCount; ++Index)
			{
				// Idle_2 is the authored breathing/tail motion. Idle_1 is a
				// near-static combat stance and reads as a frozen sprite in-game.
				EnemyIdleAnimationFrames.Add(MakeAwakenedOneFramePath(TEXT("Idle"), TEXT("idle_2"), Index));
			}
		}
		if (EnemyHitAnimationFrames.IsEmpty())
		{
			EnemyHitAnimationFrames.Reserve(AwakenedOneHitFrameCount);
			for (int32 Index = 0; Index < AwakenedOneHitFrameCount; ++Index)
			{
				EnemyHitAnimationFrames.Add(MakeAwakenedOneFramePath(TEXT("Hit"), TEXT("hit"), Index));
			}
		}
		if (EnemyAttackAnimationFrames.IsEmpty())
		{
			EnemyAttackAnimationFrames.Reserve(AwakenedOneAttackFrameCount);
			for (int32 Index = 0; Index < AwakenedOneAttackFrameCount; ++Index)
			{
				EnemyAttackAnimationFrames.Add(MakeAwakenedOneFramePath(TEXT("Attack"), TEXT("attack_1"), Index));
			}
		}
		if (!IsValid(FallbackCharacterTexture))
		{
			FallbackCharacterTexture = Cast<UTexture2D>(StaticLoadObject(
				UTexture2D::StaticClass(),
				nullptr,
				TEXT("/Game/SlayTheSpireDemo/UI/Textures/T_AwakenedOne_Static.T_AwakenedOne_Static")));
		}

		LoadAssets(EnemyIdleAnimationFrames);
		LoadAssets(EnemyHitAnimationFrames);
		LoadAssets(EnemyAttackAnimationFrames);
		// EnemyCorpseTexture is an optional override. An unset override is
		// intentional because the terminal state can keep the last animation frame.
		if (!EnemyCorpseTexture.IsNull())
		{
			LoadAsset(EnemyCorpseTexture);
		}

		bAnimationAssetsLoaded = FailedAssetCount == 0;
		if (!bAnimationAssetsLoaded)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[BattleHUD] Failed to load %d Awakened One animation assets; first missing asset: %s. Check MapsToCook/DirectoriesToAlwaysCook."),
				FailedAssetCount,
				*FirstFailedAssetPath);
		}
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

	LoadAssets(IdleAnimationFrames);
	LoadAssets(HitAnimationFrames);
	LoadAsset(CorpseTexture);

	bAnimationAssetsLoaded = FailedAssetCount == 0;
	if (!bAnimationAssetsLoaded)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[BattleHUD] Failed to load %d Ironclad animation assets; first missing asset: %s. Check MapsToCook/DirectoriesToAlwaysCook."),
			FailedAssetCount,
			*FirstFailedAssetPath);
	}
}

bool UBattleHUDCombatantPresentationWidgetBase::ShouldUseNativeAnimationProfile() const
{
	return CombatantView.bPlayer || bAnimateEnemyCharacter;
}

bool UBattleHUDCombatantPresentationWidgetBase::IsUsingEnemyAnimationProfile() const
{
	return !CombatantView.bPlayer && bAnimateEnemyCharacter;
}

float UBattleHUDCombatantPresentationWidgetBase::GetAnimationDuration(
	EBattleHUDCombatantAnimation Animation) const
{
	if (IsUsingEnemyAnimationProfile())
	{
		switch (Animation)
		{
		case EBattleHUDCombatantAnimation::Hit:
			return EnemyHitAnimationDuration;
		case EBattleHUDCombatantAnimation::Attack:
			return EnemyAttackAnimationDuration;
		default:
			return EnemyIdleAnimationDuration;
		}
	}

	return Animation == EBattleHUDCombatantAnimation::Hit
		? HitAnimationDuration
		: Animation == EBattleHUDCombatantAnimation::Attack
			? AttackAnimationDuration
			: IdleAnimationDuration;
}

TArray<TSoftObjectPtr<UTexture2D>>&
UBattleHUDCombatantPresentationWidgetBase::GetFramesForAnimation(
	EBattleHUDCombatantAnimation Animation)
{
	if (IsUsingEnemyAnimationProfile())
	{
		if (Animation == EBattleHUDCombatantAnimation::Hit)
		{
			return EnemyHitAnimationFrames;
		}
		if (Animation == EBattleHUDCombatantAnimation::Attack
			&& !EnemyAttackAnimationFrames.IsEmpty())
		{
			return EnemyAttackAnimationFrames;
		}
		return EnemyIdleAnimationFrames;
	}

	return Animation == EBattleHUDCombatantAnimation::Hit ? HitAnimationFrames : IdleAnimationFrames;
}

const TArray<TSoftObjectPtr<UTexture2D>>&
UBattleHUDCombatantPresentationWidgetBase::GetFramesForAnimation(
	EBattleHUDCombatantAnimation Animation) const
{
	if (IsUsingEnemyAnimationProfile())
	{
		if (Animation == EBattleHUDCombatantAnimation::Hit)
		{
			return EnemyHitAnimationFrames;
		}
		if (Animation == EBattleHUDCombatantAnimation::Attack
			&& !EnemyAttackAnimationFrames.IsEmpty())
		{
			return EnemyAttackAnimationFrames;
		}
		return EnemyIdleAnimationFrames;
	}

	return Animation == EBattleHUDCombatantAnimation::Hit ? HitAnimationFrames : IdleAnimationFrames;
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
		UTexture2D* DeathTexture = IsUsingEnemyAnimationProfile()
			? (EnemyCorpseTexture.IsNull() ? nullptr : ResolveAnimationTexture(EnemyCorpseTexture))
			: ResolveAnimationTexture(CorpseTexture);

		if (IsUsingEnemyAnimationProfile() && !IsValid(DeathTexture))
		{
			DeathTexture = ResolveAnimatedFallbackTexture();
		}

		if (!IsValid(DeathTexture))
		{
			DeathTexture = FallbackCharacterTexture.Get();
		}
		ApplyCharacterTexture(DeathTexture);
		ApplyCharacterTransform(0.0f, 0.98f, -8.0f);
		Img_Character->SetRenderOpacity(FMath::Clamp(DefeatOpacity, 0.0f, 1.0f));
		return;
	}

	TArray<TSoftObjectPtr<UTexture2D>>& Frames = GetFramesForAnimation(CurrentAnimation);
	const bool bLoop = CurrentAnimation == EBattleHUDCombatantAnimation::Idle
		|| CurrentAnimation == EBattleHUDCombatantAnimation::Victory;
	// Player Attack keeps the existing lunge over the authored Idle cadence.
	// Enemy Attack uses the authored Awakened One Attack_1 sequence directly.
	const bool bUseEnemyAttackFrames =
		IsUsingEnemyAnimationProfile()
		&& CurrentAnimation == EBattleHUDCombatantAnimation::Attack
		&& !EnemyAttackAnimationFrames.IsEmpty();
	const float FrameDuration = bUseEnemyAttackFrames
		? EnemyAttackAnimationDuration
		: CurrentAnimation == EBattleHUDCombatantAnimation::Hit
			? (IsUsingEnemyAnimationProfile() ? EnemyHitAnimationDuration : HitAnimationDuration)
			: (IsUsingEnemyAnimationProfile() ? EnemyIdleAnimationDuration : IdleAnimationDuration);
	const int32 FrameIndex = GetAnimationFrameIndex(
		AnimationElapsedSeconds,
		FrameDuration,
		Frames.Num(),
		bLoop);
	UTexture2D* FrameTexture = nullptr;
	if (FrameIndex != INDEX_NONE && Frames.IsValidIndex(FrameIndex))
	{
		FrameTexture = ResolveAnimationTexture(Frames[FrameIndex]);
	}
	if (IsValid(FrameTexture))
	{
		LastAppliedAnimationFrame = FrameTexture;
		ApplyCharacterTexture(LastAppliedAnimationFrame.Get());
	}
	else
	{
		UTexture2D* AnimatedFallback = IsUsingEnemyAnimationProfile()
			? ResolveAnimatedFallbackTexture()
			: nullptr;
		ApplyCharacterTexture(IsValid(AnimatedFallback) ? AnimatedFallback : FallbackCharacterTexture.Get());
	}

	float TranslationAlpha = 0.0f;
	float ScaleMultiplier = 1.0f;
	if (CurrentAnimation == EBattleHUDCombatantAnimation::Attack && !bUseEnemyAttackFrames)
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
	const bool bProfileChanged = CombatantView.bPlayer != InCombatantView.bPlayer;
	const bool bCombatantChanged =
		CombatantView.PresentationId != InCombatantView.PresentationId
		|| CombatantView.bDead != InCombatantView.bDead;
	const bool bNeedEnemyProfileLoad =
		!InCombatantView.bPlayer
		&& bAnimateEnemyCharacter
		&& EnemyIdleAnimationFrames.IsEmpty();
	CombatantView = InCombatantView;
	bTargetSelectionActive = bInTargetSelectionActive;
	bLegalTarget = bInTargetSelectionActive && bInLegalTarget && InTargetId != INDEX_NONE;
	TargetId = bLegalTarget ? InTargetId : INDEX_NONE;
	bTargetHighlighted = bLegalTarget || bInTargetHighlighted;

	if (bProfileChanged || bCombatantChanged || bNeedEnemyProfileLoad)
	{
		FallbackCharacterTexture = nullptr;
		LastAppliedAnimationFrame = nullptr;
		LoadedAnimationTextures.Reset();
		bAnimationAssetsLoaded = false;
		bAnimationAssetsLoadAttempted = false;
		EnsureAnimationAssetsLoaded();
		CurrentAnimation = EBattleHUDCombatantAnimation::Idle;
		AnimationElapsedSeconds = 0.0f;
	}

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
