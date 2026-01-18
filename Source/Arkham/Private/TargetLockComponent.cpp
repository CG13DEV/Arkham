#include "TargetLockComponent.h"
#include "Human.h"
#include "GameFramework/Controller.h"

UTargetLockComponent::UTargetLockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	bIsLocked = false;
	CurrentTarget = nullptr;

	// Значения по умолчанию
	MaxLockDistance = 1000.f;
	MaxLockBreakDistance = 1500.f;
	MinLockDistance = 300.f; // Ближе 3 метров - таргет всегда работает
	LockAngle = 60.f;
	RotationSpeed = 720.f;
	TargetHeightOffset = 80.f;

	// Авто-активация
	bEnableAutoLock = true;
	AutoLockDistance = 600.f; // Ближе чем поиск, чтобы не спамить
	bManuallyDisabled = false;
	LastAutoCheckTime = 0.f;
	AutoCheckInterval = 0.2f; // Проверяем каждые 0.2 секунды
}

void UTargetLockComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerHuman = Cast<AHuman>(GetOwner());
	if (!OwnerHuman)
	{
		UE_LOG(LogTemp, Error, TEXT("TargetLockComponent: Owner is not AHuman!"));
	}
}

void UTargetLockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerHuman)
		return;

	// Проверяем авто-активацию (если включена и не заблокирован вручную)
	if (bEnableAutoLock && !bManuallyDisabled && !bIsLocked)
	{
		CheckAutoLock();
	}

	// Если уже заблокирован, проверяем валидность цели
	if (bIsLocked)
	{
		// Проверяем валидность текущей цели
		if (!IsTargetValid(CurrentTarget))
		{
			DisableTargetLock();
			return;
		}

		// ВАЖНО: НЕ поворачиваем если персонаж мертв или играется Death Montage (Root Motion)
		if (OwnerHuman->IsDead())
		{
			DisableTargetLock(); // Сбрасываем таргет при смерти
			return;
		}

		// Обновляем поворот к цели ТОЛЬКО если НЕ бежим
		// При беге персонаж поворачивается по направлению движения (в Human.cpp)
		if (!OwnerHuman->IsRunning())
		{
			UpdateRotationToTarget(DeltaTime);
		}
	}
}

void UTargetLockComponent::ToggleTargetLock()
{
	if (bIsLocked)
	{
		// Игрок вручную выключает - ставим флаг
		bManuallyDisabled = true;
		DisableTargetLock();
	}
	else
	{
		// Игрок вручную включает - сбрасываем флаг блокировки
		bManuallyDisabled = false;
		EnableTargetLock();
	}
}

void UTargetLockComponent::EnableTargetLock()
{
	if (!OwnerHuman || bIsLocked)
		return;

	// Ищем ближайшую цель
	AActor* NewTarget = FindClosestTarget();
	if (!NewTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("TargetLock: No valid targets found"));
		return;
	}

	CurrentTarget = NewTarget;
	bIsLocked = true;
	bManuallyDisabled = false; // Сбрасываем флаг ручного выключения

	UE_LOG(LogTemp, Warning, TEXT("TargetLock: LOCKED on %s"), *CurrentTarget->GetName());
}

void UTargetLockComponent::DisableTargetLock()
{
	if (!bIsLocked)
		return;

	bIsLocked = false;
	CurrentTarget = nullptr;

	UE_LOG(LogTemp, Warning, TEXT("TargetLock: UNLOCKED"));
}

void UTargetLockComponent::SwitchTargetRight()
{
	if (!bIsLocked || !OwnerHuman)
		return;

	AActor* NewTarget = FindTargetInDirection(true);
	if (NewTarget && NewTarget != CurrentTarget)
	{
		CurrentTarget = NewTarget;
		UE_LOG(LogTemp, Warning, TEXT("TargetLock: Switched RIGHT to %s"), *CurrentTarget->GetName());
	}
}

void UTargetLockComponent::SwitchTargetLeft()
{
	if (!bIsLocked || !OwnerHuman)
		return;

	AActor* NewTarget = FindTargetInDirection(false);
	if (NewTarget && NewTarget != CurrentTarget)
	{
		CurrentTarget = NewTarget;
		UE_LOG(LogTemp, Warning, TEXT("TargetLock: Switched LEFT to %s"), *CurrentTarget->GetName());
	}
}

void UTargetLockComponent::SetTarget(AActor* NewTarget, bool bAutoEnable)
{
	if (!OwnerHuman)
		return;

	CurrentTarget = NewTarget;

	if (bAutoEnable && NewTarget)
	{
		bIsLocked = true;
	}
}

void UTargetLockComponent::ClearTarget()
{
	CurrentTarget = nullptr;
	bIsLocked = false;
}

TArray<AActor*> UTargetLockComponent::FindAvailableTargets() const
{
	TArray<AActor*> ValidTargets;

	if (!OwnerHuman || !OwnerHuman->GetWorld())
		return ValidTargets;

	// Sphere trace для поиска всех актеров в радиусе
	TArray<FHitResult> HitResults;
	FVector StartLocation = OwnerHuman->GetActorLocation();
	FVector EndLocation = StartLocation;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerHuman);

	bool bHit = OwnerHuman->GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(MaxLockDistance),
		QueryParams
	);

	if (!bHit)
		return ValidTargets;

	// Фильтруем валидные цели
	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
			continue;

		// Проверяем что это AHuman
		AHuman* TargetHuman = Cast<AHuman>(HitActor);
		if (!TargetHuman || TargetHuman->IsDead())
			continue;

		// Проверяем угол
		FVector DirectionToTarget = (HitActor->GetActorLocation() - StartLocation).GetSafeNormal();
		FVector ForwardVector = OwnerHuman->GetActorForwardVector();
		
		float DotProduct = FVector::DotProduct(ForwardVector, DirectionToTarget);
		float Angle = FMath::Acos(DotProduct) * (180.f / PI);

		if (Angle <= LockAngle)
		{
			ValidTargets.Add(HitActor);
		}
	}

	return ValidTargets;
}

AActor* UTargetLockComponent::FindClosestTarget() const
{
	TArray<AActor*> Targets = FindAvailableTargets();
	if (Targets.Num() == 0)
		return nullptr;

	AActor* ClosestTarget = nullptr;
	float MinDistance = MaxLockDistance;

	FVector OwnerLocation = OwnerHuman->GetActorLocation();

	for (AActor* Target : Targets)
	{
		float Distance = FVector::Dist(OwnerLocation, Target->GetActorLocation());
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			ClosestTarget = Target;
		}
	}

	return ClosestTarget;
}

AActor* UTargetLockComponent::FindTargetInDirection(bool bRight) const
{
	TArray<AActor*> Targets = FindAvailableTargets();
	if (Targets.Num() == 0)
		return nullptr;

	if (!OwnerHuman->GetController())
		return nullptr;

	// Используем направление камеры (контроллера) для определения право/лево
	FVector CameraRight = FRotationMatrix(OwnerHuman->GetController()->GetControlRotation()).GetScaledAxis(EAxis::Y);
	FVector OwnerLocation = OwnerHuman->GetActorLocation();

	AActor* BestTarget = nullptr;
	float BestScore = -1.f;

	for (AActor* Target : Targets)
	{
		if (Target == CurrentTarget)
			continue;

		FVector DirectionToTarget = (Target->GetActorLocation() - OwnerLocation).GetSafeNormal();

		// Вычисляем насколько цель справа/слева
		float RightDot = FVector::DotProduct(CameraRight, DirectionToTarget);
		
		// Если ищем справа, берем только положительные значения
		if (bRight && RightDot < 0.f)
			continue;
		if (!bRight && RightDot > 0.f)
			continue;

		// Также учитываем расстояние до цели
		float Distance = FVector::Dist(OwnerLocation, Target->GetActorLocation());
		float Score = FMath::Abs(RightDot) / (Distance / MaxLockDistance);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Target;
		}
	}

	// Если не нашли в нужном направлении, оставляем текущую
	return BestTarget ? BestTarget : CurrentTarget.Get();
}

bool UTargetLockComponent::IsTargetValid(AActor* Target) const
{
	if (!Target || !OwnerHuman)
		return false;

	// Проверяем что цель жива
	AHuman* TargetHuman = Cast<AHuman>(Target);
	if (!TargetHuman || TargetHuman->IsDead())
	{
		UE_LOG(LogTemp, Log, TEXT("TargetLock: Target %s is DEAD or not Human - invalid"), *Target->GetName());
		return false;
	}

	// Проверяем дистанцию
	float Distance = FVector::Dist(OwnerHuman->GetActorLocation(), Target->GetActorLocation());
	
	// ВАЖНО: Если цель очень близко (ближе MinLockDistance) - таргет ВСЕГДА валиден
	// Это решает проблему потери таргета в ближнем бою
	if (Distance <= MinLockDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("TargetLock: Target %s is CLOSE (%.1f <= %.1f) - always valid"), 
			*Target->GetName(), Distance, MinLockDistance);
		return true;
	}
	
	// На средней/дальней дистанции проверяем MaxLockBreakDistance
	if (Distance > MaxLockBreakDistance)
	{
		UE_LOG(LogTemp, Log, TEXT("TargetLock: Target %s is TOO FAR (%.1f > %.1f) - invalid"), 
			*Target->GetName(), Distance, MaxLockBreakDistance);
		return false;
	}

	return true;
}

void UTargetLockComponent::UpdateRotationToTarget(float DeltaTime)
{
	if (!CurrentTarget || !OwnerHuman)
		return;

	// ВАЖНО: При беге НЕ управляем поворотом! Human.cpp сам поворачивает по направлению движения
	if (OwnerHuman->IsRunning())
		return;

	// Получаем направление к цели (с учетом высоты offset)
	FVector TargetLocation = CurrentTarget->GetActorLocation() + FVector(0.f, 0.f, TargetHeightOffset);
	FVector OwnerLocation = OwnerHuman->GetActorLocation() + FVector(0.f, 0.f, TargetHeightOffset);
	FVector DirectionToTarget = (TargetLocation - OwnerLocation).GetSafeNormal2D();

	if (DirectionToTarget.IsNearlyZero())
		return;

	// Целевая ротация
	FRotator TargetRotation = DirectionToTarget.Rotation();
	FRotator CurrentRotation = OwnerHuman->GetActorRotation();

	// Плавная интерполяция
	FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed / 180.f);
	NewRotation.Pitch = 0.f;
	NewRotation.Roll = 0.f;

	OwnerHuman->SetActorRotation(NewRotation);
}

void UTargetLockComponent::CheckAutoLock()
{
	if (!OwnerHuman || !OwnerHuman->GetWorld())
		return;

	// Проверяем интервал
	float CurrentTime = OwnerHuman->GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastAutoCheckTime < AutoCheckInterval)
		return;

	LastAutoCheckTime = CurrentTime;

	// Ищем ближайшую цель в радиусе AutoLockDistance
	TArray<FHitResult> HitResults;
	FVector StartLocation = OwnerHuman->GetActorLocation();
	FVector EndLocation = StartLocation;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerHuman);

	bool bHit = OwnerHuman->GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(AutoLockDistance),
		QueryParams
	);

	if (!bHit)
		return;

	// Ищем ближайшего валидного врага
	AActor* ClosestEnemy = nullptr;
	float MinDistance = AutoLockDistance;

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
			continue;

		// Проверяем что это AHuman и он жив
		AHuman* TargetHuman = Cast<AHuman>(HitActor);
		if (!TargetHuman || TargetHuman->IsDead())
			continue;

		// Вычисляем расстояние
		float Distance = FVector::Dist(StartLocation, HitActor->GetActorLocation());
		
		// Проверяем что цель в конусе обзора
		FVector DirectionToTarget = (HitActor->GetActorLocation() - StartLocation).GetSafeNormal();
		FVector ForwardVector = OwnerHuman->GetActorForwardVector();
		
		float DotProduct = FVector::DotProduct(ForwardVector, DirectionToTarget);
		float Angle = FMath::Acos(DotProduct) * (180.f / PI);

		// Более широкий угол для авто-активации
		if (Angle <= LockAngle * 1.5f && Distance < MinDistance)
		{
			MinDistance = Distance;
			ClosestEnemy = HitActor;
		}
	}

	// Если нашли цель - автоматически активируем Target Lock
	if (ClosestEnemy)
	{
		CurrentTarget = ClosestEnemy;
		bIsLocked = true;
	}
}

