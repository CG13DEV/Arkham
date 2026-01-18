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

	// Инициализация режима Look
	bIsLookMode = false;
	LockedSpringArmPosition = FVector::ZeroVector;
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

	// В режиме Look не обновляем позицию камеры
	if (bIsLookMode)
	{
		return;
	}

	FVector DistanceToTarget =  GetActorLocation() - TargetSpringArmFloatingLocation;
	FVector Direction = DistanceToTarget.GetSafeNormal();
	if (DistanceToTarget.SizeSquared() > FMath::Square(TargetSpringArmLength))
	{
		
		TargetSpringArmFloatingLocation = GetActorLocation() + -Direction * TargetSpringArmLength;
	}

	TargetSpringArmFloatingLocation = ChackVisibilityForSpringArm(
		GetActorLocation(),
		TargetSpringArmFloatingLocation,
		SpringArmMain->ProbeSize,
		SpringArmMain->ProbeChannel
	);

	SpringArmMain->SetWorldLocation(TargetSpringArmFloatingLocation + FVector(0.f, 0.f, 160.f));
	FVector MainDirection = GetActorLocation() + FVector(0.f, 0.f, 80.f) - SpringArmMain->GetComponentLocation();

	// Проверяем, достаточно ли велико расстояние для стабильного вычисления ротации
	const float MinDistanceForRotation = 50.f; // Минимальное расстояние для избежания нестабильности
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
	{
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AArkhamCharacter::Input_Look);
		EIC->BindAction(IA_Look, ETriggerEvent::Completed, this, &AArkhamCharacter::Input_LookReleased);
		EIC->BindAction(IA_Look, ETriggerEvent::Canceled, this, &AArkhamCharacter::Input_LookReleased);
	}

	if (IA_Run)
	{
		EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AArkhamCharacter::Input_RunPressed);
		EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AArkhamCharacter::Input_RunReleased);
		EIC->BindAction(IA_Run, ETriggerEvent::Canceled, this, &AArkhamCharacter::Input_RunReleased);
	}

	if (IA_Attack)
		EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AArkhamCharacter::Input_Attack);
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
	const FVector2D Axis = Value.Get<FVector2D>();
	
	// Проверяем, есть ли инпут (начало Look режима)
	if (!bIsLookMode && (FMath::Abs(Axis.X) > 0.01f || FMath::Abs(Axis.Y) > 0.01f))
	{
		// Входим в режим Look
		bIsLookMode = true;
		
		// Получаем точку крепления на персонаже (корень + offset)
		FVector AttachPoint = GetActorLocation() + FVector(0.f, 0.f, 60.f);
		
		// Запоминаем текущую позицию SpringArmMain
		LockedSpringArmPosition = SpringArmMain->GetComponentLocation();
		
		// Вычисляем расстояние от камеры до точки крепления
		float Distance = FVector::Dist(LockedSpringArmPosition, AttachPoint);
		
		// Перемещаем SpringArmMain в точку крепления
		SpringArmMain->SetWorldLocation(AttachPoint);
		
		// Устанавливаем TargetArmLength на вычисленное расстояние
		SpringArmMain->TargetArmLength = Distance;
		
		UE_LOG(LogTemp, Warning, TEXT("*** LOOK MODE ACTIVATED: Distance = %.2f ***"), Distance);
	}
	
	// Применяем вращение камеры
	if (bIsLookMode)
	{
		AddControllerYawInput(Axis.X);
		AddControllerPitchInput(Axis.Y);
	}
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

void AArkhamCharacter::Input_Attack(const FInputActionValue& Value)
{
	RequestMeleeAttack();
}

FVector AArkhamCharacter::ChackVisibilityForSpringArm(FVector PawnLocation, FVector Target, float ProbSize,
	TEnumAsByte<ECollisionChannel> ProbeChannel)
{
	if (!GetWorld()) return Target;
	FHitResult HitResult;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(this);
	if (GetWorld()->SweepSingleByChannel(
		HitResult,
		PawnLocation,
		Target,
		FQuat::Identity,
		ProbeChannel,
		FCollisionShape::MakeSphere(ProbSize),
		CollisionQueryParams
	))
	{
		// Если есть столкновение, возвращаем точку столкновения с небольшим отступом
		FVector Direction = (Target - PawnLocation).GetSafeNormal();
		return HitResult.Location - Direction * ProbSize;
	}

	return Target;
}
