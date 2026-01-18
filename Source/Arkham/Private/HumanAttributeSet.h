#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "HumanAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class UHumanAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UHumanAttributeSet();

	// Скорость движения, которую мы пробрасываем в CharacterMovement->MaxWalkSpeed
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MoveSpeed, Category="Movement")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UHumanAttributeSet, MoveSpeed);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

	// Здоровье
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category="Health")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UHumanAttributeSet, Health);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	// Максимальное здоровье
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category="Health")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UHumanAttributeSet, MaxHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	// Meta-атрибут для получения урона (не реплицируется, используется в Execution Calculations)
	UPROPERTY(BlueprintReadOnly, Category="Health")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UHumanAttributeSet, Damage);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Клампинг здоровья
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
};
