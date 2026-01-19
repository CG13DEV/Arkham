#include "HumanBotController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "HumanBot.h"

AHumanBotController::AHumanBotController()
{
	PrimaryActorTick.bCanEverTick = true;

	// Создаём AI Perception компонент в контроллере (ВАЖНО!)
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	SetPerceptionComponent(*AIPerception);

	// Настраиваем зрение
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	if (SightConfig)
	{
		SightConfig->SightRadius = 1500.f;
		SightConfig->LoseSightRadius = 2000.f;
		SightConfig->PeripheralVisionAngleDegrees = 90.f;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
		SightConfig->SetMaxAge(5.0f);

		AIPerception->ConfigureSense(*SightConfig);
		AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	}
}

void AHumanBotController::BeginPlay()
{
	Super::BeginPlay();

	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AHumanBotController::OnPerceptionUpdated);
	}
}

void AHumanBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void AHumanBotController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AHumanBot* Bot = Cast<AHumanBot>(GetPawn());
	if (!Bot || Bot->IsDead())
		return;

	AActor* Target = Bot->GetCurrentTarget();
	if (!Target)
	{
		Bot->StopRunningToTarget();
		return;
	}

	float DistanceToTarget = FVector::Dist(Bot->GetActorLocation(), Target->GetActorLocation());

	if (DistanceToTarget > Bot->GetRunToTargetDistance())
	{
		Bot->StartRunningToTarget();
	}
	else if (DistanceToTarget < Bot->GetStopRunDistance())
	{
		Bot->StopRunningToTarget();
	}
}

void AHumanBotController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
		return;

	AHumanBot* Bot = Cast<AHumanBot>(GetPawn());
	if (!Bot || Bot->IsDead())
		return;

	// Проверяем, что это игрок (или другой Human)
	AHuman* TargetHuman = Cast<AHuman>(Actor);
	if (!TargetHuman)
		return;

	// Игнорируем других ботов и мёртвых персонажей
	if (Cast<AHumanBot>(Actor) || TargetHuman->IsDead())
		return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// Обнаружили цель
		Bot->SetTarget(Actor);
	}
	else
	{
		// Потеряли цель
		if (Bot->GetCurrentTarget() == Actor)
		{
			Bot->ClearTarget();
		}
	}
}

