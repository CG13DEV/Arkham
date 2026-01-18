#include "HumanAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

UHumanAttributeSet::UHumanAttributeSet()
{
	// Инициализация начальных значений
	InitHealth(100.f);
	InitMaxHealth(100.f);
}

void UHumanAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHumanAttributeSet, MoveSpeed, OldValue);
}

void UHumanAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHumanAttributeSet, Health, OldValue);
}

void UHumanAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHumanAttributeSet, MaxHealth, OldValue);
}

void UHumanAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UHumanAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHumanAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHumanAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UHumanAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Клампим здоровье между 0 и MaxHealth
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UHumanAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Обработка мета-атрибута Damage
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float LocalDamage = GetDamage();
		SetDamage(0.f); // Сбрасываем мета-атрибут

		if (LocalDamage > 0.f)
		{
			// Применяем урон к здоровью
			const float NewHealth = FMath::Max(GetHealth() - LocalDamage, 0.f);
			SetHealth(NewHealth);
		}
	}

	// Клампим здоровье после любых изменений
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}
