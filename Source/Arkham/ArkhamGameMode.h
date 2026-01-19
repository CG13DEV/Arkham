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

	/** Вызывается когда бот умирает - проверяет остались ли боты */
	UFUNCTION(BlueprintCallable, Category="Game|Level")
	void OnBotDeath(AHumanBot* DeadBot);

	/** Получить количество живых ботов */
	UFUNCTION(BlueprintCallable, Category="Game|Level")
	int32 GetAliveBotCount() const;

	/** Вызывается при смерти игрока для рестарта уровня */
	UFUNCTION(BlueprintCallable, Category="Game")
	void OnPlayerDeath(APawn* DeadPlayer);

	/** Телепортирует всех ботов на начальные позиции и сбрасывает их состояние (вместо Destroy/Spawn) */
	UFUNCTION(BlueprintCallable, Category="Game")
	void ResetAllBots();

	/** Удаляет всех ботов на уровне (старый метод, не используется) */
	UFUNCTION(BlueprintCallable, Category="Game")
	void DestroyAllBots();

	/** Спавнит ботов заново на их начальные позиции (старый метод, не используется) */
	UFUNCTION(BlueprintCallable, Category="Game")
	void RespawnAllBots();

	/** Перемещает игрока на PlayerStart */
	UFUNCTION(BlueprintCallable, Category="Game")
	void RespawnPlayer(APawn* Player);

	/** Очищает цели у всех ботов на уровне */
	UFUNCTION(BlueprintCallable, Category="Game")
	void ClearAllBotTargets();

	/** Полный рестарт: удаляет ботов, респавнит их и игрока */
	UFUNCTION(BlueprintCallable, Category="Game")
	void RestartLevel();

protected:
	virtual void BeginPlay() override;

	/** Находит и кэширует все спавнеры на уровне */
	void CacheBotSpawners();

	/** Спавнит ботов через спавнеры для текущего уровня */
	void SpawnBotsForCurrentLevel();

	/** Сохраняем начальные позиции ботов при старте уровня (старый метод) */
	void CacheBotStartPositions();

private:
	/** Список всех спавнеров на уровне */
	UPROPERTY()
	TArray<TObjectPtr<ABotSpawner>> BotSpawners;

	/** Карта: бот класс -> начальная позиция и ротация */
	UPROPERTY()
	TMap<TSubclassOf<AHumanBot>, FTransform> BotStartTransforms;

	/** Список всех ботов на уровне */
	UPROPERTY()
	TArray<TObjectPtr<AHumanBot>> CachedBots;
};



