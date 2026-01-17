#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_HumanRun.generated.h"

UCLASS()
class UGA_HumanRun : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HumanRun();

	// GE, который бустит MoveSpeed (например Multiply 1.6 или Add +250)
	UPROPERTY(EditDefaultsOnly, Category="Run")
	TSubclassOf<class UGameplayEffect> RunEffectClass;

	// Скорость бега (если RunEffectClass не задан, используется это значение)
	UPROPERTY(EditDefaultsOnly, Category="Run")
	float RunSpeed = 450.0f;

protected:
	FActiveGameplayEffectHandle ActiveRunEffectHandle;

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
};
