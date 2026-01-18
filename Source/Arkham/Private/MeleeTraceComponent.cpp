#include "MeleeTraceComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
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
	{
		UE_LOG(LogTemp, Error, TEXT("MeleeTrace: StartTrace failed - World or Owner is NULL"));
		return;
	}

	// Уже трейсим — перезапуск
	if (bTracing)
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeTrace: Restarting trace (was already tracing)"));
		StopTrace();
	}

	SourceASC = InSourceASC;
	SourceObject = InSourceObject;
	HitActors.Reset();
	bTracing = true;

	UE_LOG(LogTemp, Warning, TEXT("🎯 MeleeTrace: StartTrace | Damage: %.1f | Sockets: %d | Interval: %.3f"), 
		DamageForNextTrace > 0.f ? DamageForNextTrace : BaseDamage, 
		TraceSockets.Num(), 
		TraceInterval);

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
	DamageForNextTrace = 0.f;
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

	// ВАЖНО: Не наносим урон если атакующий мертв
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character))
		{
			UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
			if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))))
			{
				UE_LOG(LogTemp, Log, TEXT("⚪ MeleeTrace: Owner is dead - stopping trace"));
				StopTrace();
				return;
			}
		}
	}

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

		// Логируем попадание
		UE_LOG(LogTemp, Log, TEXT("🎯 MeleeTrace: Hit detected! Socket: %s, Hits: %d"), 
			*SocketName.ToString(), Hits.Num());

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor == GetOwner())
				continue;

			if (!bAllowMultipleHitsPerActor && HitActors.Contains(HitActor))
			{
				UE_LOG(LogTemp, Log, TEXT("⚪ MeleeTrace: %s already hit - skipping"), *HitActor->GetName());
				continue;
			}

			UE_LOG(LogTemp, Warning, TEXT("✅ MeleeTrace: NEW HIT on %s!"), *HitActor->GetName());
			HitActors.Add(HitActor);
			ApplyDamageToTarget(HitActor, Hit);
		}
	}
}

void UMeleeTraceComponent::ApplyDamageToTarget(AActor* TargetActor, const FHitResult& Hit)
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Error, TEXT("MeleeTrace: ApplyDamageToTarget - TargetActor is NULL!"));
		return;
	}

	UAbilitySystemComponent* LocalSourceASC = SourceASC.Get();
	if (!LocalSourceASC)
	{
		UE_LOG(LogTemp, Error, TEXT("MeleeTrace: ApplyDamageToTarget - SourceASC is NULL!"));
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeTrace: Target '%s' has no AbilitySystemComponent - skipping damage"), 
			*TargetActor->GetName());
		return;
	}

	if (DamageEffectClass)
	{
		FGameplayEffectContextHandle Ctx = LocalSourceASC->MakeEffectContext();
		Ctx.AddSourceObject(SourceObject.IsValid() ? SourceObject.Get() : GetOwner());
		Ctx.AddHitResult(Hit);

		FGameplayEffectSpecHandle SpecHandle = LocalSourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Ctx);
		if (SpecHandle.IsValid())
		{
			LocalSourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
			UE_LOG(LogTemp, Warning, TEXT("💥 MeleeTrace: Applied DamageEffectClass to %s"), *TargetActor->GetName());
		}
		return;
	}

	// Fallback: моментальный runtime GE на мета-атрибут Damage
	FGameplayEffectContextHandle Ctx = LocalSourceASC->MakeEffectContext();
	Ctx.AddSourceObject(SourceObject.IsValid() ? SourceObject.Get() : GetOwner());
	Ctx.AddHitResult(Hit);

	UGameplayEffect* DamageGE = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("GE_MeleeDamage_Runtime"));
	DamageGE->DurationPolicy = EGameplayEffectDurationType::Instant;

	// Используем DamageForNextTrace если установлен, иначе BaseDamage
	float FinalDamage = (DamageForNextTrace > 0.f) ? DamageForNextTrace : BaseDamage;

	FGameplayModifierInfo ModInfo;
	ModInfo.Attribute = UHumanAttributeSet::GetDamageAttribute();
	ModInfo.ModifierOp = EGameplayModOp::Additive;
	ModInfo.ModifierMagnitude = FScalableFloat(FinalDamage);
	DamageGE->Modifiers.Add(ModInfo);

	FGameplayEffectSpec Spec(DamageGE, Ctx, 1.0f);
	TargetASC->ApplyGameplayEffectSpecToSelf(Spec);

	UE_LOG(LogTemp, Warning, TEXT("💥 MeleeTrace: Applied %.1f damage to %s (Source: %s)"), 
		FinalDamage, 
		*TargetActor->GetName(),
		SourceObject.IsValid() ? *SourceObject->GetName() : TEXT("NULL"));
}
