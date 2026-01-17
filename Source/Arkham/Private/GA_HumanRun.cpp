#include "GA_HumanRun.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagsManager.h"
#include "HumanAttributeSet.h"

UGA_HumanRun::UGA_HumanRun()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Теги:
	// Ability.Movement.Run — чтобы удобно отменять по отпусканию кнопки
	// State.Movement.Running — состояние (по нему AHuman переключит ротацию)
	const FGameplayTag AbilityRunTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Movement.Run"));
	const FGameplayTag StateRunningTag = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Running"));

	AbilityTags.AddTag(AbilityRunTag);
	ActivationOwnedTags.AddTag(StateRunningTag);
}

void UGA_HumanRun::ActivateAbility(
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

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Если задан RunEffectClass - используем его
	if (RunEffectClass)
	{
		const UGameplayEffect* GE = RunEffectClass.GetDefaultObject();
		ActiveRunEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, GE, 1.0f);
	}
	// Иначе создаём эффект программно
	else
	{
		// Создаем Gameplay Effect для изменения MoveSpeed
		UGameplayEffect* RunEffect = NewObject<UGameplayEffect>(GetTransientPackage(), TEXT("RunSpeedEffect"));
		RunEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;

		// Добавляем модификатор для MoveSpeed
		FGameplayModifierInfo ModInfo;
		ModInfo.Attribute = UHumanAttributeSet::GetMoveSpeedAttribute();
		ModInfo.ModifierOp = EGameplayModOp::Override;
		ModInfo.ModifierMagnitude = FScalableFloat(RunSpeed);

		RunEffect->Modifiers.Add(ModInfo);

		// Применяем эффект
		FGameplayEffectSpec* Spec = new FGameplayEffectSpec(RunEffect, MakeEffectContext(Handle, ActorInfo), 1.0f);
		ActiveRunEffectHandle = ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
	}
}

void UGA_HumanRun::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid() && ActiveRunEffectHandle.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveRunEffectHandle);
		ActiveRunEffectHandle.Invalidate();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
