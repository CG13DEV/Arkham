#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeTraceComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class UMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeTraceComponent();

	UFUNCTION(BlueprintCallable, Category="Combat|Melee")
	void StartTrace(UAbilitySystemComponent* InSourceASC, UObject* InSourceObject = nullptr);

	UFUNCTION(BlueprintCallable, Category="Combat|Melee")
	void StopTrace();

	UFUNCTION(BlueprintCallable, Category="Combat|Melee")
	bool IsTracing() const { return bTracing; }

	UFUNCTION(BlueprintCallable, Category="Combat|Melee")
	void SetTraceMesh(USkeletalMeshComponent* InMesh);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void TickTrace();
	void CacheInitialSocketPositions();
	void ApplyDamageToTarget(AActor* TargetActor, const FHitResult& Hit);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	TObjectPtr<USkeletalMeshComponent> TraceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	TArray<FName> TraceSockets = { FName(TEXT("hand_r")), FName(TEXT("hand_l")) };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee", meta=(ClampMin="0.0"))
	float TraceRadius = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee", meta=(ClampMin="0.0"))
	float TraceInterval = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	float BaseDamage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	bool bAllowMultipleHitsPerActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Melee")
	bool bDebugDraw = false;

private:
	bool bTracing = false;
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	TWeakObjectPtr<UObject> SourceObject;
	TArray<FVector> LastSocketPositions;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	FTimerHandle TraceTimerHandle;
};
