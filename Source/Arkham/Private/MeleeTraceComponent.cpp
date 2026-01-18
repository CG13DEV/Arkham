#include "MeleeTraceComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

#include "HumanAttributeSet.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

UMeleeTraceComponent::UMeleeTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMeleeTraceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TraceMesh)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			TraceMesh = Character->GetMesh();
		}
		else
		{
			TraceMesh = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
		}
	}
}

void UMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTrace();
	Super::EndPlay(EndPlayReason);
}

void UMeleeTraceComponent::SetTraceMesh(USkeletalMeshComponent* InMesh)
{
	TraceMesh = InMesh;
}

void UMeleeTraceComponent::StartTrace(UAbilitySystemComponent* InSourceASC, UObject* InSourceObject)
{
	if (!GetWorld() || !GetOwner())
		return;

	// Уже трейсим — перезапуск
	StopTrace();

	SourceASC = InSourceASC;
	SourceObject = InSourceObject;
	HitActors.Reset();
	bTracing = true;

	CacheInitialSocketPositions();

	GetWorld()->GetTimerManager().SetTimer(
		TraceTimerHandle,
		this,
		&UMeleeTraceComponent::TickTrace,
		FMath::Max(TraceInterval, 0.001f),
		true
	);
}

void UMeleeTraceComponent::StopTrace()
{
	bTracing = false;
	HitActors.Reset();
	LastSocketPositions.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TraceTimerHandle);
	}
}

void UMeleeTraceComponent::CacheInitialSocketPositions()
{
	LastSocketPositions.Reset();

	if (!TraceMesh || TraceSockets.Num() == 0)
		return;

	LastSocketPositions.SetNum(TraceSockets.Num());
	for (int32 i = 0; i < TraceSockets.Num(); ++i)
	{
		LastSocketPositions[i] = TraceMesh->GetSocketLocation(TraceSockets[i]);
	}
}

void UMeleeTraceComponent::TickTrace()
{
	if (!bTracing || !GetOwner() || !GetWorld())
		return;

	// Урон только на сервере
	if (!GetOwner()->HasAuthority())
		return;

	if (!TraceMesh || TraceSockets.Num() == 0)
		return;

	if (LastSocketPositions.Num() != TraceSockets.Num())
		CacheInitialSocketPositions();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeTrace), false);
	QueryParams.AddIgnoredActor(GetOwner());

	FCollisionShape Shape = FCollisionShape::MakeSphere(TraceRadius);

	for (int32 i = 0; i < TraceSockets.Num(); ++i)
	{
		const FName SocketName = TraceSockets[i];
		const FVector CurrentPos = TraceMesh->GetSocketLocation(SocketName);
		const FVector Start = LastSocketPositions.IsValidIndex(i) ? LastSocketPositions[i] : CurrentPos;
		const FVector End = CurrentPos;
		if (LastSocketPositions.IsValidIndex(i))	LastSocketPositions[i] = CurrentPos;

		TArray<FHitResult> Hits;
		const bool bHit = GetWorld()->SweepMultiByChannel(
			Hits,
			Start,
			End,
			FQuat::Identity,
			TraceChannel,
			Shape,
			QueryParams
		);

		if (bDebugDraw)
		{
			DrawDebugSphere(GetWorld(), End, TraceRadius, 10, bHit ? FColor::Green : FColor::Red, false, 0.05f);
			DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 0.05f, 0, 1.0f);
		}

		if (!bHit)
			continue;

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor == GetOwner())
				continue;

			if (!bAllowMultipleHitsPerActor && HitActors.Contains(HitActor))
				continue;

			HitActors.Add(HitActor);
			ApplyDamageToTarget(HitActor, Hit);
		}
	}
}

void UMeleeTraceComponent::ApplyDamageToTarget(AActor* TargetActor, const FHitResult& Hit)
{
	UAbilitySystemComponent* LocalSourceASC = SourceASC.Get();
	if (!LocalSourceASC)
		return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
		return;

	if (DamageEffectClass)
	{
		FGameplayEffectContextHandle Ctx = LocalSourceASC->MakeEffectContext();
		Ctx.AddSourceObject(SourceObject.IsValid() ? SourceObject.Get() : GetOwner());
		Ctx.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle = LocalSourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Ctx);
		if (SpecHandle.IsValid())
		{
			LocalSourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		}
		return;
	}

	// Fallback: моментальный runtime GE на мета-атрибут Damage
	FGameplayEffectContextHandle Ctx = LocalSourceASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject.IsValid() ? SourceObject.Get() : GetOwner());
	Ctx.AddHitResult(Hit);

	UGameplayEffect* DamageGE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_MeleeDamage_Runtime"));
	DamageGE->DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UHumanAttributeSet::GetDamageAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FScalableFloat(BaseDamage);
	DamageGE->Modifiers.Add(ModInfo);

	FGameplayEffectSpec Spec(DamageGE, Ctx, 1.0f);
	TargetASC->ApplyGameplayEffectSpecToSelf(Spec);
}
