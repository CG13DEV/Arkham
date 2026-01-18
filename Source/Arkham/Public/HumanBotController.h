#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "HumanBotController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * AI Controller для HumanBot с поддержкой State Tree
 * State Tree Component добавляется в Blueprint контроллера
 */
UCLASS()
class ARKHAM_API AHumanBotController : public AAIController
{
	GENERATED_BODY()

public:
	AHumanBotController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;

	/** AI Perception компонент */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	/** Конфигурация зрения */
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Callback при обнаружении актора */
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};

