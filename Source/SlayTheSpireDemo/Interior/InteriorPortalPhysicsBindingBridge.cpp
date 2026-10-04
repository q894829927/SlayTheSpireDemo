#include "InteriorPortalPhysicsBindingBridge.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/SimCallbackObject.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "HAL/ThreadSafeCounter64.h"

namespace InteriorPortalPhysics
{
	namespace { FThreadSafeCounter64 ProbeSolverEpochs; }

	struct FBindingObserveInput final : Chaos::FSimCallbackInput
	{
		FPassageBindingDomain Domain;
		FBoundaryCommand Command;
		bool bCancel = false;
		void Reset() { Domain = {}; Command = {}; bCancel = false; }
	};
	struct FBindingObserveOutput final : Chaos::FSimCallbackOutput
	{
		FPassageBindingDomain Domain;
		FPassageSessionHandoff Handoff;
		bool bRetired = false;
		uint64 Steps = 0;
		bool bHasClearance = false;
		EStaticClearanceReason ClearanceReason = EStaticClearanceReason::InvalidInterval;
		uint32 SceneIssueMask = 0;
		ENativeBindingIssue BindingIssue = ENativeBindingIssue::None;
		int32 BindingComponent = -1;
		FPhysicsStepKey ClearanceStep;
		void Reset()
		{ Domain = {}; Handoff = {}; bRetired = false; Steps = 0; bHasClearance = false;
		  ClearanceReason = EStaticClearanceReason::InvalidInterval;
		  SceneIssueMask = 0;
		  BindingIssue = ENativeBindingIssue::None; BindingComponent = -1; ClearanceStep = {}; }
	};
	class FPortalBindingObserveCallback final : public Chaos::TSimCallbackObject<FBindingObserveInput,
		FBindingObserveOutput,Chaos::ESimCallbackOptions::PreIntegrate | Chaos::ESimCallbackOptions::ParticleUnregister>
	{
	public:
		void BindBeforeDispatch(Chaos::FSingleParticlePhysicsProxy* Body,
			Chaos::FSingleParticlePhysicsProxy* Support0, Chaos::FSingleParticlePhysicsProxy* Support1,
			const FPreparedPhysicsBinding& Prepared, const FPassageBindingDomain& InDomain)
		{
			Domain = InDomain;
			BodyProxy = Body;
			Entry = Prepared.Command.Entry; Exit = Prepared.Command.Exit;
			Session = MakeUnique<FChaosPassageSession>(static_cast<Chaos::FPBDRigidsSolver*>(GetSolver()),
				Body,Support0,Support1,Prepared.Command,Domain.SolverEpoch,Domain.BindingEpoch);
			if (!Session->IsRetired() && !Session->AttachClearance_Internal(Prepared)) { Session->Retire_Internal(); }
		}
		virtual FName GetFNameForStatId() const override { return TEXT("PortalBindingObserve"); }
	private:
		FPassageBindingDomain Domain;
		Chaos::FSingleParticlePhysicsProxy* BodyProxy = nullptr;
		FTransform Entry, Exit;
		TUniquePtr<FChaosPassageSession> Session;
		uint64 Steps = 0;
		virtual void OnPreSimulate_Internal() override {}
		virtual void OnPreIntegrate_Internal() override
		{
			if (!Session) { return; }
			const auto* Input = GetConsumerInput_Internal();
			if (Input && (Input->bCancel || !(Input->Domain == Domain)
				|| Input->Command.Traveller.Handle != Domain.Body
				|| Input->Command.PairGeneration != Domain.PairGeneration
				|| !Input->Command.Entry.Equals(Entry,0) || !Input->Command.Exit.Equals(Exit,0)))
			{ Session->Retire_Internal(); }
			auto& Output = GetProducerOutputData_Internal();
			Output.Domain = Domain;
			Output.Steps = ++Steps;
			if (!Session->IsRetired() && Input && Session->Clearance() && BodyProxy)
			{
				const auto* Handle = BodyProxy->GetPhysicsThreadAPI();
				if (Handle)
				{
					FBoundaryState Start;
					Start.Pose = FTransform(Handle->R(),Handle->X(),Input->Command.Traveller.Geometry.BakedScale);
					Start.LinearVelocity = Handle->V(); Start.AngularVelocity = Handle->W();
					FBoundaryState Predicted = Start;
					const FPhysicsStepKey Step {Domain.SolverEpoch,
						static_cast<Chaos::FPBDRigidsSolver*>(GetSolver())->GetCurrentFrame(),Steps,GetDeltaTime_Internal()};
					Predicted.Pose.AddToTranslation((Handle->V() + Handle->Acceleration()*Step.DeltaSeconds)*Step.DeltaSeconds);
					const auto Proof = Session->Clearance()->Certify_Internal(Input->Command,Start,Predicted,Step,
						EClearanceStage::PreIntegrate,0);
					Output.bHasClearance = true;
					Output.ClearanceReason = Proof.Reason();
					Output.SceneIssueMask = Proof.SceneIssueMask();
					Output.BindingIssue = Proof.BindingIssue();
					Output.BindingComponent = Proof.BindingComponent();
					Output.ClearanceStep = Step;
					if (Proof.Reason() == EStaticClearanceReason::BindingChanged || Proof.Reason() == EStaticClearanceReason::Retired)
					{ Session->Retire_Internal(); }
				}
			}
			Output.bRetired = Session->IsRetired();
			if (Output.bRetired) { Output.Handoff = Session->Retire_Internal(); }
		}
		virtual void OnParticleUnregistered_Internal(
			TArray<TTuple<Chaos::FUniqueIdx,Chaos::FSingleParticlePhysicsProxy*>>& Removed) override
		{
			if (!Session) { return; }
			for (const auto& Item : Removed)
			{
				if (Session->ReferencesProxy(Item.Get<1>())) { Session->Retire_Internal(); break; }
			}
		}
	};

	FPortalPhysicsBindingBridge::~FPortalPhysicsBindingBridge() { Shutdown_GameThread(); }

	bool FPortalPhysicsBindingBridge::Start_GameThread(const FPhysicsBindingRequest& Request,
		const FPreparedPhysicsBinding& InPrepared)
	{
		if (!IsInGameThread() || Callback || Lifecycle.HasActive() || Lifecycle.HasRetiring()) { return false; }
		FPhysScene_Chaos* NewScene = Request.World ? Request.World->GetPhysicsScene() : nullptr;
		auto* Solver = NewScene ? NewScene->GetSolver() : nullptr;
		auto* Body = Request.Body && Request.Body->GetBodyInstance()
			? Request.Body->GetBodyInstance()->GetPhysicsActor() : nullptr;
		auto* Support0 = Request.Supports[0] && Request.Supports[0]->GetBodyInstance()
			? Request.Supports[0]->GetBodyInstance()->GetPhysicsActor() : nullptr;
		auto* Support1 = Request.Supports[1] && Request.Supports[1]->GetBodyInstance()
			? Request.Supports[1]->GetBodyInstance()->GetPhysicsActor() : nullptr;
		const uint64 SolverEpoch = ProbeSolverEpochs.Increment();
		if (!Solver || !Body || !Support0 || !Support1 || !SolverEpoch
			|| !Lifecycle.Open(InPrepared.Command.Traveller.Handle,SolverEpoch,InPrepared.Command.PairGeneration,Domain))
		{ return false; }
		Scene = NewScene; Prepared = InPrepared;
		ObservedSteps = 0;
		ClearanceSteps = 0; MismatchSteps = 0; SceneIssueMask = 0;
		Proxies[0] = Body; Proxies[1] = Support0; Proxies[2] = Support1;
		Callback = Solver->CreateAndRegisterSimCallbackObject_External<FPortalBindingObserveCallback>();
		Callback->BindBeforeDispatch(Body,Support0,Support1,Prepared,Domain);
		return true;
	}

	void FPortalPhysicsBindingBridge::Poll_GameThread()
	{
		if (!Callback) { return; }
		while (auto Output = Callback->PopOutputData_External())
		{
			if (!(Output->Domain == Domain)) { continue; }
			ObservedSteps = FMath::Max(ObservedSteps,Output->Steps);
			if (Output->bHasClearance)
			{
				++ClearanceSteps;
				if (Output->ClearanceReason == EStaticClearanceReason::BindingChanged) { ++MismatchSteps; }
				ClearanceReason = Output->ClearanceReason;
				SceneIssueMask = Output->SceneIssueMask;
				BindingIssue = Output->BindingIssue;
				BindingComponent = Output->BindingComponent;
				ClearanceStep = Output->ClearanceStep;
			}
			if (Output->bRetired)
			{
				Lifecycle.BeginRetirement(Domain);
				if (Lifecycle.ConfirmRetired(Domain,Output->Handoff))
				{
					Scene->GetSolver()->UnregisterAndFreeSimCallbackObject_External(Callback);
					Callback = nullptr; Scene = nullptr; Proxies[0] = Proxies[1] = Proxies[2] = nullptr;
					bCancelling = false;
					break;
				}
			}
		}
	}

	void FPortalPhysicsBindingBridge::Cancel_GameThread()
	{
		if (!Callback) { return; }
		if (!bCancelling) { Lifecycle.BeginRetirement(Domain); bCancelling = true; }
		// Sim callback input is consumed per dispatch. Keep cancellation live until
		// the PT retirement handoff is observed rather than assuming one GT frame
		// reaches every solver frame/substep.
		auto* Input = Callback->GetProducerInputData_External();
		Input->Domain = Domain; Input->bCancel = true;
	}

	void FPortalPhysicsBindingBridge::Update_GameThread(const FPhysicsBindingRequest* Request)
	{
		if (!IsInGameThread()) { return; }
		Poll_GameThread();
		if (!Request) { Cancel_GameThread(); return; }
		FPreparedPhysicsBinding Current;
		if (PreparePhysicsBinding_GameThread(*Request,Current) != EPhysicsBindingResult::Ready)
		{ Cancel_GameThread(); return; }
		if (!Callback) { Start_GameThread(*Request,Current); }
		if (bCancelling) { Cancel_GameThread(); return; }
		if (!Callback) { return; }
		const auto* Body = Request->Body->GetBodyInstance()->GetPhysicsActor();
		const auto* Support0 = Request->Supports[0]->GetBodyInstance()->GetPhysicsActor();
		const auto* Support1 = Request->Supports[1]->GetBodyInstance()->GetPhysicsActor();
		if (Body != Proxies[0] || Support0 != Proxies[1] || Support1 != Proxies[2]
			|| Current.Command.Traveller.Handle != Domain.Body
			|| Current.Command.PairGeneration != Domain.PairGeneration
			|| !Current.Command.Entry.Equals(Prepared.Command.Entry,0)
			|| !Current.Command.Exit.Equals(Prepared.Command.Exit,0)
			|| Current.Command.HalfWidth != Prepared.Command.HalfWidth
			|| Current.Command.HalfHeight != Prepared.Command.HalfHeight
			|| !(Current.Command.Traveller.Geometry == Prepared.Command.Traveller.Geometry)
			|| Current.Command.LocalAuthorityReference != Prepared.Command.LocalAuthorityReference
			|| Current.Command.SupportHalfThicknessCm != Prepared.Command.SupportHalfThicknessCm
			|| !(Current.SupportGeometry[0] == Prepared.SupportGeometry[0])
			|| !(Current.SupportGeometry[1] == Prepared.SupportGeometry[1])
			|| !Current.SupportPose[0].Equals(Prepared.SupportPose[0],0)
			|| !Current.SupportPose[1].Equals(Prepared.SupportPose[1],0))
		{ Cancel_GameThread(); return; }
		auto* Input = Callback->GetProducerInputData_External();
		Input->Domain = Domain; Input->Command = Current.Command;
	}

	void FPortalPhysicsBindingBridge::Shutdown_GameThread()
	{
		if (Callback && Scene)
		{
			Lifecycle.BeginRetirement(Domain);
			Scene->GetSolver()->UnregisterAndFreeSimCallbackObject_External(Callback);
		}
		Callback = nullptr; Scene = nullptr; Proxies[0] = Proxies[1] = Proxies[2] = nullptr;
		bCancelling = false;
	}
}
