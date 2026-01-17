#include "HumanAttributeSet.h"
#include "Net/UnrealNetwork.h"

UHumanAttributeSet::UHumanAttributeSet()
{
}

void UHumanAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHumanAttributeSet, MoveSpeed, OldValue);
}

void UHumanAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UHumanAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
}
