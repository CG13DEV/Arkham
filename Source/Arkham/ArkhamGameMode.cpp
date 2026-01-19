// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArkhamGameMode.h"
#include "HumanBot.h"
#include "BotSpawner.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

AArkhamGameMode::AArkhamGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}

void AArkhamGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	CachedBots.Empty(); // Очищаем список перед началом игры
	
	CacheBotSpawners();
	SpawnBotsForCurrentLevel();
	
	if (!bLevelScreenShown)
	{
		bLevelScreenShown = true;
		OnCurrentLevel(CurrentLevel);
	}
}

void AArkhamGameMode::ResetAllBotsToStart()
{
	UE_LOG(LogTemp, Warning, TEXT("🎮 ResetAllBotsToStart: Resetting %d bots to start positions"), CachedBots.Num());

	int32 ResetCount = 0;
	
	for (AHumanBot* Bot : CachedBots)
	{
		if (!Bot || !Bot->IsValidLowLevel())
		{
			continue;
		}

		// Получаем начальную позицию из спавнера
		for (ABotSpawner* Spawner : BotSpawners)
		{
			if (Spawner && Spawner->GetSpawnedBot() == Bot)
			{
				FTransform StartTransform = Spawner->GetActorTransform();
				Bot->SetActorTransform(StartTransform);
				
				// Сбрасываем цель
				Bot->ClearTarget();
				
				// Останавливаем бег
				if (Bot->IsRunning())
				{
					Bot->StopRun();
				}
				
				UE_LOG(LogTemp, Log, TEXT("  ✓ Reset bot: %s to location %s"), 
					*Bot->GetName(), *StartTransform.GetLocation().ToString());
				ResetCount++;
				break;
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 ResetAllBotsToStart: Reset %d bots"), ResetCount);
}

void AArkhamGameMode::OnPlayerDeath(APawn* DeadPlayer)
{
	if (!DeadPlayer)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("🎮 OnPlayerDeath: Player died on level %d, restarting current level..."), CurrentLevel);

	// Сбрасываем цели у всех ботов
	ClearAllBotTargets();
	
	// Телепортируем ботов на начальные позиции
	ResetAllBotsToStart();
	
	// Респавним игрока через задержку
	FTimerHandle RestartTimerHandle;
	GetWorldTimerManager().SetTimer(RestartTimerHandle, [this, DeadPlayer]()
	{
		RespawnPlayer(DeadPlayer);
	}, 2.0f, false);
}


void AArkhamGameMode::ClearAllBotTargets()
{
	int32 ClearedCount = 0;
	
	// Проходим по всем ботам на уровне
	for (TActorIterator<AHumanBot> It(GetWorld()); It; ++It)
	{
		AHumanBot* Bot = *It;
		if (Bot && Bot->IsValidLowLevel())
		{
			Bot->ClearTarget();
			
			// Останавливаем бег
			if (Bot->IsRunning())
			{
				Bot->StopRun();
			}
			
			ClearedCount++;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 ClearAllBotTargets: Cleared targets for %d bots"), ClearedCount);
}

void AArkhamGameMode::RespawnPlayer(APawn* Player)
{
	if (Player)
	{
		Player->Destroy();
	}

	// Находим PlayerStart
	AActor* PlayerStart = nullptr;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		PlayerStart = *It;
		break;
	}

	if (!PlayerStart)
	{
		UE_LOG(LogTemp, Error, TEXT("🎮 RespawnPlayer: No PlayerStart found!"));
		return;
	}

	// Спавним игрока на PlayerStart
	FTransform StartTransform = PlayerStart->GetActorTransform();

	APawn* NewPlayer = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, StartTransform);
	
	if (NewPlayer)
	{
		// Небольшая задержка перед Possess для корректной инициализации
		FTimerHandle PossessTimer;
		GetWorld()->GetTimerManager().SetTimer(PossessTimer, [this, NewPlayer]()
		{
			if (NewPlayer && NewPlayer->IsValidLowLevel())
			{
				APlayerController* PC = GetWorld()->GetFirstPlayerController();
				if (PC)
				{
					PC->Possess(NewPlayer);
					UE_LOG(LogTemp, Warning, TEXT("🎮 RespawnPlayer: Player possessed at %s"), 
						*NewPlayer->GetActorLocation().ToString());
				}
			}
		}, 0.1f, false);
		
		UE_LOG(LogTemp, Warning, TEXT("🎮 RespawnPlayer: Player spawned at %s"), *StartTransform.GetLocation().ToString());
	}
}

// ========================================
// Система уровней и спавнеров
// ========================================

void AArkhamGameMode::CacheBotSpawners()
{
	BotSpawners.Empty();
	MaxLevel = 0;
	
	// Находим все спавнеры на уровне
	for (TActorIterator<ABotSpawner> It(GetWorld()); It; ++It)
	{
		ABotSpawner* Spawner = *It;
		if (Spawner)
		{
			BotSpawners.Add(Spawner);
			
			// Определяем максимальный уровень
			if (Spawner->GetSpawnLevel() > MaxLevel)
			{
				MaxLevel = Spawner->GetSpawnLevel();
			}
			
			UE_LOG(LogTemp, Log, TEXT("  - Found spawner: Level %d at %s"), 
				Spawner->GetSpawnLevel(), *Spawner->GetActorLocation().ToString());
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 CacheBotSpawners: Found %d spawners, MaxLevel = %d"), 
		BotSpawners.Num(), MaxLevel);
}

void AArkhamGameMode::SpawnBotsForCurrentLevel()
{
	if (BotSpawners.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("🎮 SpawnBotsForCurrentLevel: No spawners found!"));
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 SpawnBotsForCurrentLevel: Spawning bots for level %d..."), CurrentLevel);
	
	int32 SpawnedCount = 0;
	
	for (ABotSpawner* Spawner : BotSpawners)
	{
		if (!Spawner)
			continue;
		
		// Спавним бота если уровень подходит
		AHumanBot* Bot = Spawner->SpawnBot(CurrentLevel);
		if (Bot)
		{
			// Добавляем только если его еще нет в списке
			if (!CachedBots.Contains(Bot))
			{
				CachedBots.Add(Bot);
			}
			SpawnedCount++;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 SpawnBotsForCurrentLevel: Spawned %d bots for level %d, total bots: %d"), 
		SpawnedCount, CurrentLevel, CachedBots.Num());
}

void AArkhamGameMode::NextLevel()
{
	CurrentLevel++;
	
	// Проверяем, не превысили ли максимальный уровень
	if (CurrentLevel > MaxLevel)
	{
		UE_LOG(LogTemp, Warning, TEXT("🎮 NextLevel: Completed all %d levels, finishing game..."), MaxLevel);
		FinishGame();
		return;
	}

	bLevelScreenShown = false;
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 NextLevel: Moving to level %d (MaxLevel: %d)"), 
		CurrentLevel, MaxLevel);
	
	// Сбрасываем цели у всех ботов
	ClearAllBotTargets();
	
	// Телепортируем существующих ботов на начальные позиции
	ResetAllBotsToStart();
	
	// Спавним новых ботов для этого уровня (если есть)
	SpawnBotsForCurrentLevel();
	
	// Респавним игрока
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	RespawnPlayer(PlayerPawn);
	
	// Показываем UI нового уровня
	if (!bLevelScreenShown)
	{
		bLevelScreenShown = true;
		OnCurrentLevel(CurrentLevel);
		
		UE_LOG(LogTemp, Warning, TEXT("🎮 NextLevel: OnCurrentLevel event called for level %d"), CurrentLevel);
	}
}

void AArkhamGameMode::FinishGame()
{
	UE_LOG(LogTemp, Warning, TEXT("🎮 FinishGame: Game completed! All %d levels finished!"), MaxLevel);
	
	// Вызываем Blueprint событие
	OnFinishGame();
}


