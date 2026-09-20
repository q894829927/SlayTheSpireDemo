#pragma once

#include "CoreMinimal.h"

namespace InteriorPortalRecursionLifetime
{
	// At most one retiring predecessor while allocating its replacement. A second
	// shrink may retire that replacement, then allocation waits non-blockingly.
	// Thus at most two allocated lifetimes exist per endpoint/level.
	inline bool CanAllocateReplacement(const int32 RetiringCount)
	{
		return RetiringCount >= 0 && RetiringCount < 2;
	}

	enum class EResourceState : uint8
	{
		Unallocated,
		Allocated,
		Active,
		Retiring,
		Reclaimable
	};

	inline const TCHAR* ToString(const EResourceState State)
	{
		switch (State)
		{
		case EResourceState::Unallocated:
			return TEXT("UNALLOCATED");
		case EResourceState::Allocated:
			return TEXT("ALLOCATED");
		case EResourceState::Active:
			return TEXT("ACTIVE");
		case EResourceState::Retiring:
			return TEXT("RETIRING");
		case EResourceState::Reclaimable:
			return TEXT("RECLAIMABLE");
		default:
			return TEXT("UNKNOWN");
		}
	}

	struct FPublicationToken
	{
		uint64 LifetimeId = 0;
		uint64 PublicationGeneration = 0;

		bool IsValid() const
		{
			return LifetimeId != 0 && PublicationGeneration != 0;
		}

		bool operator==(const FPublicationToken& Other) const
		{
			return LifetimeId == Other.LifetimeId
				&& PublicationGeneration == Other.PublicationGeneration;
		}

		bool operator!=(const FPublicationToken& Other) const
		{
			return !(*this == Other);
		}
	};

	class FLifetimeIdSource
	{
	public:
		uint64 Allocate()
		{
			if (NextId == 0)
			{
				NextId = 1;
			}

			const uint64 Result = NextId;
			++NextId;
			if (NextId == 0)
			{
				NextId = 1;
			}
			return Result;
		}

	private:
		uint64 NextId = 1;
	};

	struct FLifetime
	{
		static constexpr uint64 PackedComponentMask = 0xffffffffull;

		uint64 LifetimeId = 0;
		uint64 PublicationGeneration = 0;
		EResourceState State = EResourceState::Unallocated;
		TAtomic<uint64> ActivePackedPublicationIdentity { 0 };

		bool BeginAllocated(const uint64 NewLifetimeId)
		{
			if (NewLifetimeId == 0
				|| NewLifetimeId > PackedComponentMask
				|| (State != EResourceState::Unallocated
					&& State != EResourceState::Reclaimable))
			{
				return false;
			}

			LifetimeId = NewLifetimeId;
			PublicationGeneration = 1;
			State = EResourceState::Allocated;
			ActivePackedPublicationIdentity.Store(GetPackedPublicationIdentity());
			return true;
		}

		bool MarkActive()
		{
			if (!IsReusable())
			{
				return false;
			}
			State = EResourceState::Active;
			return true;
		}

		FPublicationToken AdvancePublicationGeneration()
		{
			const FPublicationToken Previous = CapturePublicationToken();
			if (!IsReusable() || !Previous.IsValid())
			{
				return FPublicationToken();
			}

			AdvancePublicationGenerationInternal();
			ActivePackedPublicationIdentity.Store(GetPackedPublicationIdentity());
			return Previous;
		}

		bool BeginRetirement(FPublicationToken& OutRetiredPublication)
		{
			if (!IsReusable())
			{
				return false;
			}

			OutRetiredPublication = CapturePublicationToken();
			AdvancePublicationGenerationInternal();
			State = EResourceState::Retiring;
			ActivePackedPublicationIdentity.Store(0);
			return true;
		}

		bool MarkReclaimable()
		{
			if (State != EResourceState::Retiring)
			{
				return false;
			}
			State = EResourceState::Reclaimable;
			ActivePackedPublicationIdentity.Store(0);
			return true;
		}

		bool ResetUnallocated()
		{
			if (State != EResourceState::Reclaimable)
			{
				return false;
			}
			LifetimeId = 0;
			PublicationGeneration = 0;
			State = EResourceState::Unallocated;
			ActivePackedPublicationIdentity.Store(0);
			return true;
		}

		bool IsReusable() const
		{
			return State == EResourceState::Allocated
				|| State == EResourceState::Active;
		}

		bool CanSubmit(const int32 Level, const int32 RequestedDepth) const
		{
			return IsReusable()
				&& Level >= 0
				&& RequestedDepth > 0
				&& Level < RequestedDepth;
		}

		FPublicationToken CapturePublicationToken() const
		{
			return FPublicationToken { LifetimeId, PublicationGeneration };
		}

		uint64 GetPackedPublicationIdentity() const
		{
			if (LifetimeId == 0 || PublicationGeneration == 0
				|| LifetimeId > PackedComponentMask
				|| PublicationGeneration > PackedComponentMask)
			{
				return 0;
			}
			return (LifetimeId << 32) | PublicationGeneration;
		}

		bool CanPublish(const FPublicationToken& Token) const
		{
			return IsReusable()
				&& Token.IsValid()
				&& Token.LifetimeId == LifetimeId
				&& Token.PublicationGeneration == PublicationGeneration;
		}

		bool CanPublishPacked(const uint64 PackedIdentity) const
		{
			return PackedIdentity != 0
				&& PackedIdentity == ActivePackedPublicationIdentity.Load();
		}

		bool ShouldRetirePublication(const FPublicationToken& PublishedToken) const
		{
			return (State == EResourceState::Retiring
					|| State == EResourceState::Reclaimable)
				&& PublishedToken.IsValid()
				&& PublishedToken.LifetimeId == LifetimeId
				&& PublishedToken.PublicationGeneration < PublicationGeneration;
		}

		bool ShouldRetirePackedPublication(const uint64 PublishedIdentity) const
		{
			if (PublishedIdentity == 0 || LifetimeId == 0)
			{
				return false;
			}
			const uint64 PublishedLifetimeId = PublishedIdentity >> 32;
			const uint64 PublishedGeneration = PublishedIdentity & PackedComponentMask;
			return PublishedLifetimeId == LifetimeId
				&& PublishedGeneration < PublicationGeneration;
		}

	private:
		void AdvancePublicationGenerationInternal()
		{
			++PublicationGeneration;
			if (PublicationGeneration == 0 || PublicationGeneration > PackedComponentMask)
			{
				PublicationGeneration = 1;
			}
		}
	};
}
