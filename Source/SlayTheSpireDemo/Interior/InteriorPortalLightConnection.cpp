#include "InteriorPortalLightConnection.h"
#include "InteriorPortal.h"
#include "InteriorPortalSystem.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/ScopeLock.h"
#include "PrimitiveSceneInfo.h"
#include "SceneInterface.h"
#include "SceneRendererInterface.h"
#include "SceneUniformBuffer.h"
#include "SceneViewExtension.h"

// Register before shader layouts are finalized (runtime module loads PostConfigInit).
// Constants only: the default empty member needs no RDG resource and works in every scene.
BEGIN_SHADER_PARAMETER_STRUCT(FInteriorPortalLightConnectionParameters, )
    SHADER_PARAMETER(FUintVector4, Metadata) // resource generation low/high, endpoint count, backend ABI
    SHADER_PARAMETER(FUintVector4, SceneEpoch) // owner session low/high, sealed game frame low/high
    SHADER_PARAMETER_ARRAY(FVector4f, CentersHigh, [2])
    SHADER_PARAMETER_ARRAY(FVector4f, CentersLow, [2])
    SHADER_PARAMETER_ARRAY(FVector4f, Normals, [2])
    SHADER_PARAMETER_ARRAY(FVector4f, Rights, [2])
    SHADER_PARAMETER_ARRAY(FVector4f, Ups, [2])
    SHADER_PARAMETER_ARRAY(FUintVector4, Identities, [2])
END_SHADER_PARAMETER_STRUCT()
DECLARE_SCENE_UB_STRUCT(FInteriorPortalLightConnectionParameters, InteriorPortalLighting, );
IMPLEMENT_SCENE_UB_STRUCT(FInteriorPortalLightConnectionParameters, InteriorPortalLighting,
    [](FInteriorPortalLightConnectionParameters& Value, FRDGBuilder&) { Value = {}; });

namespace InteriorPortalLighting
{
    bool FConnectionInput::IsValid() const
    {
        if (Count != 2) return false;
        for (int32 E = 0; E < 2; ++E)
        {
            if (Frames[E].ContainsNaN() || !Frames[E].GetRotation().IsNormalized()
                || !Frames[E].GetScale3D().Equals(FVector::OneVector, 0)
                || !FMath::IsFinite(Sizes[E].X) || !FMath::IsFinite(Sizes[E].Y)
                || Sizes[E].X <= 0 || Sizes[E].Y <= 0
                || !Supports[E].IsValid() || Endpoints[E] == FObjectKey{}) return false;
        }
        return Sizes[0].Equals(Sizes[1], 0) && Endpoints[0] != Endpoints[1];
    }

    bool FConnectionInput::Matches(const FConnectionInput& Other) const
    {
        if (Count != Other.Count || Count > 2) return false;
        for (uint32 E = 0; E < Count; ++E)
        {
            if (!Frames[E].Equals(Other.Frames[E], 0) || !Sizes[E].Equals(Other.Sizes[E], 0)
                || Endpoints[E] != Other.Endpoints[E] || Supports[E] != Other.Supports[E]
                || Surfaces[E] != Other.Surfaces[E]) return false;
        }
        return true;
    }

    FSealedConnection FConnectionTimeline::Seal(const FConnectionInput& Input, uint64 Frame)
    {
        if (bSealed && Current.Frame == Frame) return Current;
        const FConnectionInput ValidInput = Input.IsValid() ? Input : FConnectionInput{};
        if (!bSealed || !Current.Input.Matches(ValidInput)) ++Current.Generation;
        Current.Input = ValidInput;
        Current.Frame = Frame;
        bSealed = true;
        return Current;
    }

    class FSceneConnectionExtension final : public FWorldSceneViewExtension
    {
    public:
        FSceneConnectionExtension(const FAutoRegister& Register, AInteriorPortalSystem* InSystem, uint64 InSession)
            : FWorldSceneViewExtension(Register, InSystem->GetWorld()), System(InSystem), Session(InSession) {}

        bool Owns(const AInteriorPortalSystem* Candidate) const { return System == Candidate; }

        virtual void PostCreateSceneRenderer(const FSceneViewFamily& Family, ISceneRenderer* Renderer) override
        {
            check(IsInGameThread());
            FConnectionInput Input;
            if (System.IsValid() && System->IsLinked())
            {
                AInteriorPortal* Portals[2] = {System->BluePortal, System->OrangePortal};
                Input.Count = 2;
                for (int32 E = 0; E < 2; ++E)
                {
                    const AInteriorPortal* Portal = Portals[E];
                    if (!IsValid(Portal) || !IsValid(Portal->Support))
                    { Input.Count = 0; break; }
                    Input.Frames[E] = Portal->GetLogicalFrame();
                    Input.Endpoints[E] = FObjectKey(Portal);
                    Input.Sizes[E] = FVector2D(Portal->HalfWidth, Portal->HalfHeight);
                    Input.Supports[E] = Portal->Support->GetPrimitiveSceneId();
                    Input.Surfaces[E] = IsValid(Portal->Surface) ? Portal->Surface->GetPrimitiveSceneId() : FPrimitiveComponentId{};
                }
            }
            const FSealedConnection Sealed = Timeline.Seal(Input, GFrameCounter);
            FScopeLock Guard(&Lock);
            Pending.Add(Renderer, Sealed);
        }

        virtual void PreRenderViewFamily_RenderThread(FRDGBuilder&, FSceneViewFamily& Family) override
        {
            ISceneRenderer* Renderer = Family.GetSceneRenderer();
            if (!Renderer) return;
            FSealedConnection Sealed;
            {
                FScopeLock Guard(&Lock);
                if (!Pending.RemoveAndCopyValue(Renderer, Sealed)) return;
            }
            if (PublishedFrame == Sealed.Frame)
            {
                Renderer->GetSceneUniforms().Set(SceneUB::InteriorPortalLighting, Published);
                return;
            }
            // This hook runs after the scene's static primitive update. Resolve only
            // exact frozen component identities; persistent indices are never reused from GT.
            FInteriorPortalLightConnectionParameters Value = {};
            Value.SceneEpoch = FUintVector4(uint32(Session), uint32(Session >> 32),
                uint32(Sealed.Frame), uint32(Sealed.Frame >> 32));
            bool bResolved = Sealed.Input.Count == 2 && Family.Scene;
            for (uint32 E = 0; bResolved && E < Sealed.Input.Count; ++E)
            {
                const FPrimitiveSceneInfo* Support = Family.Scene->GetPrimitiveSceneInfo(Sealed.Input.Supports[E]);
                const FPrimitiveSceneInfo* Surface = Family.Scene->GetPrimitiveSceneInfo(Sealed.Input.Surfaces[E]);
                if (!Support) { bResolved = false; break; }
                Value.Identities[E] = FUintVector4(Support->GetPersistentIndex().Index,
                    Surface ? Surface->GetPersistentIndex().Index : MAX_uint32, 1 - E, 0);
            }
            // Queue-order publication protects caches against topology changes AND
            // render-proxy replacement with the same component identity.
            if (LastInputGeneration != Sealed.Generation || LastResolved != bResolved
                || (bResolved && (LastIdentities[0] != Value.Identities[0] || LastIdentities[1] != Value.Identities[1])))
            {
                ++ResourceGeneration;
                LastInputGeneration = Sealed.Generation;
                LastResolved = bResolved;
                LastIdentities[0] = Value.Identities[0];
                LastIdentities[1] = Value.Identities[1];
            }
            Value.Metadata = FUintVector4(uint32(ResourceGeneration), uint32(ResourceGeneration >> 32), bResolved ? 2 : 0, 1);
            if (bResolved)
            {
                for (int32 E = 0; E < 2; ++E)
                {
                    const FTransform& Frame = Sealed.Input.Frames[E];
                    const FVector Center = Frame.GetLocation();
                    const FVector3f High(Center);
                    Value.CentersHigh[E] = FVector4f(High, 0);
                    Value.CentersLow[E] = FVector4f(FVector3f(Center - FVector(High)), 0);
                    Value.Normals[E] = FVector4f(FVector3f(Frame.GetUnitAxis(EAxis::X)), 0);
                    Value.Rights[E] = FVector4f(FVector3f(Frame.GetUnitAxis(EAxis::Y)), float(Sealed.Input.Sizes[E].X));
                    Value.Ups[E] = FVector4f(FVector3f(Frame.GetUnitAxis(EAxis::Z)), float(Sealed.Input.Sizes[E].Y));
                }
            }
            else
            {
                Value.Identities[0] = Value.Identities[1] = FUintVector4(MAX_uint32, MAX_uint32, 0, 0);
            }
            // One owner, identical world-space data; every renderer materializes its
            // own RDG uniform buffer. Never retain an RDG pointer across graphs/views.
            Renderer->GetSceneUniforms().Set(SceneUB::InteriorPortalLighting, Value);
            Published = Value;
            PublishedFrame = Sealed.Frame;
        }

    private:
        TWeakObjectPtr<AInteriorPortalSystem> System; // GT only
        const uint64 Session;
        FConnectionTimeline Timeline; // GT only
        FCriticalSection Lock;
        TMap<ISceneRenderer*, FSealedConnection> Pending;
        uint64 LastInputGeneration = 0, ResourceGeneration = 0; // RT only
        bool LastResolved = false;
        FUintVector4 LastIdentities[2] = {};
        FInteriorPortalLightConnectionParameters Published = {};
        uint64 PublishedFrame = MAX_uint64;
    };

    TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> Acquire(AInteriorPortalSystem* System)
    {
        check(IsInGameThread());
        if (!IsValid(System) || !System->GetWorld() || !System->GetWorld()->Scene) return nullptr;
        static uint64 NextSession = 0;
        static TMap<TWeakObjectPtr<UWorld>, TWeakPtr<FSceneConnectionExtension, ESPMode::ThreadSafe>> Owners;
        for (auto It = Owners.CreateIterator(); It; ++It)
            if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
        const TWeakObjectPtr<UWorld> World = System->GetWorld();
        if (const auto* Existing = Owners.Find(World))
        {
            const auto Pinned = Existing->Pin();
            // The initiative supports one explicit pair per world; never pick a different
            // system according to actor discovery order.
            return Pinned && Pinned->Owns(System) ? Pinned : nullptr;
        }
        auto NewOwner = FSceneViewExtensions::NewExtension<FSceneConnectionExtension>(System, ++NextSession);
        Owners.Add(World, NewOwner);
        return NewOwner;
    }

    void Attach(const TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe>& Connection, FSceneViewFamily& Family)
    {
        if (Connection) Family.ViewExtensions.AddUnique(Connection.ToSharedRef());
        InteriorPortalLightVisibilityProbe::AttachToFamily(Family);
    }
}
