#include "InteriorHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "InteriorChildCharacter.h"
#include "InteriorLightSwitch.h"
#include "InteriorPlayerController.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortal.h"
#include "GameFramework/PlayerController.h"

void AInteriorHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	const AInteriorPlayerController* PortalPlayer = Cast<AInteriorPlayerController>(GetOwningPlayerController());
	const AInteriorPortalSystem* Portals = PortalPlayer ? PortalPlayer->GetPortalSystem() : nullptr;
	const bool bGun = PortalPlayer && PortalPlayer->IsPortalGunEquipped();
	if (Portals)
	{
		DrawRect(FLinearColor(0.01f, .015f, .025f, .8f), 18, 146, 820, 64);
		DrawText(bGun ? TEXT("LMB Blue  |  RMB Orange  |  R Clear  |  G Holster  |  E Interact")
			: TEXT("[G] Equip portal device  |  E / LMB Interact"), FLinearColor::White, 24, 150, GEngine->GetMediumFont(), 1.7f);
		DrawText(Portals->PlacementMessage, FLinearColor(.7f,.8f,.9f), 24, 179, GEngine->GetMediumFont(), 1.5f);
		if (bGun)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const AInteriorPortal* Endpoint = Side ? Portals->OrangePortal : Portals->BluePortal;
				const FLinearColor Color = Side ? FLinearColor(1,.3f,.02f) : FLinearColor(.02f,.5f,1);
				for (int32 I=0; I<18; ++I)
				{
					const float A = (I/18.f * PI + PI*.5f) + Side*PI;
					const float B = ((I+1)/18.f * PI + PI*.5f) + Side*PI;
					DrawLine(CenterX+FMath::Cos(A)*18, CenterY+FMath::Sin(A)*24,
						CenterX+FMath::Cos(B)*18, CenterY+FMath::Sin(B)*24,
						Endpoint && Endpoint->bPlaced ? Color : Color*.3f, 3);
				}
			}
		}
	}
	DrawRect(FLinearColor::Black, CenterX - 4.0f, CenterY - 2.0f, 8.0f, 4.0f);
	DrawRect(FLinearColor::White, CenterX - 3.0f, CenterY - 1.0f, 6.0f, 2.0f);

	AInteriorChildCharacter* Character = Cast<AInteriorChildCharacter>(GetOwningPawn());
	AInteriorLightSwitch* Switch = IsValid(Character) ? Character->GetInteractionFocus() : nullptr;
	DrawText(TEXT("WASD Move  |  Mouse Look  |  Space Jump"), FLinearColor::White, 24.0f, 24.0f, GEngine->GetMediumFont(), 2.0f);
	const TCHAR* DayNightHint = IsValid(Character) && Character->IsNight()
		? TEXT("[Q] Switch to DAY") : TEXT("[Q] Switch to NIGHT");
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.65f), 18.0f, 62.0f, 320.0f, 36.0f);
	DrawText(DayNightHint, FLinearColor::White, 24.0f, 66.0f, GEngine->GetMediumFont(), 2.0f);
	const TCHAR* FlashlightHint = IsValid(Character) && Character->IsFlashlightEnabled()
		? TEXT("[F] Flashlight ON - Turn OFF") : TEXT("[F] Flashlight OFF - Turn ON");
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.65f), 18.0f, 102.0f, 460.0f, 36.0f);
	DrawText(FlashlightHint, FLinearColor::White, 24.0f, 106.0f, GEngine->GetMediumFont(), 2.0f);
	if (IsValid(Character) && Character->IsNight())
	{
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.65f), 18.0f, Canvas->ClipY - 42.0f, 660.0f, 30.0f);
		DrawText(TEXT("Beethoven - Moonlight Sonata I | Paul Pitman / musopen.org"),
			FLinearColor::White, 24.0f, Canvas->ClipY - 37.0f, GEngine->GetMediumFont(), 1.4f);
	}
	if (!IsValid(Switch))
	{
		return;
	}

	const FString Prompt = FString(bGun ? TEXT("[E] ") : TEXT("[E / LMB] ")) + (Switch->AreLightsOn() ? TEXT("Turn lights OFF") : TEXT("Turn lights ON"));
	float PromptWidth = 0.0f, PromptHeight = 0.0f;
	Canvas->StrLen(GEngine->GetMediumFont(), Prompt, PromptWidth, PromptHeight);
	PromptWidth *= 2.5f;
	PromptHeight *= 2.5f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.75f), CenterX - PromptWidth * 0.5f - 12.0f, CenterY + 16.0f, PromptWidth + 24.0f, PromptHeight + 12.0f);
	FCanvasTextItem PromptItem(
		FVector2D(CenterX - PromptWidth * 0.5f, CenterY + 22.0f),
		FText::FromString(Prompt),
		GEngine->GetMediumFont(),
		FLinearColor::White
	);
	PromptItem.Scale = FVector2D(2.5f, 2.5f);
	PromptItem.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(PromptItem);
}
