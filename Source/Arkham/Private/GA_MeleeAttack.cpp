#include "GA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Human.h"

UGA_MeleeAttack::UGA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Теги способности
	const FGameplayTag AbilityMeleeTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combat.MeleeAttack"));
	AbilityTags.AddTag(AbilityMeleeTag);

	// Блокируем другие атаки во время выполнения
	const FGameplayTag BlockTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Attacking"));
	ActivationOwnedTags.AddTag(BlockTag);
	ActivationBlockedTags.AddTag(BlockTag);

	// Не можем атаковать если мертвы
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"));
	ActivationBlockedTags.AddTag(DeadTag);
}

void UGA_MeleeAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_MeleeAttack: AttackMontage is not set!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackMontage,
		1.0f
	);

	if (!Task)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Task->OnCompleted.AddDynamic(this, &UGA_MeleeAttack::OnMontageCompleted);
	Task->OnCancelled.AddDynamic(this, &UGA_MeleeAttack::OnMontageCancelled);
	Task->OnInterrupted.AddDynamic(this, &UGA_MeleeAttack::OnMontageCancelled);
	Task->ReadyForActivation();
}

void UGA_MeleeAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Fail-safe: если окно урона осталось включённым (например, прерывание), вырубаем.
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (AHuman* Human = Cast<AHuman>(ActorInfo->AvatarActor.Get()))
		{
			Human->StopMeleeTrace();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_MeleeAttack::OnMontageCompleted()
{
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
	{
		if (AHuman* Human = Cast<AHuman>(CurrentActorInfo->AvatarActor.Get()))
		{
			Human->OnMeleeAttackAbilityEnded();
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttack::OnMontageCancelled()
{
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
	{
		if (AHuman* Human = Cast<AHuman>(CurrentActorInfo->AvatarActor.Get()))
		{
			Human->OnMeleeAttackAbilityEnded();
		}
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
