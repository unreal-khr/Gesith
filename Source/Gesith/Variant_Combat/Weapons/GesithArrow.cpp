// Copyright Epic Games, Inc. All Rights Reserved.


#include "GesithArrow.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "CombatCharacter.h"
#include "CombatDamageable.h"

AGesithArrow::AGesithArrow()
{
	bReplicates = true;
	SetReplicateMovement(true);

	// arrows that never hit anything clean themselves up
	InitialLifeSpan = 10.0f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(5.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->CanCharacterStepUpOn = ECB_No;
	RootComponent = Collision;

	// a thin cylinder lying along the flight direction
	Shaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shaft"));
	Shaft->SetupAttachment(Collision);
	Shaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shaft->SetRelativeLocation(FVector(-40.0f, 0.0f, 0.0f));
	Shaft->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	Shaft->SetRelativeScale3D(FVector(0.02f, 0.02f, 0.8f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	if (CylinderMesh.Succeeded())
	{
		Shaft->SetStaticMesh(CylinderMesh.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.0f;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->MaxSpeed = 20000.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.5f;
}

void AGesithArrow::BeginPlay()
{
	Super::BeginPlay();

	// never hit the archer on the way out of the bow
	if (AActor* Shooter = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}
}

void AGesithArrow::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	// clients fly the arrow themselves between movement updates
	Movement->Velocity = NewVelocity;
}

void AGesithArrow::Launch(float NewDamage, const FVector& Velocity)
{
	Damage = NewDamage;

	Movement->Velocity = Velocity;

	if (HasAuthority())
	{
		Collision->OnComponentHit.AddDynamic(this, &AGesithArrow::OnArrowHit);
		Collision->OnComponentBeginOverlap.AddDynamic(this, &AGesithArrow::OnArrowOverlap);
	}
}

void AGesithArrow::OnArrowHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	StrikeTarget(OtherActor, OtherComp, Hit.ImpactPoint);
}

void AGesithArrow::OnArrowOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const ACombatCharacter* Shooter = Cast<ACombatCharacter>(GetInstigator());
	const ACombatCharacter* Character = Cast<ACombatCharacter>(OtherActor);

	// fly on past the archer, allies and fallen soldiers
	if (OtherActor == Shooter || (Character && (Character->IsDead() || (Shooter && Shooter->IsAllyOf(Character)))))
	{
		return;
	}

	StrikeTarget(OtherActor, OtherComp, bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation());
}

void AGesithArrow::StrikeTarget(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& ImpactPoint)
{
	if (bHasHit)
	{
		return;
	}

	bHasHit = true;

	if (ICombatDamageable* Damageable = Cast<ICombatDamageable>(OtherActor))
	{
		const FVector Impulse = GetVelocity().GetSafeNormal() * KnockbackImpulse;

		Damageable->ApplyDamage(Damage, this, ImpactPoint, Impulse);
	}

	// stick into whatever was hit and stay for a while
	Movement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (OtherComp)
	{
		AttachToComponent(OtherComp, FAttachmentTransformRules::KeepWorldTransform);
	}

	SetLifeSpan(StuckLifeSpan);
}
