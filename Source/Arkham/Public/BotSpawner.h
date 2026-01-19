#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotSpawner.generated.h"

class AHumanBot;

/**
 * Спавнер для ботов с индексом уровня
 * Спавнит бота только если текущий уровень >= его индексу
 */
UCLASS()
class ARKHAM_API ABotSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	ABotSpawner();

	/** Индекс уровня на котором спавнится этот бот (1, 2, 3...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	int32 SpawnAtLevel = 1;

	/** Класс бота для спавна */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	TSubclassOf<AHumanBot> BotClass;

	/** Заспавнить бота если уровень подходит */
	UFUNCTION(BlueprintCallable, Category="Spawner")
	AHumanBot* SpawnBot(int32 CurrentLevel);

	/** Получить индекс уровня спавна */
	UFUNCTION(BlueprintCallable, Category="Spawner")
	int32 GetSpawnLevel() const { return SpawnAtLevel; }

	/** Получить заспавненного бота */
	UFUNCTION(BlueprintCallable, Category="Spawner")
	AHumanBot* GetSpawnedBot() const { return SpawnedBot; }

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Ссылка на заспавненного бота */
	UPROPERTY()
	TObjectPtr<AHumanBot> SpawnedBot;
};

