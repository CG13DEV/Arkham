#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HitReaction.generated.h"

/**
 * GameplayAbility для проигрывания HitReaction анимации
 * Автоматически отменяет бег через тег State.HitReaction
 */
UCLASS()
class ARKHAM_API UGA_HitReaction : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HitReaction();

	/** Монтаж анимации получения урона */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="HitReaction")
	TObjectPtr<UAnimMontage> HitReactionMontage;

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

	/** Вызывается при завершении монтажа */
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};

