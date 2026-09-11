#include "BattleHUDWidget.h"

#include "BattleHUDCombatantPresentationWidgetBase.h"
#include "BattleHUDViewModel.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

namespace
{
	bool IsFiniteVector(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}
}

bool UBattleHUDWidget::EnsureTransientVFXHost()
{
	UCanvasPanel* Root = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!IsValid(Root))
	{
		return false;
	}

	if (IsValid(TransientVFXHost))
	{
		if (TransientVFXHost->GetParent() == Root
			&& Cast<UCanvasPanelSlot>(TransientVFXHost->Slot) != nullptr)
		{
			return true;
		}
		ReleaseTransientVFXHost();
	}

	TransientVFXHost = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("TransientVFXHost_Runtime"));
	if (!IsValid(TransientVFXHost))
	{
		return false;
	}

	UCanvasPanelSlot* HostSlot = Root->AddChildToCanvas(TransientVFXHost);
	if (!IsValid(HostSlot))
	{
		TransientVFXHost = nullptr;
		return false;
	}
	HostSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	HostSlot->SetOffsets(FMargin(0.0f));

	int32 TopZ = 0;
	for (UWidget* Child : Root->GetAllChildren())
	{
		if (Child == TransientVFXHost)
		{
			continue;
		}
		if (const UCanvasPanelSlot* ChildSlot = Cast<UCanvasPanelSlot>(Child->Slot))
		{
			TopZ = FMath::Max(TopZ, ChildSlot->GetZOrder());
		}
	}
	HostSlot->SetZOrder(TopZ + 1);
	// HitTestInvisible is sufficient to keep this visual-only while preserving
	// normal Slate enabled-state rendering for the damage text children.
	TransientVFXHost->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

void UBattleHUDWidget::ReleaseTransientVFXHost()
{
	CancelAllDetachedDamageVisuals();
	if (IsValid(TransientVFXHost))
	{
		TransientVFXHost->RemoveFromParent();
	}
	TransientVFXHost = nullptr;
}

int32 UBattleHUDWidget::FindDetachedDamageInstanceIndex(
	const FDetachedDamageToken& Token) const
{
	if (!Token.IsValid())
	{
		return INDEX_NONE;
	}
	for (int32 Index = 0; Index < DetachedDamageInstances.Num(); ++Index)
	{
		if (DetachedDamageInstances[Index].Token == Token)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void UBattleHUDWidget::RemoveDetachedDamageInstanceAt(int32 Index)
{
	if (!DetachedDamageInstances.IsValidIndex(Index))
	{
		return;
	}
	if (UTextBlock* DamageText = DetachedDamageInstances[Index].Widget.Get())
	{
		DamageText->RemoveFromParent();
	}
	DetachedDamageInstances.RemoveAt(Index);
}

bool UBattleHUDWidget::PrepareDetachedDamageVisual(
	const FPresentationSessionToken& SessionToken,
	const FPresentationRecord& Record,
	int64 SourceFinalStateRevision,
	float VisualDuration,
	FDetachedDamageToken& OutToken)
{
	OutToken = FDetachedDamageToken{};
	if (!SessionToken.IsValid()
		|| Record.Type != EBattlePresentationRecordType::Damage
		|| Record.BattleId != SessionToken.BattleId
		|| Record.ResolutionId <= 0
		|| Record.PresentationSequence <= 0
		|| SourceFinalStateRevision <= 0
		|| Record.Damage.IncomingDamage <= 0
		|| !FMath::IsFinite(VisualDuration)
		|| VisualDuration <= 0.0f
		|| DetachedDamageInstances.Num() >= MaxDetachedDamageNumberInstances
		|| !EnsureTransientVFXHost())
	{
		return false;
	}

	UBattleHUDCombatantPresentationWidgetBase* TargetPresentation = nullptr;
	UProgressBar* TargetHPProgress = nullptr;
	UTextBlock* TargetHPText = nullptr;
	UTextBlock* TargetBlockText = nullptr;
	const FBattleHUDCombatantView* HistoricalTarget = nullptr;
	if (!ResolveDamageTarget(
		Record.Damage.TargetPresentationId,
		TargetPresentation,
		TargetHPProgress,
		TargetHPText,
		TargetBlockText,
		HistoricalTarget)
		|| !IsValid(TargetPresentation)
		|| !IsValid(TransientVFXHost))
	{
		return false;
	}

	const FGeometry& TargetGeometry = TargetPresentation->GetCachedGeometry();
	const FGeometry& HostGeometry = TransientVFXHost->GetCachedGeometry();
	const FVector2D TargetSize = TargetGeometry.GetLocalSize();
	const FVector2D HostSize = HostGeometry.GetLocalSize();
	if (!IsFiniteVector(TargetSize)
		|| !IsFiniteVector(HostSize)
		|| TargetSize.X <= 0.0f
		|| TargetSize.Y <= 0.0f
		|| HostSize.X <= 0.0f
		|| HostSize.Y <= 0.0f)
	{
		return false;
	}

	const FVector2D TargetCenterAbsolute = TargetGeometry.LocalToAbsolute(TargetSize * 0.5f);
	const FVector2D HostLocalStart = HostGeometry.AbsoluteToLocal(TargetCenterAbsolute);
	if (!IsFiniteVector(TargetCenterAbsolute) || !IsFiniteVector(HostLocalStart))
	{
		return false;
	}

	if (NextDetachedDamageVisualGeneration <= 0)
	{
		NextDetachedDamageVisualGeneration = 1;
	}
	FDetachedDamageToken Token;
	Token.SessionToken = SessionToken;
	Token.SourceResolutionId = Record.ResolutionId;
	Token.PresentationSequence = Record.PresentationSequence;
	Token.LocalDamageVisualGeneration = NextDetachedDamageVisualGeneration++;
	if (!Token.IsValid())
	{
		return false;
	}

	UTextBlock* DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	if (!IsValid(DamageText))
	{
		return false;
	}
	DamageText->SetText(FText::AsNumber(Record.Damage.IncomingDamage));
	DamageText->SetVisibility(ESlateVisibility::Hidden);
	DamageText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

	UCanvasPanelSlot* DamageSlot = TransientVFXHost->AddChildToCanvas(DamageText);
	if (!IsValid(DamageSlot))
	{
		DamageText->RemoveFromParent();
		return false;
	}
	DamageSlot->SetAutoSize(true);
	DamageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	DamageSlot->SetPosition(HostLocalStart);

	FDetachedDamageWidgetInstance Instance;
	Instance.Token = Token;
	Instance.SourceFinalStateRevision = SourceFinalStateRevision;
	Instance.Spec.IncomingDamage = Record.Damage.IncomingDamage;
	Instance.Spec.TargetPresentationId = Record.Damage.TargetPresentationId;
	Instance.Spec.FrozenHostLocalStartPosition = HostLocalStart;
	Instance.Spec.VisualDuration = VisualDuration;
	Instance.Widget = DamageText;
	Instance.ElapsedSeconds = 0.0f;
	Instance.FrozenHostLocalSize = HostSize;
	Instance.LifecycleState = EDetachedDamageVisualLifecycleState::Prepared;
	if (!Instance.Spec.IsValid())
	{
		DamageText->RemoveFromParent();
		return false;
	}

	DetachedDamageInstances.Add(MoveTemp(Instance));
	OutToken = Token;
	return true;
}

bool UBattleHUDWidget::ActivatePreparedDetachedDamageVisual(
	const FDetachedDamageToken& Token)
{
	const int32 Index = FindDetachedDamageInstanceIndex(Token);
	if (!DetachedDamageInstances.IsValidIndex(Index))
	{
		return false;
	}
	FDetachedDamageWidgetInstance& Instance = DetachedDamageInstances[Index];
	UTextBlock* DamageText = Instance.Widget.Get();
	if (Instance.LifecycleState != EDetachedDamageVisualLifecycleState::Prepared
		|| !IsValid(DamageText)
		|| !IsValid(TransientVFXHost)
		|| DamageText->GetParent() != TransientVFXHost)
	{
		RemoveDetachedDamageInstanceAt(Index);
		return false;
	}

	const FVector2D CurrentHostSize = TransientVFXHost->GetCachedGeometry().GetLocalSize();
	if (!IsFiniteVector(CurrentHostSize)
		|| CurrentHostSize.X <= 0.0f
		|| CurrentHostSize.Y <= 0.0f
		|| !CurrentHostSize.Equals(Instance.FrozenHostLocalSize, 0.5f))
	{
		RemoveDetachedDamageInstanceAt(Index);
		return false;
	}

	Instance.ElapsedSeconds = 0.0f;
	Instance.LifecycleState = EDetachedDamageVisualLifecycleState::Active;
	DamageText->SetRenderTranslation(FVector2D::ZeroVector);
	DamageText->SetRenderOpacity(1.0f);
	DamageText->SetVisibility(ESlateVisibility::HitTestInvisible);
	return true;
}

bool UBattleHUDWidget::CancelDetachedDamageVisual(
	const FDetachedDamageToken& Token)
{
	const int32 Index = FindDetachedDamageInstanceIndex(Token);
	if (!DetachedDamageInstances.IsValidIndex(Index))
	{
		return false;
	}
	RemoveDetachedDamageInstanceAt(Index);
	return true;
}

int32 UBattleHUDWidget::CancelDetachedDamageVisualsForSession(
	const FPresentationSessionToken& SessionToken)
{
	if (!SessionToken.IsValid())
	{
		return 0;
	}
	int32 RemovedCount = 0;
	for (int32 Index = DetachedDamageInstances.Num() - 1; Index >= 0; --Index)
	{
		if (DetachedDamageInstances[Index].Token.SessionToken == SessionToken)
		{
			RemoveDetachedDamageInstanceAt(Index);
			++RemovedCount;
		}
	}
	return RemovedCount;
}

void UBattleHUDWidget::CancelAllDetachedDamageVisuals()
{
	for (int32 Index = DetachedDamageInstances.Num() - 1; Index >= 0; --Index)
	{
		RemoveDetachedDamageInstanceAt(Index);
	}
	DetachedDamageInstances.Reset();
}

void UBattleHUDWidget::UpdateDetachedDamageNumbers(float DeltaSeconds)
{
	if (DetachedDamageInstances.IsEmpty())
	{
		return;
	}
	if (!IsValid(TransientVFXHost))
	{
		CancelAllDetachedDamageVisuals();
		return;
	}

	const FVector2D CurrentHostSize = TransientVFXHost->GetCachedGeometry().GetLocalSize();
	if (!IsFiniteVector(CurrentHostSize)
		|| CurrentHostSize.X <= 0.0f
		|| CurrentHostSize.Y <= 0.0f)
	{
		CancelAllDetachedDamageVisuals();
		return;
	}

	const float SafeDelta = FMath::Max(DeltaSeconds, 0.0f);
	for (int32 Index = DetachedDamageInstances.Num() - 1; Index >= 0; --Index)
	{
		FDetachedDamageWidgetInstance& Instance = DetachedDamageInstances[Index];
		UTextBlock* DamageText = Instance.Widget.Get();
		if (!IsValid(DamageText)
			|| DamageText->GetParent() != TransientVFXHost
			|| !CurrentHostSize.Equals(Instance.FrozenHostLocalSize, 0.5f))
		{
			RemoveDetachedDamageInstanceAt(Index);
			continue;
		}
		if (Instance.LifecycleState == EDetachedDamageVisualLifecycleState::Prepared)
		{
			continue;
		}
		if (!Instance.Spec.IsValid())
		{
			RemoveDetachedDamageInstanceAt(Index);
			continue;
		}

		Instance.ElapsedSeconds += SafeDelta;
		const float Alpha = FMath::Clamp(
			Instance.ElapsedSeconds / Instance.Spec.VisualDuration,
			0.0f,
			1.0f);
		const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);
		DamageText->SetRenderTranslation(FVector2D(0.0f, -48.0f * Eased));
		DamageText->SetRenderOpacity(1.0f - Alpha);
		if (Instance.ElapsedSeconds >= Instance.Spec.VisualDuration)
		{
			RemoveDetachedDamageInstanceAt(Index);
		}
	}
}

void UBattleHUDWidget::PlayDamageCombatantCuesFromCommittedRecord(
	const FPresentationRecord& Record)
{
	if (Record.Type != EBattlePresentationRecordType::Damage
		|| !IsKnownCombatantPresentationId(Record.Damage.TargetPresentationId))
	{
		return;
	}

	PlayNativeCombatantAnimation(
		Record.Damage.TargetPresentationId,
		EBattleHUDCombatantAnimation::Hit);

	if (IsValid(ViewModel)
		&& Record.Damage.DamageKind == EDamageKind::Attack
		&& Record.Damage.TargetPresentationId == ViewModel->Player.PresentationId
		&& IsKnownCombatantPresentationId(Record.Damage.SourcePresentationId)
		&& Record.Damage.SourcePresentationId != ViewModel->Player.PresentationId)
	{
		PlayNativeCombatantAnimation(
			Record.Damage.SourcePresentationId,
			EBattleHUDCombatantAnimation::Attack);
	}
}

void UBattleHUDWidget::PlayCommittedDamageCombatantCues(
	const FPresentationRecord& Record,
	const FPresentationSessionToken& SessionToken)
{
	if (!SessionToken.IsValid()
		|| Record.BattleId != SessionToken.BattleId)
	{
		return;
	}
	PlayDamageCombatantCuesFromCommittedRecord(Record);
}
