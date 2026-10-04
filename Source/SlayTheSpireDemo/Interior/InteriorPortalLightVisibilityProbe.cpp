// Opt-in visibility observer. Reads the actual renderer Scene UB; never changes GI.
#include "InteriorPortalLightConnection.h"
#include "InteriorPortalSystem.h"
#include "InteriorPortalRenderSample.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FXRenderingUtils.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "RenderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "SceneRendererInterface.h"
#include "SceneUniformBuffer.h"
#include "SceneViewExtension.h"
#include "ShaderCompilerCore.h"
#include "Serialization/JsonSerializer.h"

namespace InteriorPortalLightVisibilityProbe
{
    constexpr uint32 ResultCount = 11;
#if RHI_RAYTRACING
    class FProbeCS final : public FGlobalShader
    {
        DECLARE_GLOBAL_SHADER(FProbeCS);
        SHADER_USE_PARAMETER_STRUCT(FProbeCS, FGlobalShader);
        BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
            SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
            SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
            SHADER_PARAMETER_RDG_BUFFER_SRV(RaytracingAccelerationStructure, ProbeTLAS)
            SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint4>, ProbeResults)
        END_SHADER_PARAMETER_STRUCT()
        static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
        {
            return IsRayTracingEnabledForProject(Parameters.Platform) && RHISupportsInlineRayTracing(Parameters.Platform);
        }
        static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
            FShaderCompilerEnvironment& Environment)
        {
            FGlobalShader::ModifyCompilationEnvironment(Parameters, Environment);
            Environment.CompilerFlags.Add(CFLAG_InlineRayTracing);
        }
    };
    IMPLEMENT_GLOBAL_SHADER(FProbeCS, "/Project/InteriorPortalLightVisibilityProbe.usf", "MainCS", SF_Compute);
#endif
    struct FObservation
    {
        bool bMain = false;
        FUintVector4 Results[ResultCount] = {};
    };
    struct FReport
    {
        FCriticalSection Lock;
        FString Status = TEXT("WAITING_FOR_SCENE_TLAS");
        uint64 Request = 0, Frame = 0;
        TArray<FObservation> Observations;
    };

    class FProbeExtension final : public FWorldSceneViewExtension
    {
    public:
        FProbeExtension(const FAutoRegister& Register, UWorld* World,
            TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> InConnection,
            TSharedRef<FReport, ESPMode::ThreadSafe> InReport)
            : FWorldSceneViewExtension(Register, World), Connection(InConnection), Report(InReport),
              RequestedFrame(GFrameCounter + 1) {}
        void Disable() { bEnabled.Store(false); }
        bool MatchesScene(const FSceneViewFamily& Family) const
        { return GetWorld() && GetWorld()->Scene == Family.Scene; }
        void Capture()
        {
            check(IsInGameThread());
            FScopeLock Guard(&Report->Lock);
            if (Report->Status == TEXT("GPU_SUBMITTED")) return;
            Report->Status = TEXT("WAITING_FOR_SCENE_TLAS");
            // A command can arrive while an older frame is already partly queued.
            // Observe a complete future GT frame, not whichever RT callback runs next.
            RequestedFrame = GFrameCounter + 1;
            ++Requested;
        }
        virtual ESceneViewExtensionFlags GetFlags() const override
        {
            return ESceneViewExtensionFlags::SubscribesToPostTLASBuild
                | ESceneViewExtensionFlags::RequiresHardwareInlineRayTracing;
        }
        virtual void PostTLASBuild_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& View) override
        {
            if (!bEnabled.Load() || View.bIsSceneCapture || !View.Family) return;
            const bool bMain = InteriorPortalRendering::IsPlayerMainView(*View.Family, View);
            if (!bMain && !View.Family->bAdditionalViewFamily) return;
#if RHI_RAYTRACING
            uint64 NewRequest, NewRequestFrame;
            {
                FScopeLock Guard(&Report->Lock);
                NewRequest = Requested;
                NewRequestFrame = RequestedFrame;
            }
            if (NewRequest != ActiveRequest)
            {
                if (!Pending.IsEmpty()) return;
                ActiveRequest = NewRequest;
                MinimumFrame = NewRequestFrame;
                ActiveFrame = MAX_uint64;
                bSawMain = false;
                FScopeLock Guard(&Report->Lock);
                Report->Request = ActiveRequest;
                Report->Observations.Reset();
            }
            for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
            {
                FPending& P = Pending[Index];
                if (!P.Readback->IsReady()) continue;
                const void* Data = P.Readback->Lock(sizeof(FUintVector4) * ResultCount);
                FObservation Observation;
                Observation.bMain = P.bMain;
                FMemory::Memcpy(Observation.Results, Data, sizeof(Observation.Results));
                P.Readback->Unlock();
                { FScopeLock Guard(&Report->Lock); Report->Observations.Add(Observation); }
                Pending.RemoveAt(Index);
            }
            if (!ActiveRequest) return;
            const uint64 Frame = View.Family->FrameCounter;
            if (Frame < MinimumFrame) return;
            if (ActiveFrame != MAX_uint64 && Frame != ActiveFrame)
            {
                if (bSawMain && Pending.IsEmpty()) SetStatus(TEXT("GPU_READBACK_COMPLETE_OPAQUE_VISIBILITY_ONLY"));
                return;
            }
            if (!GRHISupportsInlineRayTracing || !View.Family->Scene
                || !UE::FXRenderingUtils::RayTracing::HasRayTracingScene(*View.Family->Scene))
            { SetStatus(TEXT("UNSUPPORTED_INLINE_TLAS")); return; }
            const FRDGBufferSRVRef TLAS = UE::FXRenderingUtils::RayTracing::GetRayTracingSceneViewRDG(*View.Family->Scene, View);
            if (!TLAS) { SetStatus(TEXT("NO_SCENE_TLAS")); return; }
            ActiveFrame = Frame;
            bSawMain |= bMain;
            FProbeCS::FParameters* Parameters = GraphBuilder.AllocParameters<FProbeCS::FParameters>();
            Parameters->Scene = GetSceneUniformBufferRef(GraphBuilder, View);
            Parameters->View = View.ViewUniformBuffer;
            Parameters->ProbeTLAS = TLAS;
            FRDGBufferRef Results = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector4), ResultCount),
                TEXT("InteriorPortal.LightVisibilityProbe.Results"));
            Parameters->ProbeResults = GraphBuilder.CreateUAV(Results);
            TShaderMapRef<FProbeCS> Shader(GetGlobalShaderMap(View.GetFeatureLevel()));
            FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("InteriorPortalLightVisibilityProbe"),
                Shader, Parameters, FIntVector(1, 1, 1));
            const TSharedPtr<FRHIGPUBufferReadback, ESPMode::ThreadSafe> Readback =
                MakeShared<FRHIGPUBufferReadback, ESPMode::ThreadSafe>(TEXT("InteriorPortalLightVisibilityProbe"));
            AddEnqueueCopyPass(GraphBuilder, Readback.Get(), Results, sizeof(FUintVector4) * ResultCount);
            GraphBuilder.AddPass(RDG_EVENT_NAME("InteriorPortalLightVisibilityProbe.KeepAlive"),
                ERDGPassFlags::None, [Readback](FRHICommandListImmediate&) {});
            Pending.Add({bMain, Readback});
            FScopeLock Guard(&Report->Lock);
            Report->Frame = Frame;
            Report->Status = TEXT("GPU_SUBMITTED");
#else
            SetStatus(TEXT("UNSUPPORTED_RHI_RAYTRACING"));
#endif
        }
    protected:
        virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
        {
            return bEnabled.Load() && FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
        }
    private:
        void SetStatus(const TCHAR* Status) { FScopeLock Guard(&Report->Lock); Report->Status = Status; }
        TSharedPtr<ISceneViewExtension, ESPMode::ThreadSafe> Connection;
        TSharedRef<FReport, ESPMode::ThreadSafe> Report;
        TAtomic<bool> bEnabled {true};
        uint64 Requested = 1, RequestedFrame; // guarded by Report->Lock
        uint64 ActiveRequest = 0, ActiveFrame = MAX_uint64;
        uint64 MinimumFrame = 0;
        bool bSawMain = false;
#if RHI_RAYTRACING
        struct FPending { bool bMain; TSharedPtr<FRHIGPUBufferReadback, ESPMode::ThreadSafe> Readback; };
        TArray<FPending> Pending;
#endif
    };
    TSharedPtr<FProbeExtension, ESPMode::ThreadSafe> Extension;
    TSharedPtr<FReport, ESPMode::ThreadSafe> Latest;
    void AttachToFamily(FSceneViewFamily& Family)
    {
        check(IsInGameThread());
        if (Extension && Extension->MatchesScene(Family))
            Family.ViewExtensions.AddUnique(Extension.ToSharedRef());
    }
    void Stop()
    {
        if (Extension) { Extension->Disable(); FlushRenderingCommands(); Extension.Reset(); }
    }
    void Start(UWorld* World)
    {
        Stop();
        Latest = MakeShared<FReport, ESPMode::ThreadSafe>();
        if (!World) { Latest->Status = TEXT("NO_WORLD"); return; }
        AInteriorPortalSystem* System = nullptr;
        for (TActorIterator<AInteriorPortalSystem> It(World); It; ++It)
        {
            if (System) { Latest->Status = TEXT("AMBIGUOUS_SYSTEM"); return; }
            System = *It;
        }
        const auto Connection = InteriorPortalLighting::Acquire(System);
        if (!Connection) { Latest->Status = TEXT("NO_SCENE_CONNECTION_OWNER"); return; }
        Extension = FSceneViewExtensions::NewExtension<FProbeExtension>(World, Connection, Latest.ToSharedRef());
    }
    void Capture() { if (Extension) Extension->Capture(); }
    TSharedRef<FJsonObject> ToJson(const FObservation& O)
    {
        auto Object = MakeShared<FJsonObject>();
        Object->SetBoolField(TEXT("main"), O.bMain);
        const FUintVector4& M = O.Results[8];
        const FUintVector4& S = O.Results[9];
        Object->SetStringField(TEXT("generation"), LexToString((uint64(M.Y) << 32) | M.X));
        Object->SetNumberField(TEXT("endpointCount"), M.Z);
        Object->SetNumberField(TEXT("backendABI"), M.W);
        Object->SetStringField(TEXT("sceneSession"), LexToString((uint64(S.Y) << 32) | S.X));
        Object->SetStringField(TEXT("sealedFrame"), LexToString((uint64(S.W) << 32) | S.Z));
        Object->SetArrayField(TEXT("supportIndices"), {MakeShared<FJsonValueNumber>(O.Results[10].X), MakeShared<FJsonValueNumber>(O.Results[10].Z)});
        Object->SetArrayField(TEXT("surfaceIndices"), {MakeShared<FJsonValueNumber>(O.Results[10].Y), MakeShared<FJsonValueNumber>(O.Results[10].W)});
        TArray<TSharedPtr<FJsonValue>> Rays;
        for (uint32 I = 0; I < 8; ++I)
        {
            const FUintVector4& R = O.Results[I];
            float Distance; FMemory::Memcpy(&Distance, &R.Z, sizeof(float));
            auto Ray = MakeShared<FJsonObject>();
            Ray->SetNumberField(TEXT("endpoint"), I / 4);
            Ray->SetNumberField(TEXT("case"), I % 4);
            Ray->SetNumberField(TEXT("status"), R.X);
            Ray->SetNumberField(TEXT("hitPrimitive"), R.Y);
            Ray->SetNumberField(TEXT("distanceCm"), Distance);
            Ray->SetNumberField(TEXT("hops"), R.W);
            Rays.Add(MakeShared<FJsonValueObject>(Ray));
        }
        Object->SetArrayField(TEXT("rays"), Rays);
        return Object;
    }
    void Dump()
    {
        if (!Latest) return;
        FScopeLock Guard(&Latest->Lock);
        auto Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("status"), Latest->Status);
        Object->SetStringField(TEXT("request"), LexToString(Latest->Request));
        Object->SetNumberField(TEXT("frame"), double(Latest->Frame));
        Object->SetBoolField(TEXT("productionLightingChanged"), false);
        Object->SetBoolField(TEXT("opaqueVisibilityOnly"), true);
        TArray<TSharedPtr<FJsonValue>> Views;
        for (const FObservation& Observation : Latest->Observations)
        {
            const auto View = ToJson(Observation);
            Views.Add(MakeShared<FJsonValueObject>(View));
            if (Observation.bMain)
            {
                Object->SetArrayField(TEXT("supportIndices"), View->GetArrayField(TEXT("supportIndices")));
                Object->SetArrayField(TEXT("rays"), View->GetArrayField(TEXT("rays")));
            }
        }
        Object->SetArrayField(TEXT("views"), Views);
        FString Json;
        FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("AutomationReports/PortalLightVisibilityProbe.json")));
    }
    FAutoConsoleCommandWithWorld StartCommand(TEXT("portal.StartLightVisibilityProbe"),
        TEXT("Observe one sealed scene frame across main/additional TLAS views; no GI changes."),
        FConsoleCommandWithWorldDelegate::CreateStatic(&Start));
    FAutoConsoleCommand CaptureCommand(TEXT("portal.CaptureLightVisibilityProbe"),
        TEXT("Read a new scene frame without restarting the observer or freezing stale endpoints."),
        FConsoleCommandDelegate::CreateStatic(&Capture));
    FAutoConsoleCommand StopCommand(TEXT("portal.StopLightVisibilityProbe"), TEXT("Stop the opt-in TLAS observer."),
        FConsoleCommandDelegate::CreateStatic(&Stop));
    FAutoConsoleCommand DumpCommand(TEXT("portal.DumpLightVisibilityProbe"), TEXT("Write actual Scene UB readbacks; not GI acceptance."),
        FConsoleCommandDelegate::CreateStatic(&Dump));
}
