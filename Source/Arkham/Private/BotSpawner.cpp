#include "BotSpawner.h"
#include "HumanBot.h"
#include "Components/BillboardComponent.h"

ABotSpawner::ABotSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// Визуализация в редакторе
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

#if WITH_EDITORONLY_DATA
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(RootComponent);
#endif
}

void ABotSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	// Спавнеры не спавнят ботов в BeginPlay
	// Это делает GameMode в зависимости от уровня
}

AHumanBot* ABotSpawner::SpawnBot(int32 CurrentLevel)
{
	// Проверяем что уровень подходит
	if (CurrentLevel < SpawnAtLevel)
	{
		UE_LOG(LogTemp, Log, TEXT("BotSpawner: Level %d < SpawnAtLevel %d - not spawning"), 
			CurrentLevel, SpawnAtLevel);
		return nullptr;
	}

	// Проверяем что класс установлен
	if (!BotClass)
	{
		UE_LOG(LogTemp, Error, TEXT("BotSpawner: BotClass is NULL!"));
		return nullptr;
	}

	// Проверяем что еще не заспавнили
	if (SpawnedBot && SpawnedBot->IsValidLowLevel())
	{
		UE_LOG(LogTemp, Warning, TEXT("BotSpawner: Bot already spawned!"));
		return SpawnedBot;
	}

	// Спавним бота
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	FTransform SpawnTransform = GetActorTransform();
	SpawnedBot = GetWorld()->SpawnActor<AHumanBot>(BotClass, SpawnTransform, SpawnParams);

	if (SpawnedBot)
	{
		UE_LOG(LogTemp, Warning, TEXT("🤖 BotSpawner: Spawned bot %s at level %d (Location: %s)"), 
			*SpawnedBot->GetName(), CurrentLevel, *SpawnTransform.GetLocation().ToString());
		
		// ВАЖНО: Проверяем что AI контроллер создался
		// Иногда нужно небольшая задержка для инициализации
		FTimerHandle CheckControllerTimer;
		GetWorld()->GetTimerManager().SetTimerForNextTick([this, WeakBot = TWeakObjectPtr<AHumanBot>(SpawnedBot)]()
		{
			if (WeakBot.IsValid())
			{
				AHumanBot* Bot = WeakBot.Get();
				if (Bot->GetController())
				{
					UE_LOG(LogTemp, Warning, TEXT("  ✓ Bot %s has controller: %s"), 
						*Bot->GetName(), *Bot->GetController()->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("  ✗ Bot %s has NO controller! AI will not work!"), 
						*Bot->GetName());
					
					// Пытаемся создать контроллер вручную
					Bot->SpawnDefaultController();
					
					if (Bot->GetController())
					{
						UE_LOG(LogTemp, Warning, TEXT("  ✓ Controller spawned manually for %s"), *Bot->GetName());
					}
				}
			}
		});
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BotSpawner: Failed to spawn bot!"));
	}

	return SpawnedBot;
}

#if WITH_EDITOR
void ABotSpawner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Обновляем метку в редакторе при изменении SpawnAtLevel
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ABotSpawner, SpawnAtLevel))
	{
		FString NewLabel = FString::Printf(TEXT("BotSpawner_Level%d"), SpawnAtLevel);
		SetActorLabel(NewLabel);
	}
}
#endif

