#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_MeleeDamage.generated.h"

/**
 * GameplayEffect для нанесения урона в ближнем бою
 * Использует мета-атрибут Damage, который конвертируется в Health в AttributeSet
 */
UCLASS()
class UGE_MeleeDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_MeleeDamage();
};

