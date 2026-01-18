#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Human.generated.h"

class UAbilitySystemComponent;
class UHumanAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UMeleeTraceComponent;
class UTargetLockComponent;
class UMotionWarpingComponent;

UCLASS()
class AHuman : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AHuman();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Публичный API для игрока/бота:
	UFUNCTION(BlueprintCallable, Category="Movement|Run")
	void StartRun();

	UFUNCTION(BlueprintCallable, Category="Movement|Run")
	void StopRun();

	UFUNCTION(BlueprintCallable, Category="Movement|Run")
	bool IsRunning() const;

	/** Направление взгляда относительно персонажа для анимации поворота головы [-180, 180] */
	UFUNCTION(BlueprintCallable, Category="Animation")
	float GetDirectionView() const { return DirectionView; }

	// === Target Lock ===
	UFUNCTION(BlueprintCallable, Category="Combat|TargetLock")
	UTargetLockComponent* GetTargetLockComponent() const { return TargetLockComponent; }

	UFUNCTION(BlueprintCallable, Category="Combat|TargetLock")
	bool IsTargetLocked() const;

	UFUNCTION(BlueprintCallable, Category="Combat|TargetLock")
	AActor* GetLockedTarget() const;

	// === Здоровье ===
	UFUNCTION(BlueprintCallable, Category="Health")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category="Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category="Health")
	bool IsDead() const { return bIsDead; }

	/** Старый API: просто активирует GA по тегу */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void PerformMeleeAttack();

	/** Новый API: "как в Thug" но без направлений: буфер ввода + повтор после завершения текущей атаки */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void RequestMeleeAttack();

	/** Вызывается из Montage Notify / NotifyState для запуска/остановки трейса */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void StartMeleeTrace();

	UFUNCTION(BlueprintCallable, Category="Combat")
	void StopMeleeTrace();

	/** Установить урон для следующей атаки (вызывается из абилки перед анимацией) */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetNextAttackDamage(float Damage);

	/**
	/** При экипировке мили-оружия — передай сюда актор оружия.
	 * Мы возьмём у него UMeleeTraceComponent и будем использовать его вместо Unarmed.
	 */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetMeleeTraceSourceActor(AActor* InSourceActor);

	/** Motion Warping для атак - подстраивает позицию к цели */
	UFUNCTION(BlueprintCallable, Category="Combat")
	void WarpAttack(float Radius, float Distance);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	// --- GAS ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UHumanAttributeSet> Attributes;

	// --- Target Lock ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UTargetLockComponent> TargetLockComponent;

	// --- Motion Warping ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UMotionWarpingComponent> MotionWarping;

	UPROPERTY(EditDefaultsOnly, Category="GAS")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly, Category="GAS")
	TSubclassOf<UGameplayEffect> DefaultAttributesEffect;

	bool bASCReady = false;
	bool bAbilitiesGiven = false;

	void InitASC();
	void GiveDefaultAbilities();
	void ApplyDefaultAttributes();
	void BindASCDelegates();

	// Делегаты
	void OnMoveSpeedChanged(const struct FOnAttributeChangeData& Data);
	void OnRunningTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void OnDeathTagAdded(const FGameplayTag Tag, int32 NewCount);

	// Обработка смерти
	void HandleDeath();
	void EnableRagdoll();

	// Включение поворота “в камеру” (ходьба) / “в движение” (бег)
	void ApplyLocomotionConfig(bool bRunning);

	/** Обновление DirectionView и поворота персонажа */
	void UpdateRotation(float DeltaTime);

	// Теги
	FGameplayTag Tag_State_Running;
	FGameplayTag Tag_Ability_Run;
	FGameplayTag Tag_Ability_MeleeAttack;
	FGameplayTag Tag_State_Dead;
	FGameplayTag Tag_State_Attacking;

	// Параметры
	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float WalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float RunRotationRate = 1000.f;

	/** Скорость поворота персонажа при ходьбе (градусов/сек) */
	UPROPERTY(EditDefaultsOnly, Category="Movement")
	float WalkRotationRate = 500.f;

	/** Скорость интерполяции DirectionView */
	UPROPERTY(EditDefaultsOnly, Category="Animation")
	float DirectionViewInterpSpeed = 10.f;

	/** Монтаж анимации получения урона */
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TObjectPtr<UAnimMontage> HitReactionMontage;

	/** Монтаж анимации смерти (опционально, перед рэгдоллом) */
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** Флаг смерти */
	bool bIsDead = false;

	/** Флаг проигрывания Death Montage (блокирует поворот для Root Motion) */
	bool bIsPlayingDeathMontage = false;

	// === Ближний бой (без направлений) ===
	bool bAttackInputBuffered = false;
	float LastMeleeAbilityStartTime = -1000.f;
	float CurrentAttackDamage = 0.f;

	/** Через сколько секунд после старта атаки разрешаем буфер (чтобы не словить двойной триггер на один клик) */
	UPROPERTY(EditDefaultsOnly, Category="Combat")
	float MinAttackTimeBeforeBuffer = 0.05f;

	/** Трейсер кулака */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMeleeTraceComponent> UnarmedMeleeTrace;

	/** Трейсер активного оружия (если задан через SetMeleeTraceSourceActor) */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Combat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMeleeTraceComponent> ActiveWeaponMeleeTrace;

	TWeakObjectPtr<AActor> MeleeTraceSourceActor;

	UMeleeTraceComponent* ResolveMeleeTraceComponent() const;

	/** Вызывается GA_MeleeAttack при завершении монтажа (для обработки буфера ввода) */
	void OnMeleeAttackAbilityEnded();
	
	/** Вызывается при BlendOut Death Montage для включения ragdoll */
	void OnDeathMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	friend class UGA_MeleeAttack;

private:
	/** Направление взгляда относительно персонажа (для анимации головы) */
	float DirectionView = 0.f;
};
