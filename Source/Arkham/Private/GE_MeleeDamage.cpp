#include "GE_MeleeDamage.h"
#include "HumanAttributeSet.h"

UGE_MeleeDamage::UGE_MeleeDamage()
{
	// Мгновенный эффект
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// Модификатор урона
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UHumanAttributeSet::GetDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	
	// Урон настраивается через SetByCaller с тегом "Data.Damage"
	FSetByCallerFloat DamageSetByCaller;
	DamageSetByCaller.DataTag = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"));
	
	FGameplayEffectModifierMagnitude ModMagnitude(DamageSetByCaller);
	DamageModifier.ModifierMagnitude = ModMagnitude;

	Modifiers.Add(DamageModifier);
}

