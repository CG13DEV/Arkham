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

	/** Время последней атаки */
	float LastAttackTime = 0.f;


	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

