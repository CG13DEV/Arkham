#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MeleeAttack.generated.h"

/**
 * Тип атаки топором
 */
UENUM(BlueprintType)
enum class EAxeAttackType : uint8
{
	// Атаки на месте
	LightStandingRight UMETA(DisplayName = "Light Standing Right"),  // Замах справа на месте
	LightStandingLeft UMETA(DisplayName = "Light Standing Left"),    // Замах слева на месте
	
	// Атаки с движением вперед
	LightForwardRight UMETA(DisplayName = "Light Forward Right"),    // Атака вперед справа
	LightForwardLeft UMETA(DisplayName = "Light Forward Left"),      // Атака вперед слева
	
	// Сильные атаки с движением
	HeavyForwardRight UMETA(DisplayName = "Heavy Forward Right"),    // Сильный удар вперед справа
	HeavyForwardLeft UMETA(DisplayName = "Heavy Forward Left"),      // Сильный удар вперед слева
	
	// Специальная атака
	VerticalChop UMETA(DisplayName = "Vertical Chop")               // Рубящий удар сверху вниз
};

/**
 * Данные одной атаки в комбо
 */
USTRUCT(BlueprintType)
struct FAxeAttackData
{
	GENERATED_BODY()

	/** Тип атаки */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EAxeAttackType AttackType;

	/** Монтаж анимации */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Урон атаки */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Damage = 20.f;

	/** Множитель урона для комбо (увеличивается с каждой атакой) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ComboDamageMultiplier = 1.0f;

	/** Окно для следующей атаки в комбо (секунды после завершения монтажа) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ComboWindowDuration = 0.8f;

	/** Возможные следующие атаки в комбо */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<EAxeAttackType> PossibleFollowUps;

	FAxeAttackData()
		: AttackType(EAxeAttackType::LightStandingRight)
		, Damage(20.f)
		, ComboDamageMultiplier(1.0f)
		, ComboWindowDuration(0.8f)
	{}
};

/**
 * Способность атаки топором с комбо-системой
 * - 7 разных типов атак
 * - Цепочки комбо
 * - Окно для продолжения комбо
 * - Увеличение урона в комбо
 */
UCLASS()
class UGA_MeleeAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MeleeAttack();

	/** База данных всех атак */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Attacks")
	TArray<FAxeAttackData> AttackDatabase;

	/** Стартовые атаки (с которых можно начать комбо) */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Combo")
	TArray<EAxeAttackType> StarterAttacks;

	/** Максимальная длина комбо */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Combo")
	int32 MaxComboLength = 5;

	/** Множитель урона за каждую атаку в комбо */
	UPROPERTY(EditDefaultsOnly, Category="Combat|Combo")
	float ComboBonus = 0.15f; // +15% урона за каждую атаку

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
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageCancelled();

	/** Выбрать следующую атаку в комбо */
	EAxeAttackType SelectNextAttack(EAxeAttackType CurrentAttack, bool bHeavyInput);

	/** Получить данные атаки по типу */
	FAxeAttackData* GetAttackData(EAxeAttackType AttackType);

	/** Сбросить комбо */
	void ResetCombo();

private:
	/** Текущая атака в комбо */
	EAxeAttackType CurrentAttackType;

	/** Счетчик комбо */
	int32 ComboCounter;

	/** Флаг - зажата кнопка сильной атаки */
	bool bHeavyAttackInput;

	/** Время начала текущей атаки (для буферизации) */
	float AttackStartTime;

	/** Таймер окна комбо */
	FTimerHandle ComboWindowTimer;
};


