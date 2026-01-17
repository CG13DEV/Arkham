#include "Human.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagsManager.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "HumanAttributeSet.h"

AHuman::AHuman()
{
	PrimaryActorTick.bCanEverTick = true; // Включаем Tick для управления поворотом

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	Attributes = CreateDefaultSubobject<UHumanAttributeSet>(TEXT("HumanAttributes"));

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
}

UAbilitySystemComponent* AHuman::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AHuman::BeginPlay()
{
	Super::BeginPlay();
	InitASC();
}

void AHuman::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
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
	if (!Controller) return;

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FVector Velocity = GetVelocity();
	const float Speed = Velocity.Size2D();
	const bool bIsMoving = Speed > 1.0f;
	const bool bRunning = IsRunning();


	// ============================================================
	// 1. СТОИТ НА МЕСТЕ - только поворачиваем голову (DirectionView)
	// ============================================================
	if (!bIsMoving)
	{
		// Вычисляем разницу между направлением взгляда и персонажа
		float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, ControlRotation.Yaw);
		
		// Плавная интерполяция DirectionView для анимации головы
		DirectionView = FMath::FInterpTo(DirectionView, DeltaYaw, DeltaTime, DirectionViewInterpSpeed);
		
		// Капсула НЕ поворачивается
		return;
	}

	// ============================================================
	// 2. БЕГ - поворачиваемся к направлению движения
	// ============================================================
	if (bRunning)
	{
		// Направление движения
		const FRotator VelocityRotation = Velocity.ToOrientationRotator();
		
		// Плавный поворот к направлению движения
		const float RotationSpeed = RunRotationRate * DeltaTime;
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, VelocityRotation, DeltaTime, RunRotationRate / 180.f);
		NewRotation.Pitch = 0.f;
		NewRotation.Roll = 0.f;
		
		SetActorRotation(NewRotation);
		
		// DirectionView = 0 при беге (голова смотрит прямо)
		DirectionView = FMath::FInterpTo(DirectionView, 0.f, DeltaTime, DirectionViewInterpSpeed);
	}
	// ============================================================
	// 3. ХОДЬБА - поворачиваемся к контроллеру (камере)
	// ============================================================
	else
	{
		// Целевой yaw от контроллера
		FRotator TargetRotation = FRotator(0.f, ControlRotation.Yaw, 0.f);
		
		// Плавный поворот к контроллеру
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, WalkRotationRate / 180.f);
		NewRotation.Pitch = 0.f;
		NewRotation.Roll = 0.f;
		
		SetActorRotation(NewRotation);
		
		// DirectionView стремится к 0 (персонаж догоняет камеру)
		float DeltaYaw = FMath::FindDeltaAngleDegrees(NewRotation.Yaw, ControlRotation.Yaw);
		DirectionView = FMath::FInterpTo(DirectionView, DeltaYaw, DeltaTime, DirectionViewInterpSpeed);
	}
}

bool AHuman::IsRunning() const
{
	return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag_State_Running);
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

