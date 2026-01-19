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
	if (CurrentLevel < SpawnAtLevel)
	{
		return nullptr;
	}

	if (!BotClass)
	{
		return nullptr;
	}

	// Проверяем, существует ли уже заспавненный бот
	if (SpawnedBot && SpawnedBot->IsValidLowLevel())
	{
		return SpawnedBot;
	}

	// Обнуляем ссылку, если бот был уничтожен
	SpawnedBot = nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	FTransform SpawnTransform = GetActorTransform();
	SpawnedBot = GetWorld()->SpawnActor<AHumanBot>(BotClass, SpawnTransform, SpawnParams);

	if (SpawnedBot)
	{
		UE_LOG(LogTemp, Log, TEXT("BotSpawner: Spawned bot '%s' at level %d (required level: %d)"),
			*SpawnedBot->GetName(), CurrentLevel, SpawnAtLevel);
			
		FTimerHandle CheckControllerTimer;
		GetWorld()->GetTimerManager().SetTimerForNextTick([this, WeakBot = TWeakObjectPtr<AHumanBot>(SpawnedBot)]()
		{
			if (WeakBot.IsValid())
			{
				AHumanBot* Bot = WeakBot.Get();
				if (!Bot->GetController())
				{
					Bot->SpawnDefaultController();
				}
			}
		});
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

