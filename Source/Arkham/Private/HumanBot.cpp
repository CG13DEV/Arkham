#include "HumanBot.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"
#include "HumanBotController.h"
#include "Perception/AIPerceptionComponent.h"
#include "TargetLockComponent.h"

AHumanBot::AHumanBot()
{
	// ВАЖНО: Включаем Tick для бота (для UpdateRotation и других систем)
	PrimaryActorTick.bCanEverTick = true;

	// Устанавливаем AI Controller класс
	AIControllerClass = AHumanBotController::StaticClass();
	
	// Автоматически владеть AI контроллером
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AHumanBot::BeginPlay()
{
	Super::BeginPlay();
	
	if (GetController())
	{
		OnBotInitialized();
	}
	else
	{
		SpawnDefaultController();
		
		FTimerHandle RetryTimer;
		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			if (GetController())
			{
				OnBotInitialized();
			}
		});
	}
}

void AHumanBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Простое AI управление как fallback если State Tree не работает
	if (CurrentTarget && !bIsDead)
	{
		AAIController* AIC = Cast<AAIController>(GetController());
		if (AIC)
		{
			float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
			
			if (Distance > AttackDistance + 50.f)
			{
				AIC->MoveToActor(CurrentTarget, AttackDistance);
				
				if (Distance > RunToTargetDistance)
				{
					StartRunningToTarget();
				}
				else if (Distance < StopRunDistance)
				{
					StopRunningToTarget();
				}
			}
			else
			{
				AIC->StopMovement();
				StopRunningToTarget();
				
				if (CanAttack())
				{
					ExecuteAttack();
				}
			}
		}
	}
}

void AHumanBot::SetTarget(AActor* NewTarget)
{
	if (CurrentTarget == NewTarget)
		return;

	CurrentTarget = NewTarget;

	// Используем TargetLockComponent для автоматического поворота к цели
	if (TargetLockComponent)
	{
		TargetLockComponent->SetTarget(NewTarget, true);
	}
}
void AHumanBot::ClearTarget()
{
	CurrentTarget = nullptr;
	
	// Очищаем Target Lock
	if (TargetLockComponent)
	{
		TargetLockComponent->ClearTarget();
	}
	
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
		return;
	}

	PerformMeleeAttack();
	LastAttackTime = GetWorld()->GetTimeSeconds();
}

void AHumanBot::StartRunningToTarget()
{
	if (!IsRunning())
	{
		StartRun();
		UE_LOG(LogTemp, Log, TEXT("HumanBot: %s started running to target"), *GetName());
	}
}

void AHumanBot::StopRunningToTarget()
{
	if (IsRunning())
	{
		StopRun();
		UE_LOG(LogTemp, Log, TEXT("HumanBot: %s stopped running"), *GetName());
	}
}

void AHumanBot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHumanBot, CurrentTarget);
}

