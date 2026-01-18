#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetLockComponent.generated.h"

class AHuman;

/**
 * Компонент для системы Target Lock (блокировка на цель)
 * - Поиск ближайших целей в радиусе
 * - Переключение между целями
 * - Автоматический поворот персонажа к цели
 * - Режим Strafe движения
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class ARKHAM_API UTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetLockComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Включить/выключить Target Lock */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void ToggleTargetLock();

	/** Включить Target Lock */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void EnableTargetLock();

	/** Выключить Target Lock */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void DisableTargetLock();

	/** Переключиться на следующую цель (вправо) */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void SwitchTargetRight();

	/** Переключиться на предыдущую цель (влево) */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void SwitchTargetLeft();

	/** Проверка активности Target Lock */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	bool IsTargetLocked() const { return bIsLocked && CurrentTarget != nullptr; }

	/** Получить текущую цель */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/** Установить цель вручную (для AI) */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void SetTarget(AActor* NewTarget, bool bAutoEnable = true);

	/** Очистить цель */
	UFUNCTION(BlueprintCallable, Category="TargetLock")
	void ClearTarget();

protected:
	virtual void BeginPlay() override;

	/** Найти все доступные цели в радиусе */
	TArray<AActor*> FindAvailableTargets() const;

	/** Найти ближайшую цель */
	AActor* FindClosestTarget() const;

	/** Найти следующую цель в направлении (вправо/влево относительно камеры) */
	AActor* FindTargetInDirection(bool bRight) const;

	/** Проверка валидности цели (жива, в радиусе, видима) */
	bool IsTargetValid(AActor* Target) const;

	/** Обновить поворот владельца к цели */
	void UpdateRotationToTarget(float DeltaTime);

	/** Владелец компонента */
	UPROPERTY()
	TObjectPtr<AHuman> OwnerHuman;

	/** Текущая заблокированная цель */
	UPROPERTY(BlueprintReadOnly, Category="TargetLock")
	TObjectPtr<AActor> CurrentTarget;

	/** Флаг активности Target Lock */
	UPROPERTY(BlueprintReadOnly, Category="TargetLock")
	bool bIsLocked;

	/** Максимальная дистанция для поиска целей */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock")
	float MaxLockDistance;

	/** Максимальная дистанция для удержания цели (чуть больше чем поиск) */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock")
	float MaxLockBreakDistance;

	/** Угол конуса для поиска целей (градусы) */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock")
	float LockAngle;

	/** Скорость поворота к цели при Target Lock (градусов/сек) */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock")
	float RotationSpeed;

	/** Высота offset для определения направления к цели (центр масс) */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock")
	float TargetHeightOffset;

	/** Включить автоматическую активацию Target Lock при приближении врага */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock|Auto")
	bool bEnableAutoLock;

	/** Дистанция на которой автоматически активируется Target Lock */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock|Auto", meta=(EditCondition="bEnableAutoLock"))
	float AutoLockDistance;

	/** Флаг: игрок вручную выключил Target Lock (отключает авто-активацию до следующего ручного включения) */
	bool bManuallyDisabled;

	/** Время последней проверки авто-активации */
	float LastAutoCheckTime;

	/** Интервал проверки авто-активации (секунды) */
	UPROPERTY(EditDefaultsOnly, Category="TargetLock|Auto", meta=(EditCondition="bEnableAutoLock"))
	float AutoCheckInterval;

	/** Проверка и активация авто Target Lock */
	void CheckAutoLock();
};

