#pragma once

#include "CoreMinimal.h"
#include "Human.h"
#include "ArkhamCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class AArkhamCharacter : public AHuman
{
	GENERATED_BODY()

public:
	AArkhamCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<USpringArmComponent> SpringArmMain;

	FVector TargetSpringArmFloatingLocation;
	float TargetSpringArmLength;

	// Режим Look камеры
	bool bIsLookMode;
	FVector LockedSpringArmPosition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<USpringArmComponent> SpringArmProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// Enhanced Input
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Run;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Attack;

	void SetupInputMapping();

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_LookReleased();
	void Input_RunPressed();
	void Input_RunReleased();

	void Input_Attack(const FInputActionValue& Value);

	FVector ChackVisibilityForSpringArm(FVector PawnLocation, FVector Target, float ProbSize, TEnumAsByte<ECollisionChannel> ProbeChannel);
};
