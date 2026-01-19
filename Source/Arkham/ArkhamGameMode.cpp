// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArkhamGameMode.h"
#include "ArkhamCharacter.h"
#include "HumanBot.h"
#include "Human.h"
#include "HumanAttributeSet.h"
#include "BotSpawner.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
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
	
	// Кэшируем все спавнеры на уровне
	CacheBotSpawners();
	
	// Спавним ботов для текущего уровня
	SpawnBotsForCurrentLevel();
	
	// Показываем UI текущего уровня (только при первом запуске)
	if (!bLevelScreenShown)
	{
		bLevelScreenShown = true;
		OnCurrentLevel(CurrentLevel);
		UE_LOG(LogTemp, Warning, TEXT("🎮 ArkhamGameMode: Level %d started - UI shown"), CurrentLevel);
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 ArkhamGameMode: BeginPlay - Level %d, Spawners: %d, Bots spawned: %d"), 
		CurrentLevel, BotSpawners.Num(), CachedBots.Num());
}

void AArkhamGameMode::CacheBotStartPositions()
{
	CachedBots.Empty();
	BotStartTransforms.Empty();

	// Находим всех ботов на уровне
	for (TActorIterator<AHumanBot> It(GetWorld()); It; ++It)
	{
		AHumanBot* Bot = *It;
		if (Bot)
		{
			CachedBots.Add(Bot);
			
			// Сохраняем Transform
			TSubclassOf<AHumanBot> BotClass = Bot->GetClass();
			BotStartTransforms.Add(BotClass, Bot->GetActorTransform());
			
			UE_LOG(LogTemp, Log, TEXT("  - Cached bot: %s at location %s"), 
				*Bot->GetName(), *Bot->GetActorLocation().ToString());
		}
	}
}

void AArkhamGameMode::OnPlayerDeath(APawn* DeadPlayer)
{
	if (!DeadPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("🎮 OnPlayerDeath: DeadPlayer is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("🎮 OnPlayerDeath: Player %s died! Restarting level..."), *DeadPlayer->GetName());
	
	// Небольшая задержка перед рестартом (чтобы показать анимацию смерти)
	FTimerHandle RestartTimerHandle;
	GetWorldTimerManager().SetTimer(RestartTimerHandle, this, &AArkhamGameMode::RestartLevel, 2.0f, false);
}

void AArkhamGameMode::RestartLevel()
{
	UE_LOG(LogTemp, Warning, TEXT("🎮 RestartLevel: Starting... (Level: %d)"), CurrentLevel);
	
	// 1. Очищаем цели у всех ботов
	ClearAllBotTargets();
	
	// 2. Удаляем всех текущих ботов
	DestroyAllBots();
	
	// 3. Спавним ботов для текущего уровня через спавнеры
	SpawnBotsForCurrentLevel();
	
	// 4. Респавним игрока
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	RespawnPlayer(PlayerPawn);
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 RestartLevel: COMPLETE! (Level: %d, Bots: %d)"), 
		CurrentLevel, CachedBots.Num());
}

void AArkhamGameMode::ResetAllBots()
{
	if (CachedBots.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("🎮 ResetAllBots: No cached bots!"));
		return;
	}

	int32 ResetCount = 0;
	
	for (AHumanBot* Bot : CachedBots)
	{
		if (!Bot || !Bot->IsValidLowLevel())
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Skipping invalid bot"));
			continue;
		}

		// Получаем начальный Transform из кэша
		TSubclassOf<AHumanBot> BotClass = Bot->GetClass();
		if (BotStartTransforms.Contains(BotClass))
		{
			FTransform StartTransform = BotStartTransforms[BotClass];
			
			// Телепортируем на начальную позицию
			Bot->SetActorTransform(StartTransform);
			
			// Сбрасываем состояние бота
			if (AHuman* Human = Cast<AHuman>(Bot))
			{
				// Восстанавливаем здоровье
				UAbilitySystemComponent* ASC = Human->GetAbilitySystemComponent();
				if (ASC)
				{
					float MaxHealth = Human->GetMaxHealth();
					ASC->SetNumericAttributeBase(UHumanAttributeSet::GetHealthAttribute(), MaxHealth);
					
					// Убираем тег смерти если был
					FGameplayTagContainer DeathTags;
					DeathTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
					ASC->RemoveLooseGameplayTags(DeathTags);
					
					// Сбрасываем флаг смерти
					Human->Respawn();
				}
				
				// Отключаем ragdoll если был включен
				if (USkeletalMeshComponent* Mesh = Human->GetMesh())
				{
					Mesh->SetSimulatePhysics(false);
					Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
					Mesh->AttachToComponent(Human->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				}
				
				// Включаем коллизию капсулы
				if (UCapsuleComponent* Capsule = Human->GetCapsuleComponent())
				{
					Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				}
				
				// Включаем движение
				if (UCharacterMovementComponent* Movement = Human->GetCharacterMovement())
				{
					Movement->SetMovementMode(MOVE_Walking);
				}
			}
			
			UE_LOG(LogTemp, Log, TEXT("  ✓ Reset bot: %s to location %s"), 
				*Bot->GetName(), *StartTransform.GetLocation().ToString());
			ResetCount++;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("  - No cached transform for bot class: %s"), *BotClass->GetName());
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 ResetAllBots: Reset %d bots"), ResetCount);
}

void AArkhamGameMode::DestroyAllBots()
{
	int32 DestroyedCount = 0;
	
	for (TActorIterator<AHuman> It(GetWorld()); It; ++It)
	{
		AHuman* Bot = *It;
		if (Bot)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Destroying bot: %s"), *Bot->GetName());
			Bot->Destroy();
			DestroyedCount++;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 DestroyAllBots: Destroyed %d bots"), DestroyedCount);
}

void AArkhamGameMode::RespawnAllBots()
{
	if (BotStartTransforms.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("🎮 RespawnAllBots: No cached bot transforms!"));
		return;
	}

	int32 SpawnedCount = 0;
	
	for (const auto& Pair : BotStartTransforms)
	{
		TSubclassOf<AHumanBot> BotClass = Pair.Key;
		FTransform StartTransform = Pair.Value;
		
		if (!BotClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Skipping NULL bot class"));
			continue;
		}

		// ВАЖНО: Правильная настройка SpawnParameters для AI
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		// НЕ вызываем BeginPlay сразу - дадим AI инициализироваться
		SpawnParams.bDeferConstruction = false;
		
		AHumanBot* NewBot = GetWorld()->SpawnActor<AHumanBot>(BotClass, StartTransform, SpawnParams);
		
		if (NewBot)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Spawned bot: %s at location %s"), 
				*NewBot->GetName(), *StartTransform.GetLocation().ToString());
			
			// ВАЖНО: AI контроллер должен быть создан автоматически через AutoPossessAI
			// Проверяем что контроллер создался
			if (NewBot->GetController())
			{
				UE_LOG(LogTemp, Log, TEXT("    ✓ Bot has controller: %s"), *NewBot->GetController()->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("    ✗ Bot has NO controller! Trying SpawnDefaultController..."));
				NewBot->SpawnDefaultController();
				
				if (NewBot->GetController())
				{
					UE_LOG(LogTemp, Warning, TEXT("    ✓ Controller spawned manually"));
				}
			}
			
			SpawnedCount++;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("  - Failed to spawn bot of class: %s"), *BotClass->GetName());
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 RespawnAllBots: Spawned %d bots"), SpawnedCount);
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
		break; // Берём первый найденный
	}

	if (!PlayerStart)
	{
		UE_LOG(LogTemp, Error, TEXT("🎮 RespawnPlayer: PlayerStart not found on level!"));
		return;
	}

	// Телепортируем игрока на PlayerStart
	FTransform StartTransform = PlayerStart->GetActorTransform();

	Player = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, StartTransform);
	GetWorld()->GetFirstPlayerController()->Possess(Player);
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
			UE_LOG(LogTemp, Log, TEXT("  ✓ Cleared target for bot: %s"), *Bot->GetName());
			ClearedCount++;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 ClearAllBotTargets: Cleared targets for %d bots"), ClearedCount);
}

// ========================================
// Система уровней и спавнеров
// ========================================

void AArkhamGameMode::CacheBotSpawners()
{
	BotSpawners.Empty();
	
	// Находим все спавнеры на уровне
	for (TActorIterator<ABotSpawner> It(GetWorld()); It; ++It)
	{
		ABotSpawner* Spawner = *It;
		if (Spawner)
		{
			BotSpawners.Add(Spawner);
			UE_LOG(LogTemp, Log, TEXT("  - Found spawner: Level %d at %s"), 
				Spawner->GetSpawnLevel(), *Spawner->GetActorLocation().ToString());
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 CacheBotSpawners: Found %d spawners"), BotSpawners.Num());
}

void AArkhamGameMode::SpawnBotsForCurrentLevel()
{
	CachedBots.Empty();
	
	if (BotSpawners.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("🎮 SpawnBotsForCurrentLevel: No spawners found!"));
		return;
	}
	
	int32 SpawnedCount = 0;
	
	for (ABotSpawner* Spawner : BotSpawners)
	{
		if (!Spawner)
			continue;
		
		// Спавним бота если уровень подходит
		AHumanBot* Bot = Spawner->SpawnBot(CurrentLevel);
		if (Bot)
		{
			CachedBots.Add(Bot);
			SpawnedCount++;
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 SpawnBotsForCurrentLevel: Spawned %d bots for level %d"), 
		SpawnedCount, CurrentLevel);
}

void AArkhamGameMode::NextLevel()
{
	CurrentLevel++;
	bLevelScreenShown = false; // Сбрасываем флаг для показа UI нового уровня
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 NextLevel: Moving to level %d"), CurrentLevel);
	
	// Перезапускаем уровень
	RestartLevel();
	
	// Показываем UI нового уровня
	if (!bLevelScreenShown)
	{
		bLevelScreenShown = true;
		OnCurrentLevel(CurrentLevel);
		UE_LOG(LogTemp, Warning, TEXT("🎮 NextLevel: Showing UI for level %d"), CurrentLevel);
	}
}

void AArkhamGameMode::FinishGame()
{
	UE_LOG(LogTemp, Warning, TEXT("🎮🏆 FinishGame: All levels completed!"));
	
	// Вызываем Blueprint событие
	OnFinishGame();
}

void AArkhamGameMode::OnBotDeath(AHumanBot* DeadBot)
{
	if (!DeadBot)
		return;
	
	UE_LOG(LogTemp, Warning, TEXT("🎮 OnBotDeath: Bot %s died"), *DeadBot->GetName());
	
	// Проверяем сколько ботов осталось
	int32 AliveCount = GetAliveBotCount();
	UE_LOG(LogTemp, Warning, TEXT("🎮 OnBotDeath: %d bots remaining"), AliveCount);
	
	if (AliveCount == 0)
	{
		// Проверяем есть ли еще спавнеры для следующего уровня
		bool bHasMoreLevels = false;
		for (ABotSpawner* Spawner : BotSpawners)
		{
			if (Spawner && Spawner->GetSpawnLevel() > CurrentLevel)
			{
				bHasMoreLevels = true;
				break;
			}
		}
		
		if (bHasMoreLevels)
		{
			UE_LOG(LogTemp, Warning, TEXT("🎮 OnBotDeath: Level %d complete! Moving to next level..."), CurrentLevel);
			
			// Задержка перед переходом на следующий уровень
			FTimerHandle NextLevelTimer;
			GetWorldTimerManager().SetTimer(NextLevelTimer, this, &AArkhamGameMode::NextLevel, 2.0f, false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("🎮 OnBotDeath: Last level complete! Finishing game..."));
			
			// Задержка перед завершением игры
			FTimerHandle FinishTimer;
			GetWorldTimerManager().SetTimer(FinishTimer, this, &AArkhamGameMode::FinishGame, 2.0f, false);
		}
	}
}

int32 AArkhamGameMode::GetAliveBotCount() const
{
	int32 AliveCount = 0;
	
	for (TActorIterator<AHumanBot> It(GetWorld()); It; ++It)
	{
		AHumanBot* Bot = *It;
		if (Bot && Bot->IsValidLowLevel())
		{
			if (AHuman* Human = Cast<AHuman>(Bot))
			{
				if (!Human->IsDead())
				{
					AliveCount++;
				}
			}
		}
	}
	
	return AliveCount;
}

