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

	// Подписываемся на события perception в контроллере
	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AHumanBotController::OnPerceptionUpdated);
		UE_LOG(LogTemp, Warning, TEXT("HumanBotController: AI Perception initialized"));
	}
}

void AHumanBotController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("HumanBotController: Possessed %s"), 
		InPawn ? *InPawn->GetName() : TEXT("NULL"));
}

void AHumanBotController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UE_LOG(LogTemp, Warning, TEXT("AHumanBotController: %s tick"), *GetName());
	AHumanBot* Bot = Cast<AHumanBot>(GetPawn());
	if (!Bot || Bot->IsDead())
		return;

	AActor* Target = Bot->GetCurrentTarget();
	if (!Target)
	{
		// Нет цели - останавливаем бег
		Bot->StopRunningToTarget();
		return;
	}

	// Вычисляем дистанцию до цели
	float DistanceToTarget = FVector::Dist(Bot->GetActorLocation(), Target->GetActorLocation());

	// Управление бегом в зависимости от дистанции
	if (DistanceToTarget > Bot->GetRunToTargetDistance())
	{
		// Далеко от цели - бежим
		Bot->StartRunningToTarget();
	}
	else if (DistanceToTarget < Bot->GetStopRunDistance())
	{
		// Близко к цели - останавливаем бег (боевая дистанция)
		Bot->StopRunningToTarget();
	}
	// Между StopRunDistance и RunToTargetDistance - сохраняем текущее состояние (гистерезис)
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
		UE_LOG(LogTemp, Warning, TEXT("HumanBotController: Detected target: %s"), *Actor->GetName());
	}
	else
	{
		// Потеряли цель
		if (Bot->GetCurrentTarget() == Actor)
		{
			Bot->ClearTarget();
			UE_LOG(LogTemp, Warning, TEXT("HumanBotController: Lost target: %s"), *Actor->GetName());
		}
	}
}

