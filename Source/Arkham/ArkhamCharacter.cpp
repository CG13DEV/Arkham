#include "ArkhamCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "GA_HumanRun.h" // Для автоматической регистрации способности бега

AArkhamCharacter::AArkhamCharacter()
{
	// Основной Spring Arm - следует за контроллером (поворот камеры мышью)
	SpringArmMain = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmMain"));
	SpringArmMain->SetupAttachment(GetRootComponent());
	SpringArmMain->SetRelativeLocation(FVector(0.f, 0.f, 60.f)); // Высота камеры
	
	SpringArmMain->TargetArmLength = 0.0f; // Дистанция камеры
	TargetSpringArmLength = 350.f; // Целевая дистанция камеры (для плавного изменения)
	SpringArmMain->bUsePawnControlRotation = true; // ВАЖНО: следует за контроллером
	
	SpringArmMain->bEnableCameraLag = false; // Lag только на прокси
	SpringArmMain->bEnableCameraRotationLag = false;
	
	SpringArmMain->bDoCollisionTest = true; // Коллизия только на прокси
	SpringArmMain->ProbeSize = 12.f;
	SpringArmMain->ProbeChannel = ECC_Camera;
	// Прокси Spring Arm - для camera lag и collision
	SpringArmProxy = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmProxy"));
	SpringArmProxy->SetupAttachment(SpringArmMain, USpringArmComponent::SocketName);
	
	SpringArmProxy->TargetArmLength = 0.0f; // Нулевая длина - вся дистанция в Main
	SpringArmProxy->bUsePawnControlRotation = false; // Не управляется контроллером
	
	// Наследуем ротацию от Main
	SpringArmProxy->bInheritPitch = true;
	SpringArmProxy->bInheritYaw = true;
	SpringArmProxy->bInheritRoll = true;

	// Camera Lag для плавности
	SpringArmProxy->bEnableCameraLag = true;
	SpringArmProxy->CameraLagSpeed = 20.f;
	SpringArmProxy->CameraLagMaxDistance = 0.f;
	SpringArmProxy->bUseCameraLagSubstepping = true;
	SpringArmProxy->CameraLagMaxTimeStep = 1.f / 60.f;

	SpringArmProxy->bEnableCameraRotationLag = true;
	SpringArmProxy->CameraRotationLagSpeed = 20.f;

	// Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArmProxy, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false; // Не управляется напрямую

	// Автоматически добавляем способность бега в DefaultAbilities
	// Это позволяет не настраивать Blueprint вручную
	DefaultAbilities.Add(UGA_HumanRun::StaticClass());
}


void AArkhamCharacter::BeginPlay()
{
	Super::BeginPlay();
	SetupInputMapping();

	TargetSpringArmFloatingLocation = GetActorForwardVector() * -TargetSpringArmLength + GetActorLocation();

	SpringArmMain->SetWorldLocation(TargetSpringArmFloatingLocation + FVector(0.f, 0.f, 160.f));

	FVector MainDirection = GetActorLocation() + FVector(0.f, 0.f, 80.f) - SpringArmMain->GetComponentLocation();

	GetController()->SetControlRotation(MainDirection.Rotation());
}

void AArkhamCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector DistanceToTarget =  GetActorLocation() - TargetSpringArmFloatingLocation;
	FVector Direction = DistanceToTarget.GetSafeNormal();
	if (DistanceToTarget.Size() > TargetSpringArmLength)
	{
		
		TargetSpringArmFloatingLocation = GetActorLocation() + -Direction * TargetSpringArmLength;
	}

	SpringArmMain->SetWorldLocation(TargetSpringArmFloatingLocation + FVector(0.f, 0.f, 160.f));
	FVector MainDirection = GetActorLocation() + FVector(0.f, 0.f, 80.f) - SpringArmMain->GetComponentLocation();

	// Проверяем, достаточно ли велико расстояние для стабильного вычисления ротации
	const float MinDistanceForRotation = 100.f; // Минимальное расстояние для избежания нестабильности
	if (MainDirection.SizeSquared() > FMath::Square(MinDistanceForRotation))
	{
		// Вместо прямой установки делаем плавную интерполяцию
		FRotator TargetRotation = MainDirection.Rotation();
		FRotator CurrentRotation = GetController()->GetControlRotation();
		
		// Плавная интерполяция с высокой скоростью для отзывчивости
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 10.f);
		GetController()->SetControlRotation(NewRotation);
	}
	// Если расстояние слишком мало, не меняем ротацию (избегаем джиттера)
}

void AArkhamCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	SetupInputMapping(); // на случай ре-спавна/смены контроллера
}

void AArkhamCharacter::SetupInputMapping()
{
	if (!IsLocallyControlled())
		return;

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	UEnhancedInputLocalPlayerSubsystem* Subsys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsys) return;

	if (DefaultMappingContext)
	{
		Subsys->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AArkhamCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (IA_Move)
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AArkhamCharacter::Input_Move);

	if (IA_Look)
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AArkhamCharacter::Input_Look);

	if (IA_Run)
	{
		EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AArkhamCharacter::Input_RunPressed);
		EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AArkhamCharacter::Input_RunReleased);
		EIC->BindAction(IA_Run, ETriggerEvent::Canceled, this, &AArkhamCharacter::Input_RunReleased);
	}
}

void AArkhamCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) return;

	// Движение относительно yaw камеры
	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AArkhamCharacter::Input_Look(const FInputActionValue& Value)
{
	// const FVector2D Axis = Value.Get<FVector2D>();
	// AddControllerYawInput(Axis.X);
	// AddControllerPitchInput(Axis.Y);
}

void AArkhamCharacter::Input_RunPressed()
{
	UE_LOG(LogTemp, Warning, TEXT("*** INPUT: Run Button PRESSED ***"));
	StartRun(); // базовый AHuman
}

void AArkhamCharacter::Input_RunReleased()
{
	UE_LOG(LogTemp, Warning, TEXT("*** INPUT: Run Button RELEASED ***"));
	StopRun(); // базовый AHuman
}
