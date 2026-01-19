#include "GA_HitReaction.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagsManager.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"

UGA_HitReaction::UGA_HitReaction()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// Тег способности
	const FGameplayTag AbilityHitReactionTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.HitReaction"));
	const FGameplayTag StateHitReactionTag = FGameplayTag::RequestGameplayTag(TEXT("State.HitReaction"));

	AbilityTags.AddTag(AbilityHitReactionTag);
	
	// ВАЖНО: Этот тег отменит бег (GA_HumanRun имеет CancelAbilitiesWithTag)
	ActivationOwnedTags.AddTag(StateHitReactionTag);
	
	// Блокируем активацию если уже есть State.HitReaction
	ActivationBlockedTags.AddTag(StateHitReactionTag);
}

void UGA_HitReaction::ActivateAbility(
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

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || !HitReactionMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_HitReaction: Character or HitReactionMontage is NULL!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_HitReaction: AnimInstance is NULL!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Если HitReaction уже играет - прерываем его
	if (AnimInstance->Montage_IsPlaying(HitReactionMontage))
	{
		UE_LOG(LogTemp, Log, TEXT("GA_HitReaction: Interrupting current HitReaction"));
		AnimInstance->Montage_Stop(0.1f, HitReactionMontage);
	}

	// Запускаем монтаж
	float PlayLength = AnimInstance->Montage_Play(HitReactionMontage);
	
	if (PlayLength > 0.f)
	{
		// Устанавливаем callback на завершение
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UGA_HitReaction::OnMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, HitReactionMontage);
		
		UE_LOG(LogTemp, Warning, TEXT("🤕 GA_HitReaction: Playing HitReaction montage - RUN CANCELLED by tag!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GA_HitReaction: Montage_Play failed!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_HitReaction::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(LogTemp, Warning, TEXT("🤕 GA_HitReaction: Ended - RUN UNBLOCKED"));
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_HitReaction::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogTemp, Warning, TEXT("🤕 GA_HitReaction: Montage finished (Interrupted: %s)"), 
		bInterrupted ? TEXT("YES") : TEXT("NO"));
	
	// Завершаем абилку - тег State.HitReaction снимется автоматически
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
}

