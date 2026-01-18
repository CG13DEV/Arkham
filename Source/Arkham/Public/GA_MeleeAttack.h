#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MeleeAttack.generated.h"

/**
 * Способность атаки ближнего боя.
 * ВАЖНО: урон теперь НЕ наносится внутри GA.
 * Окно урона задаётся анимацией через StartMeleeTrace/StopMeleeTrace.
 */
UCLASS()
class UGA_MeleeAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MeleeAttack();

	/** Монтаж анимации атаки */
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TObjectPtr<UAnimMontage> AttackMontage;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageCancelled();
};
