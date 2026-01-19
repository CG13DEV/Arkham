#include "GA_MeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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

	// Инициализация
	CurrentAttackType = EAxeAttackType::LightStandingRight;
	ComboCounter = 0;
	bHeavyAttackInput = false;

	// Стартовые атаки (с которых можно начать комбо)
	StarterAttacks.Add(EAxeAttackType::LightStandingRight);
	StarterAttacks.Add(EAxeAttackType::LightStandingLeft);
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

	// Выбираем атаку
	EAxeAttackType SelectedAttack;
	
	if (ComboCounter == 0)
	{
		// Начало комбо - выбираем случайную стартовую атаку
		if (StarterAttacks.Num() > 0)
		{
			int32 RandomIndex = FMath::RandRange(0, StarterAttacks.Num() - 1);
			SelectedAttack = StarterAttacks[RandomIndex];
		}
		else
		{
			SelectedAttack = EAxeAttackType::LightStandingRight;
		}
	}
	else
	{
		// Продолжение комбо - выбираем следующую атаку
		SelectedAttack = SelectNextAttack(CurrentAttackType, bHeavyAttackInput);
	}

	CurrentAttackType = SelectedAttack;

	// Получаем данные атаки
	FAxeAttackData* AttackData = GetAttackData(SelectedAttack);
	if (!AttackData || !AttackData->AttackMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("GA_MeleeAttack: No attack data or montage for attack type %d"), (int32)SelectedAttack);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Увеличиваем счетчик комбо
	ComboCounter++;

	UE_LOG(LogTemp, Warning, TEXT("GA_MeleeAttack: Executing attack %d (Combo: %d)"), 
		(int32)SelectedAttack, ComboCounter);

	// === ОБНОВЛЯЕМ ВРЕМЯ НАЧАЛА АТАКИ В HUMAN ===
	// ВАЖНО: Это нужно для корректной работы буферизации в RequestMeleeAttack
	if (AHuman* Human = Cast<AHuman>(ActorInfo->AvatarActor.Get()))
	{
		Human->LastMeleeAbilityStartTime = GetWorld()->GetTimeSeconds();
	}

	// Запоминаем время начала атаки для буферизации
	AttackStartTime = GetWorld()->GetTimeSeconds();

	// === УСТАНАВЛИВАЕМ УРОН АТАКИ ===
	// Рассчитываем урон с учётом комбо-бонуса
	float FinalDamage = AttackData->Damage * (1.0f + (ComboCounter - 1) * ComboBonus);
	
	if (AHuman* Human = Cast<AHuman>(ActorInfo->AvatarActor.Get()))
	{
		Human->SetNextAttackDamage(FinalDamage);
		UE_LOG(LogTemp, Warning, TEXT("GA_MeleeAttack: Set damage %.1f (Base: %.1f, Combo: %d, Bonus: %.2f)"), 
			FinalDamage, AttackData->Damage, ComboCounter, ComboBonus);
	}

	// === MOTION WARPING ===
	// Подстраиваем позицию к цели перед атакой
	if (AHuman* Human = Cast<AHuman>(ActorInfo->AvatarActor.Get()))
	{
		Human->WarpAttack(300.f, 10.f); // Радиус 200, дистанция 100
	}

	// Запускаем анимацию
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackData->AttackMontage,
		1.0f
	);

	if (!Task)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ВАЖНО: Используем OnBlendOut вместо OnCompleted!
	// OnBlendOut срабатывает когда начинается возврат в стойку
	// Это позволяет начать следующую атаку раньше (более быстрое комбо)
	Task->OnBlendOut.AddDynamic(this, &UGA_MeleeAttack::OnMontageBlendOut);
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

void UGA_MeleeAttack::OnMontageBlendOut()
{
	// OnBlendOut вызывается когда начинается возврат в стойку
	// Это идеальный момент для проверки буфера и начала следующей атаки!

	UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: OnBlendOut - attack animation blending out"));

	// Проверяем флаг буфера из Human (он устанавливается в RequestMeleeAttack)
	bool bHasBufferedInput = false;
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
	{
		if (AHuman* Human = Cast<AHuman>(CurrentActorInfo->AvatarActor.Get()))
		{
			bHasBufferedInput = Human->bAttackInputBuffered;
		}
	}

	// === ОБРАБОТКА БУФЕРА АТАКИ ===
	if (bHasBufferedInput && ComboCounter < MaxComboLength)
	{
		// Продолжаем комбо!
		// ВАЖНО: НЕ сбрасываем ComboCounter - он нужен для следующей атаки
		UE_LOG(LogTemp, Warning, TEXT("GA_MeleeAttack: Buffered input detected - combo will continue (Combo: %d)"), ComboCounter);
		
		// НЕ сбрасываем таймер комбо и счетчик - они понадобятся для следующей атаки
	}
	else
	{
		// Нет буферизированного ввода - открываем окно комбо
		FAxeAttackData* AttackData = GetAttackData(CurrentAttackType);
		if (AttackData && ComboCounter < MaxComboLength)
		{
			UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: Opening combo window for %.2f seconds"), 
				AttackData->ComboWindowDuration);
			
			GetWorld()->GetTimerManager().SetTimer(ComboWindowTimer, this, &UGA_MeleeAttack::ResetCombo, 
				AttackData->ComboWindowDuration, false);
		}
		else
		{
			ResetCombo();
		}
	}
	
	// Уведомляем Human о завершении атаки (для обработки буфера на следующем тике)
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
	{
		if (AHuman* Human = Cast<AHuman>(CurrentActorInfo->AvatarActor.Get()))
		{
			Human->OnMeleeAttackAbilityEnded();
		}
	}

	// ВАЖНО: Завершаем абилку чтобы снять тег State.Combat.Attacking
	// Это позволит следующей атаке из буфера корректно активироваться
	UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: OnBlendOut - Ending ability to remove Attacking tag"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_MeleeAttack::OnMontageCompleted()
{
	// OnCompleted вызывается когда анимация полностью завершена
	UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: OnCompleted - animation fully finished"));
	
	// Проверяем что абилка еще активна (могла быть завершена в OnBlendOut)
	if (IsActive())
	{
		UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: OnCompleted - Ending ability"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: OnCompleted - Ability already ended (was ended in OnBlendOut)"));
	}
}

void UGA_MeleeAttack::OnMontageCancelled()
{
	if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
	{
		if (AHuman* Human = Cast<AHuman>(CurrentActorInfo->AvatarActor.Get()))
		{
			Human->StopMeleeTrace();
		}
	}
	
	ResetCombo();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

EAxeAttackType UGA_MeleeAttack::SelectNextAttack(EAxeAttackType CurrentAttack, bool bHeavyInput)
{
	FAxeAttackData* CurrentData = GetAttackData(CurrentAttack);
	if (!CurrentData || CurrentData->PossibleFollowUps.Num() == 0)
	{
		return StarterAttacks.Num() > 0 ? StarterAttacks[0] : EAxeAttackType::LightStandingRight;
	}

	// Если нажата сильная атака, приоритет на Heavy
	if (bHeavyInput)
	{
		for (EAxeAttackType FollowUp : CurrentData->PossibleFollowUps)
		{
			if (FollowUp == EAxeAttackType::HeavyForwardRight || 
				FollowUp == EAxeAttackType::HeavyForwardLeft ||
				FollowUp == EAxeAttackType::VerticalChop)
			{
				return FollowUp;
			}
		}
	}

	// Случайный выбор
	int32 RandomIndex = FMath::RandRange(0, CurrentData->PossibleFollowUps.Num() - 1);
	return CurrentData->PossibleFollowUps[RandomIndex];
}

FAxeAttackData* UGA_MeleeAttack::GetAttackData(EAxeAttackType AttackType)
{
	for (FAxeAttackData& Data : AttackDatabase)
	{
		if (Data.AttackType == AttackType)
		{
			return &Data;
		}
	}
	return nullptr;
}

void UGA_MeleeAttack::ResetCombo()
{
	UE_LOG(LogTemp, Log, TEXT("GA_MeleeAttack: Combo reset (%d hits)"), ComboCounter);
	
	ComboCounter = 0;
	bHeavyAttackInput = false;
	CurrentAttackType = EAxeAttackType::LightStandingRight;
	
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimer);
	}
}


