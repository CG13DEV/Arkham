#include "Human.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "Net/UnrealNetwork.h"

#include "HumanAttributeSet.h"

#include "MeleeTraceComponent.h"
#include "TargetLockComponent.h"
#include "MotionWarpingComponent.h"

// Forward declaration для проверки типа
class AHumanBot;

AHuman::AHuman()
{
	PrimaryActorTick.bCanEverTick = true; // Включаем Tick для управления поворотом

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	Attributes = CreateDefaultSubobject<UHumanAttributeSet>(TEXT("HumanAttributes"));

	UnarmedMeleeTrace = CreateDefaultSubobject<UMeleeTraceComponent>(TEXT("MeleeTrace_Unarmed"));

	// Target Lock компонент
	TargetLockComponent = CreateDefaultSubobject<UTargetLockComponent>(TEXT("TargetLock"));

	// Motion Warping компонент для подстройки позиции при атаках
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	// Персонаж НЕ поворачивается автоматически от контроллера
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Персонаж НЕ поворачивается автоматически по направлению движения
	// Мы управляем поворотом вручную в Tick через UpdateRotation()
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 0.f); // Отключаем авто-поворот

	// Инерционность движения
	GetCharacterMovement()->MaxAcceleration = 1024.0f;        // Ускорение (чем меньше, тем медленнее разгон)
	GetCharacterMovement()->BrakingDecelerationWalking = 1024.0f; // Торможение при ходьбе
	GetCharacterMovement()->GroundFriction = 6.0f;            // Трение (влияет на скольжение)
	GetCharacterMovement()->BrakingFrictionFactor = 1.0f;     // Множитель трения при торможении
	GetCharacterMovement()->bUseSeparateBrakingFriction = true; // Использовать отдельное трение для торможения

	Tag_State_Running = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Running"));
	Tag_Ability_Run   = FGameplayTag::RequestGameplayTag(TEXT("Ability.Movement.Run"));
	Tag_Ability_MeleeAttack = FGameplayTag::RequestGameplayTag(TEXT("Ability.Combat.MeleeAttack"));
	Tag_State_Dead = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"));
	Tag_State_Attacking = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Attacking"));
}

UAbilitySystemComponent* AHuman::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AHuman::BeginPlay()
{
	Super::BeginPlay();
	InitASC();

	// Настраиваем зависимость Tick: Human::Tick должен выполняться ПОСЛЕ TargetLockComponent
	// Это гарантирует что при беге Human::Tick имеет последнее слово в повороте
	if (TargetLockComponent)
	{
		AddTickPrerequisiteComponent(TargetLockComponent);
	}
}

void AHuman::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Не обновляем поворот если мертвы или играется Death Montage (Root Motion)
	if (bIsDead || bIsPlayingDeathMontage)
	{
		// Лог для отладки
		if (bIsPlayingDeathMontage)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("Tick: Rotation blocked - Death Montage playing"));
		}
		return;
	}
	
	// Обновляем поворот персонажа и DirectionView
	UpdateRotation(DeltaTime);
}

void AHuman::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitASC();

	if (HasAuthority())
	{
		GiveDefaultAbilities();
		ApplyDefaultAttributes();
	}
}

void AHuman::OnRep_Controller()
{
	Super::OnRep_Controller();
	InitASC();
}

void AHuman::InitASC()
{
	if (bASCReady || !AbilitySystem)
		return;

	AbilitySystem->InitAbilityActorInfo(this, this);
	BindASCDelegates();

	// Если дефолтного GE нет — зададим базовую скорость на сервере напрямую
	if (HasAuthority() && !DefaultAttributesEffect)
	{
		AbilitySystem->SetNumericAttributeBase(UHumanAttributeSet::GetMoveSpeedAttribute(), WalkSpeed);
	}

	// Применим текущие значения на CharacterMovement
	const float CurrentSpeed = Attributes ? Attributes->GetMoveSpeed() : WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeed = (CurrentSpeed > 1.f) ? CurrentSpeed : WalkSpeed;

	ApplyLocomotionConfig(AbilitySystem->HasMatchingGameplayTag(Tag_State_Running));

	bASCReady = true;
}

void AHuman::GiveDefaultAbilities()
{
	if (bAbilitiesGiven || !AbilitySystem || !HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("GiveDefaultAbilities: SKIPPED (Already given=%s, ASC=%s, Authority=%s)"),
			bAbilitiesGiven ? TEXT("YES") : TEXT("NO"),
			AbilitySystem ? TEXT("Valid") : TEXT("NULL"),
			HasAuthority() ? TEXT("YES") : TEXT("NO"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("=== GiveDefaultAbilities: Starting to give %d abilities ==="), DefaultAbilities.Num());

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - Skipping NULL ability class"));
			continue;
		}
		
		UE_LOG(LogTemp, Warning, TEXT("  - Giving ability: %s"), *AbilityClass->GetName());
		FGameplayAbilitySpec Spec = FGameplayAbilitySpec(AbilityClass, 1);
		AbilitySystem->GiveAbility(Spec);
	}

	bAbilitiesGiven = true;
	UE_LOG(LogTemp, Warning, TEXT("=== GiveDefaultAbilities: COMPLETE ==="));
}

void AHuman::ApplyDefaultAttributes()
{
	if (!AbilitySystem || !HasAuthority() || !DefaultAttributesEffect)
		return;

	FGameplayEffectContextHandle Ctx = AbilitySystem->MakeEffectContext();
	Ctx.AddSourceObject(this);

	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(DefaultAttributesEffect, 1.f, Ctx);
	if (Spec.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AHuman::BindASCDelegates()
{
	if (!AbilitySystem || !Attributes)
		return;

	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHumanAttributeSet::GetMoveSpeedAttribute())
		.AddUObject(this, &AHuman::OnMoveSpeedChanged);

	AbilitySystem->RegisterGameplayTagEvent(Tag_State_Running, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AHuman::OnRunningTagChanged);

	// Подписка на изменения здоровья
	AbilitySystem->GetGameplayAttributeValueChangeDelegate(UHumanAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AHuman::OnHealthChanged);

	// Подписка на тег смерти
	AbilitySystem->RegisterGameplayTagEvent(Tag_State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &AHuman::OnDeathTagAdded);
}

void AHuman::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	GetCharacterMovement()->MaxWalkSpeed = Data.NewValue;
}

void AHuman::OnRunningTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	ApplyLocomotionConfig(NewCount > 0);
}

void AHuman::ApplyLocomotionConfig(bool bRunning)
{
	// При беге ничего не делаем - поворот управляется в UpdateRotation
	// Этот метод оставлен для будущей логики (например, изменение анимаций)
}

void AHuman::UpdateRotation(float DeltaTime)
{
	const FRotator CurrentRotation = GetActorRotation();
	const FVector Velocity = GetVelocity();
	const float Speed = Velocity.Size2D();
	const bool bIsMoving = Speed > 1.0f;
	const bool bRunning = IsRunning();

	if (bRunning || (bIsMoving && !IsTargetLocked()))
	{
		// Направление движения
		const FRotator VelocityRotation = Velocity.ToOrientationRotator();
		
		// Плавный поворот к направлению движения
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, VelocityRotation, DeltaTime, RunRotationRate / 180.f);
		NewRotation.Pitch = 0.f;
		NewRotation.Roll = 0.f;
		
		SetActorRotation(NewRotation);

		// DirectionView = 0 при беге (голова смотрит прямо)
		DirectionView = FMath::FInterpTo(DirectionView, 0.f, DeltaTime, DirectionViewInterpSpeed);
		return;
	}

	if (bIsMoving && IsTargetLocked())
	{
		DirectionView = 0.f;
		return;
	}

	// ВАЖНО: Для ботов - не поворачиваем если стоим без движения и без Target Lock
	// Это позволяет боту сохранять начальную ротацию на уровне пока не увидит игрока
	if (Cast<AAIController>(Controller))
	{
		if (!bIsMoving && !IsTargetLocked())
		{
			// Бот стоит без цели - сохраняем текущую ротацию
			return;
		}
	}

	if (!Controller)
		return;

	const FRotator ControlRotation = Controller->GetControlRotation();

	// Вычисляем разницу между направлением взгляда и персонажа
	float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, ControlRotation.Yaw);
	
	// Плавная интерполяция DirectionView для анимации головы
	DirectionView = FMath::FInterpTo(DirectionView, DeltaYaw, DeltaTime, DirectionViewInterpSpeed);
	
	// Капсула НЕ поворачивается
}

bool AHuman::IsRunning() const
{
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag_State_Running);
}

// === Target Lock ===

bool AHuman::IsTargetLocked() const
{
	return TargetLockComponent && TargetLockComponent->IsTargetLocked();
}

AActor* AHuman::GetLockedTarget() const
{
	return TargetLockComponent ? TargetLockComponent->GetCurrentTarget() : nullptr;
}

void AHuman::StartRun()
{
	UE_LOG(LogTemp, Warning, TEXT("=== AHuman::StartRun CALLED ==="));
	
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AHuman::StartRun - AbilitySystem is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("AHuman::StartRun - Searching for Ability.Movement.Run in %d abilities"), 
		AbilitySystem->GetActivatableAbilities().Num());

	// Активируем ability по тегу Ability.Movement.Run
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability)
		{
			UE_LOG(LogTemp, Log, TEXT("  - Checking Ability: %s, Tags: %s"), 
				*Spec.Ability->GetName(),
				*Spec.Ability->AbilityTags.ToStringSimple());
			
			if (Spec.Ability->AbilityTags.HasTag(Tag_Ability_Run))
			{
				UE_LOG(LogTemp, Warning, TEXT("AHuman::StartRun - Found GA_HumanRun! Trying to activate..."));
				bool bSuccess = AbilitySystem->TryActivateAbility(Spec.Handle);
				UE_LOG(LogTemp, Warning, TEXT("AHuman::StartRun - Activation result: %s"), bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
				return;
			}
		}
	}

	UE_LOG(LogTemp, Error, TEXT("AHuman::StartRun - Ability.Movement.Run NOT FOUND in abilities!"));
}

void AHuman::StopRun()
{
	UE_LOG(LogTemp, Warning, TEXT("=== AHuman::StopRun CALLED ==="));
	
	if (!AbilitySystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AHuman::StopRun - AbilitySystem is NULL!"));
		return;
	}

	FGameplayTagContainer CancelTags;
	CancelTags.AddTag(Tag_Ability_Run);
	
	UE_LOG(LogTemp, Warning, TEXT("AHuman::StopRun - Cancelling abilities with tag: %s"), *Tag_Ability_Run.ToString());
	AbilitySystem->CancelAbilities(&CancelTags);
	UE_LOG(LogTemp, Warning, TEXT("AHuman::StopRun - Cancel request sent"));
}

// === Функции здоровья ===

float AHuman::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float AHuman::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

void AHuman::PerformMeleeAttack()
{
	if (!AbilitySystem || bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHuman::PerformMeleeAttack - Cannot attack (ASC=%s, Dead=%s)"),
			AbilitySystem ? TEXT("Valid") : TEXT("NULL"), bIsDead ? TEXT("YES") : TEXT("NO"));
		return;
	}

	// Активируем способность атаки по тегу
	for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->AbilityTags.HasTag(Tag_Ability_MeleeAttack))
		{
			bool bSuccess = AbilitySystem->TryActivateAbility(Spec.Handle);
			UE_LOG(LogTemp, Warning, TEXT("AHuman::PerformMeleeAttack - Activation result: %s"), 
				bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
			return;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("AHuman::PerformMeleeAttack - Ability.Combat.MeleeAttack NOT FOUND!"));
}


void AHuman::RequestMeleeAttack()
{
	if (!AbilitySystem || bIsDead)
	{
		UE_LOG(LogTemp, Log, TEXT("RequestMeleeAttack: Blocked - Dead: %s"), bIsDead ? TEXT("YES") : TEXT("NO"));
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// Если уже атакуем — буферизуем (как в Thug), но только спустя небольшой порог времени
	if (AbilitySystem->HasMatchingGameplayTag(Tag_State_Attacking))
	{
		// ВАЖНО: Не буферизуем если мертвы
		if (bIsDead)
		{
			UE_LOG(LogTemp, Warning, TEXT("RequestMeleeAttack: Dead - not buffering"));
			return;
		}

		float TimeSinceStart = Now - LastMeleeAbilityStartTime;
		
		UE_LOG(LogTemp, Warning, TEXT("RequestMeleeAttack: Already attacking! Time since start: %.3f, MinBuffer: %.3f"), 
			TimeSinceStart, MinAttackTimeBeforeBuffer);
		
		if (TimeSinceStart >= MinAttackTimeBeforeBuffer)
		{
			bAttackInputBuffered = true;
			UE_LOG(LogTemp, Warning, TEXT("RequestMeleeAttack: INPUT BUFFERED!"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RequestMeleeAttack: Too early for buffer (anti-double-click)"));
		}
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("RequestMeleeAttack: Starting new attack (buffer was: %s)"), 
		bAttackInputBuffered ? TEXT("TRUE") : TEXT("FALSE"));
	
	bAttackInputBuffered = false;
	LastMeleeAbilityStartTime = Now;
	PerformMeleeAttack();
}

UMeleeTraceComponent* AHuman::ResolveMeleeTraceComponent() const
{
	if (ActiveWeaponMeleeTrace)
		return ActiveWeaponMeleeTrace;
	return UnarmedMeleeTrace;
}

void AHuman::StartMeleeTrace()
{
	if (!AbilitySystem || bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ StartMeleeTrace: Blocked (ASC: %s, Dead: %s)"), 
			AbilitySystem ? TEXT("Valid") : TEXT("NULL"), bIsDead ? TEXT("YES") : TEXT("NO"));
		return;
	}

	if (UMeleeTraceComponent* Tracer = ResolveMeleeTraceComponent())
	{
		// Устанавливаем урон перед стартом трейса
		if (CurrentAttackDamage > 0.f)
		{
			Tracer->SetDamageForNextTrace(CurrentAttackDamage);
			UE_LOG(LogTemp, Warning, TEXT("⚔️ StartMeleeTrace: Damage set to %.1f"), CurrentAttackDamage);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ StartMeleeTrace: No damage set! Using BaseDamage from component"));
		}
		
		Tracer->StartTrace(AbilitySystem, this);
		UE_LOG(LogTemp, Warning, TEXT("✅ StartMeleeTrace: Trace started!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ StartMeleeTrace: MeleeTraceComponent is NULL!"));
	}
}

void AHuman::StopMeleeTrace()
{
	UE_LOG(LogTemp, Warning, TEXT("🛑 StopMeleeTrace: Called"));
	
	// Безопасно стопаем оба, на случай переключения источника во время окна
	if (UnarmedMeleeTrace)
	{
		UnarmedMeleeTrace->StopTrace();
	}
	if (ActiveWeaponMeleeTrace && ActiveWeaponMeleeTrace != UnarmedMeleeTrace)
	{
		ActiveWeaponMeleeTrace->StopTrace();
	}
	
	// Сбрасываем урон после завершения атаки
	CurrentAttackDamage = 0.f;
}

void AHuman::SetNextAttackDamage(float Damage)
{
	CurrentAttackDamage = Damage;
	UE_LOG(LogTemp, Log, TEXT("AHuman::SetNextAttackDamage - Set to %.1f"), Damage);
}

void AHuman::SetMeleeTraceSourceActor(AActor* InSourceActor)
{
	MeleeTraceSourceActor = InSourceActor;
	ActiveWeaponMeleeTrace = nullptr;

	if (InSourceActor)
	{
		ActiveWeaponMeleeTrace = InSourceActor->FindComponentByClass<UMeleeTraceComponent>();
	}
}

void AHuman::WarpAttack(float Radius, float Distance)
{
	if (!MotionWarping)
	{
		UE_LOG(LogTemp, Warning, TEXT("WarpAttack: MotionWarping component is NULL!"));
		return;
	}

	// Получаем цель из Target Lock
	AActor* Target = GetLockedTarget();

	// Если нет цели или цель слишком далеко - удаляем warp target
	if (!Target || (GetActorLocation() - Target->GetActorLocation()).Size() > Radius)
	{
		MotionWarping->RemoveWarpTarget(FName("Attack"));
		return;
	}

	// Ищем существующий warp target
	MotionWarping->FindWarpTarget(FName("Attack"));

	// Вычисляем позицию цели (головы на уровне персонажа)
	FVector TargetHead = Target->GetActorLocation() + FVector(0.f, 0.f, 80.f); // Примерная высота головы
	TargetHead.Z = GetActorLocation().Z; // Выравниваем по высоте персонажа

	// Вычисляем позицию для варпа - на расстоянии Distance от цели
	FVector WarpLocation = (GetActorLocation() - TargetHead).GetSafeNormal() * Distance + TargetHead;

	// Направление к цели
	FVector DirectionToTarget = (TargetHead - GetActorLocation()).GetSafeNormal();

	// Создаем и обновляем warp target
	FMotionWarpingTarget WarpTarget;
	WarpTarget.Name = FName(TEXT("Attack"));
	WarpTarget.Location = WarpLocation;
	WarpTarget.Rotation = (Target->GetActorLocation() - GetActorLocation()).Rotation();
	
	MotionWarping->AddOrUpdateWarpTarget(WarpTarget);

	UE_LOG(LogTemp, Log, TEXT("WarpAttack: Set warp target at distance %.1f from %s"), 
		Distance, *Target->GetName());
}

void AHuman::OnMeleeAttackAbilityEnded()
{
	StopMeleeTrace();

	UE_LOG(LogTemp, Log, TEXT("OnMeleeAttackAbilityEnded: Called (BufferFlag: %s)"), 
		bAttackInputBuffered ? TEXT("TRUE") : TEXT("FALSE"));

	if (!AbilitySystem || bIsDead)
	{
		// Очищаем буфер если мертвы
		if (bIsDead && bAttackInputBuffered)
		{
			bAttackInputBuffered = false;
			UE_LOG(LogTemp, Warning, TEXT("OnMeleeAttackAbilityEnded: Dead - cleared attack buffer"));
		}
		return;
	}

	if (!bAttackInputBuffered)
		return;

	bAttackInputBuffered = false;

	UE_LOG(LogTemp, Warning, TEXT("OnMeleeAttackAbilityEnded: Scheduling next attack on next tick!"));

	// Запускаем следующую атаку на следующий тик, чтобы тег State.Combat.Attacking успел сняться
	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		UE_LOG(LogTemp, Warning, TEXT("NextTick: Executing buffered attack!"));
		RequestMeleeAttack();
	});
}

void AHuman::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	const float Delta = Data.NewValue - Data.OldValue;
	
	UE_LOG(LogTemp, Warning, TEXT("AHuman::OnHealthChanged - Health: %.1f -> %.1f (Delta: %.1f)"),
		Data.OldValue, Data.NewValue, Delta);

	// Если получили урон (здоровье уменьшилось)
	if (Delta < 0.f && !bIsDead)
	{
		// Проигрываем анимацию получения урона
		if (HitReactionMontage)
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance)
			{
				// ВАЖНО: Если HitReaction уже играет - прерываем его и запускаем заново
				// Это позволяет показать реакцию на каждый удар
				if (AnimInstance->Montage_IsPlaying(HitReactionMontage))
				{
					UE_LOG(LogTemp, Log, TEXT("🤕 OnHealthChanged: Interrupting current HitReaction"));
					AnimInstance->Montage_Stop(0.1f, HitReactionMontage); // Быстрый blend out за 0.1 сек
				}
				
				// Запускаем HitReaction монтаж
				AnimInstance->Montage_Play(HitReactionMontage);
				UE_LOG(LogTemp, Warning, TEXT("🤕 OnHealthChanged: Playing HitReaction montage '%s' (Damage: %.1f)"), 
					*HitReactionMontage->GetName(), -Delta);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("❌ OnHealthChanged: AnimInstance is NULL!"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ OnHealthChanged: No HitReactionMontage set in Blueprint!"));
		}

		// Проверяем смерть
		if (Data.NewValue <= 0.f)
		{
			HandleDeath();
		}
	}
}

void AHuman::OnDeathTagAdded(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && !bIsDead)
	{
		HandleDeath();
	}
}

void AHuman::HandleDeath()
{
	if (bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ HandleDeath: Already dead - ignoring"));
		return;
	}

	bIsDead = true;

	// ВАЖНО: Очищаем буфер атаки чтобы не было атак после смерти
	bAttackInputBuffered = false;
	UE_LOG(LogTemp, Warning, TEXT("💀 HandleDeath: Attack buffer CLEARED"));

	StopMeleeTrace();

	UE_LOG(LogTemp, Error, TEXT("💀 HandleDeath: %s is DEAD!"), *GetName());

	// Добавляем тег смерти
	if (AbilitySystem)
	{
		FGameplayTagContainer DeathTags;
		DeathTags.AddTag(Tag_State_Dead);
		AbilitySystem->AddLooseGameplayTags(DeathTags);

		// Отменяем все активные способности
		AbilitySystem->CancelAllAbilities();
		UE_LOG(LogTemp, Warning, TEXT("💀 HandleDeath: Cancelled all abilities"));
	}

	// Отключаем контроллер (опционально)
	if (Controller)
	{
		Controller->UnPossess();
		UE_LOG(LogTemp, Warning, TEXT("💀 HandleDeath: Unpossessed controller"));
	}

	// ВАЖНО: Отключаем коллизию капсулы чтобы боты перестали атаковать
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogTemp, Warning, TEXT("💀 HandleDeath: Disabled capsule collision"));
	}

	// Отключаем движение
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
		
		// ВАЖНО: Отключаем автоматический поворот для сохранения Root Motion
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		
		UE_LOG(LogTemp, Warning, TEXT("💀 HandleDeath: Disabled movement and auto-rotation"));
	}

	// Отключаем инпут
	DisableInput(Cast<APlayerController>(Controller));

	// Проигрываем анимацию смерти (если есть)
	if (DeathMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("🎬 HandleDeath: DeathMontage found: %s"), *DeathMontage->GetName());
		
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance)
		{
			UE_LOG(LogTemp, Warning, TEXT("🎬 HandleDeath: AnimInstance is valid"));
			
			// ВАЖНО: Блокируем поворот для Root Motion анимации
			bIsPlayingDeathMontage = true;
			
			// Запускаем монтаж СНАЧАЛА
			float PlayLength = AnimInstance->Montage_Play(DeathMontage);
			
			if (PlayLength > 0.f)
			{
				// Настраиваем callback на BlendOut (начало возврата в idle)
				// ВАЖНО: SetBlendingOutDelegate нужно вызывать ПОСЛЕ Montage_Play!
				FOnMontageBlendingOutStarted BlendOutDelegate;
				BlendOutDelegate.BindUObject(this, &AHuman::OnDeathMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, DeathMontage);
				
				UE_LOG(LogTemp, Warning, TEXT("🎬 HandleDeath: Montage started (%.2f sec), BlendOut delegate SET"), PlayLength);
				UE_LOG(LogTemp, Warning, TEXT("🎬 HandleDeath: Playing death montage '%s' - rotation BLOCKED"), 
					*DeathMontage->GetName());
				UE_LOG(LogTemp, Warning, TEXT("🎬 HandleDeath: Ragdoll will activate on BlendOut - WAITING..."));
				return;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("❌ HandleDeath: Montage_Play failed (returned 0)! Enabling ragdoll immediately"));
				bIsPlayingDeathMontage = false;
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("❌ HandleDeath: AnimInstance is NULL!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ HandleDeath: No DeathMontage set in Blueprint"));
	}

	// Если нет анимации смерти - сразу включаем рэгдолл
	UE_LOG(LogTemp, Warning, TEXT("⚠️ HandleDeath: No DeathMontage - enabling ragdoll immediately"));
	EnableRagdoll();
}

void AHuman::EnableRagdoll()
{
	UE_LOG(LogTemp, Warning, TEXT("AHuman::EnableRagdoll - Enabling ragdoll physics"));

	// Сбрасываем флаг проигрывания Death Montage
	bIsPlayingDeathMontage = false;

	// Отключаем капсулу коллизии
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Включаем физику для меша
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp)
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetSimulatePhysics(true);
		MeshComp->WakeAllRigidBodies();
		
		// Отключаем CharacterMovement
		GetCharacterMovement()->DisableMovement();
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void AHuman::OnDeathMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogTemp, Warning, TEXT("🎬 OnDeathMontageBlendingOut: BlendOut started (Interrupted: %s)"), 
		bInterrupted ? TEXT("YES") : TEXT("NO"));
	
	// Включаем ragdoll когда начинается BlendOut анимации смерти
	EnableRagdoll();
}

