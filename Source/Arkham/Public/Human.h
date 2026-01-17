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

	// Включение поворота “в камеру” (ходьба) / “в движение” (бег)
	void ApplyLocomotionConfig(bool bRunning);

	/** Обновление DirectionView и поворота персонажа */
	void UpdateRotation(float DeltaTime);

	// Теги
	FGameplayTag Tag_State_Running;
	FGameplayTag Tag_Ability_Run;

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

private:
	/** Направление взгляда относительно персонажа (для анимации головы) */
	float DirectionView = 0.f;
};
