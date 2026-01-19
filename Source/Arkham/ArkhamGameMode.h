// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HumanBot.h"
#include "ArkhamGameMode.generated.h"

class ABotSpawner;

UCLASS(minimalapi)
class AArkhamGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArkhamGameMode();

	/** Текущий уровень игры (1, 2, 3...) */
	UPROPERTY(BlueprintReadOnly, Category="Game|Level")
	int32 CurrentLevel = 1;

	/** Максимальный уровень в игре (определяется автоматически по спавнерам) */
	UPROPERTY(BlueprintReadOnly, Category="Game|Level")
	int32 MaxLevel = 3;

	/** Был ли уже показан экран текущего уровня */
	UPROPERTY(BlueprintReadOnly, Category="Game|Level")
	bool bLevelScreenShown = false;

	/** Blueprint Event: Вызывается при переходе на новый уровень для показа UI */
	UFUNCTION(BlueprintImplementableEvent, Category="Game|Level")
	void OnCurrentLevel(int32 LevelIndex);

	/** Blueprint Event: Вызывается при завершении всех уровней */
	UFUNCTION(BlueprintImplementableEvent, Category="Game|Level")
	void OnFinishGame();

	/** Переход на следующий уровень */
	UFUNCTION(BlueprintCallable, Category="Game|Level")
	void NextLevel();

	/** Завершение игры (все уровни пройдены) */
	UFUNCTION(BlueprintCallable, Category="Game|Level")
	void FinishGame();

	/** Сбрасывает цели у всех ботов (вызывается из триггера) */
	UFUNCTION(BlueprintCallable, Category="Game|Level")
	void ClearAllBotTargets();

	/** Вызывается при смерти игрока для рестарта уровня */
	UFUNCTION(BlueprintCallable, Category="Game")
	void OnPlayerDeath(APawn* DeadPlayer);

	/** Перемещает игрока на PlayerStart */
	UFUNCTION(BlueprintCallable, Category="Game")
	void RespawnPlayer(APawn* Player);

protected:
	virtual void BeginPlay() override;

	/** Находит и кэширует все спавнеры на уровне */
	void CacheBotSpawners();

	/** Спавнит ботов через спавнеры для текущего уровня */
	void SpawnBotsForCurrentLevel();

	/** Телепортирует всех ботов на начальные позиции */
	void ResetAllBotsToStart();

private:
	/** Список всех спавнеров на уровне */
	UPROPERTY()
	TArray<TObjectPtr<ABotSpawner>> BotSpawners;


	/** Список всех ботов на уровне */
	UPROPERTY()
	TArray<TObjectPtr<AHumanBot>> CachedBots;
};



