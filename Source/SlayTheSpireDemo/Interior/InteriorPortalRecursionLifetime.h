#pragma once

#include "CoreMinimal.h"

namespace InteriorPortalRecursionLifetime
{
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
		uint64 LifetimeId = 0;
		uint64 PublicationGeneration = 0;
		EResourceState State = EResourceState::Unallocated;

		bool BeginAllocated(const uint64 NewLifetimeId)
		{
			if (NewLifetimeId == 0
				|| (State != EResourceState::Unallocated
					&& State != EResourceState::Reclaimable))
			{
				return false;
			}

			LifetimeId = NewLifetimeId;
			PublicationGeneration = 1;
			State = EResourceState::Allocated;
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

		bool BeginRetirement(FPublicationToken& OutRetiredPublication)
		{
			if (!IsReusable())
			{
				return false;
			}

			OutRetiredPublication = CapturePublicationToken();
			AdvancePublicationGeneration();
			State = EResourceState::Retiring;
			return true;
		}

		bool MarkReclaimable()
		{
			if (State != EResourceState::Retiring)
			{
				return false;
			}
			State = EResourceState::Reclaimable;
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

		bool CanPublish(const FPublicationToken& Token) const
		{
			return IsReusable()
				&& Token.IsValid()
				&& Token.LifetimeId == LifetimeId
				&& Token.PublicationGeneration == PublicationGeneration;
		}

		bool ShouldRetirePublication(const FPublicationToken& PublishedToken) const
		{
			return (State == EResourceState::Retiring
					|| State == EResourceState::Reclaimable)
				&& PublishedToken.IsValid()
				&& PublishedToken.LifetimeId == LifetimeId
				&& PublishedToken.PublicationGeneration < PublicationGeneration;
		}

	private:
		void AdvancePublicationGeneration()
		{
			++PublicationGeneration;
			if (PublicationGeneration == 0)
			{
				PublicationGeneration = 1;
			}
		}
	};
}
