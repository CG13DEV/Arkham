#include "HumanBot.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"
#include "HumanBotController.h"
#include "Perception/AIPerceptionComponent.h"

AHumanBot::AHumanBot()
{
	PrimaryActorTick.bCanEverTick = false;

	// Устанавливаем AI Controller класс
	AIControllerClass = AHumanBotController::StaticClass();
	
	// Автоматически владеть AI контроллером
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AHumanBot::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("HumanBot: %s spawned and ready"), *GetName());
}

void AHumanBot::SetTarget(AActor* NewTarget)
{
	if (CurrentTarget == NewTarget)
		return;

	CurrentTarget = NewTarget;
}
void AHumanBot::ClearTarget()
{
	CurrentTarget = nullptr;
	
	UE_LOG(LogTemp, Warning, TEXT("HumanBot: %s cleared target"), *GetName());
}

bool AHumanBot::IsTargetInAttackRange() const
{
	if (!CurrentTarget)
		return false;

	const float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
	return Distance <= AttackDistance;
}

bool AHumanBot::CanSeeTarget() const
{
	if (!CurrentTarget)
		return false;

	// Получаем AI контроллер
	AHumanBotController* BotController = Cast<AHumanBotController>(GetController());
	if (!BotController)
		return false;

	UAIPerceptionComponent* PerceptionComp = BotController->GetAIPerceptionComponent();
	if (!PerceptionComp)
		return false;

	FActorPerceptionBlueprintInfo Info;
	PerceptionComp->GetActorsPerception(CurrentTarget, Info);

	for (const FAIStimulus& Stimulus : Info.LastSensedStimuli)
	{
		if (Stimulus.WasSuccessfullySensed())
			return true;
	}

	return false;
}

bool AHumanBot::CanAttack() const
{
	if (bIsDead || !CurrentTarget)
		return false;

	// Проверяем кулдаун
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	return (CurrentTime - LastAttackTime) >= AttackCooldown;
}

void AHumanBot::ExecuteAttack()
{
	if (!CurrentTarget || bIsDead || !CanAttack())
	{
		UE_LOG(LogTemp, Warning, TEXT("HumanBot: %s cannot attack (Target=%s, Dead=%s, CanAttack=%s)"),
			*GetName(),
			CurrentTarget ? *CurrentTarget->GetName() : TEXT("NULL"),
			bIsDead ? TEXT("YES") : TEXT("NO"),
			CanAttack() ? TEXT("YES") : TEXT("NO"));
		return;
	}

	// Поворачиваемся к цели
	const FVector Direction = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(Direction.Rotation());
	}

	// Выполняем атаку через GAS
	PerformMeleeAttack();
	LastAttackTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Warning, TEXT("HumanBot: %s attacking target: %s"), *GetName(), *CurrentTarget->GetName());
}

void AHumanBot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHumanBot, CurrentTarget);
}

