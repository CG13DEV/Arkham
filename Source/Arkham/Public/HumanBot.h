#pragma once

#include "CoreMinimal.h"
#include "Human.h"
#include "HumanBot.generated.h"

/**
 * AI Bot на базе Human с поведением через State Tree
 * - Автоматически обнаруживает игрока (через AI Perception в контроллере)
 * - Преследует цель
 * - Атакует в ближнем бою
 */
UCLASS()
class ARKHAM_API AHumanBot : public AHuman
{
	GENERATED_BODY()

public:
	AHumanBot();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Blueprint Event: Вызывается после полной инициализации бота для запуска State Tree */
	UFUNCTION(BlueprintImplementableEvent, Category="AI")
	void OnBotInitialized();

	/** Получить текущую цель бота */
	UFUNCTION(BlueprintCallable, Category="AI")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/** Установить цель вручную */
	UFUNCTION(BlueprintCallable, Category="AI")
	void SetTarget(AActor* NewTarget);

	/** Очистить текущую цель */
	UFUNCTION(BlueprintCallable, Category="AI")
	void ClearTarget();

	/** Проверка, находится ли цель в радиусе атаки */
	UFUNCTION(BlueprintCallable, Category="AI")
	bool IsTargetInAttackRange() const;

	/** Проверка, видит ли бот цель */
	UFUNCTION(BlueprintCallable, Category="AI")
	bool CanSeeTarget() const;

	/** Выполнить атаку (вызывается из State Tree Task) */
	UFUNCTION(BlueprintCallable, Category="AI")
	void ExecuteAttack();

	/** Проверка, может ли бот атаковать (не на кулдауне) */
	UFUNCTION(BlueprintCallable, Category="AI")
	bool CanAttack() const;

	/** Начать бег к цели */
	UFUNCTION(BlueprintCallable, Category="AI")
	void StartRunningToTarget();

	/** Остановить бег */
	UFUNCTION(BlueprintCallable, Category="AI")
	void StopRunningToTarget();

	/** Получить дистанцию начала бега */
	FORCEINLINE float GetRunToTargetDistance() const { return RunToTargetDistance; }

	/** Получить дистанцию остановки бега */
	FORCEINLINE float GetStopRunDistance() const { return StopRunDistance; }

protected:
	/** Текущая цель для атаки */
	UPROPERTY(BlueprintReadOnly, Replicated, Category="AI")
	TObjectPtr<AActor> CurrentTarget;

	/** Дистанция атаки */
	UPROPERTY(EditDefaultsOnly, Category="AI|Combat")
	float AttackDistance = 150.f;

	/** Минимальное время между атаками */
	UPROPERTY(EditDefaultsOnly, Category="AI|Combat")
	float AttackCooldown = 1.5f;

	/** Дистанция на которой бот начинает бежать к цели */
	UPROPERTY(EditDefaultsOnly, Category="AI|Movement")
	float RunToTargetDistance = 300.f;

	/** Дистанция на которой бот перестает бежать (близко к цели) */
	UPROPERTY(EditDefaultsOnly, Category="AI|Movement")
	float StopRunDistance = 200.f;

	/** Время последней атаки */
	float LastAttackTime = 0.f;


	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

