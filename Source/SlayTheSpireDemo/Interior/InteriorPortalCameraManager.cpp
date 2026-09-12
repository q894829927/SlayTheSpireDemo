#include "InteriorPortalCameraManager.h"
void AInteriorPortalCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);
	FMinimalViewInfo View = GetCameraCacheView();
	View.PerspectiveNearClipPlane = .1f;
	FillCameraCache(View);
}
