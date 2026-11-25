#include "VigilActors.h"
#include "VigilGameMode.h"
#include "VigilAnim.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

// ================================================================ figure

AVigilFigure::AVigilFigure()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0, 420, 0);
	M->MaxWalkSpeed = 150.f;
	M->BrakingDecelerationWalking = 900.f;
	M->MaxAcceleration = 900.f;
	M->AirControl = 0.35f;
	M->bUseControllerDesiredRotation = false;
	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -90), FRotator(0, -90, 0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void AVigilFigure::Setup(const FString& GarmentName, bool bShowBody, const FString& SkinMat)
{
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
	Body->SetAnimInstanceClass(UVigilAnimInstance::StaticClass());
	if (GM)
	{
		if (UMaterialInterface* Skin = GM->Mat(SkinMat))
			for (int32 i = 0; i < Body->GetNumMaterials(); ++i) Body->SetMaterial(i, Skin);
	}
	Body->SetVisibility(bShowBody);
	Body->SetCastShadow(bShowBody);
	if (!GarmentName.IsEmpty())
	{
		Garment = NewObject<USkeletalMeshComponent>(this, TEXT("Garment"));
		Garment->SetupAttachment(Body);
		Garment->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("/Game/Vigil/Characters/%s.%s"), *GarmentName, *GarmentName)));
		Garment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Garment->RegisterComponent();
		Garment->SetLeaderPoseComponent(Body);
		AddInstanceComponent(Garment);
		// every clothing slot is named after its material (Cassock -> MI_Cassock); assign them here,
		// because the editor import does not keep skeletal mesh material assignments
		if (GM)
		{
			const TArray<FName> Slots = Garment->GetMaterialSlotNames();
			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				FString N = Slots[i].ToString();
				int32 Dot;
				if (N.FindChar(TEXT('.'), Dot)) N.LeftInline(Dot);
				if (UMaterialInterface* M = GM->Mat(TEXT("MI_") + N)) Garment->SetMaterial(i, M);
			}
		}
	}
}

UVigilAnimInstance* AVigilFigure::Anim() const
{
	return Cast<UVigilAnimInstance>(GetMesh()->GetAnimInstance());
}

UStaticMeshComponent* AVigilFigure::Hold(const FString& MeshName, FName Bone, FVector Offset, FRotator Rot, float Scale)
{
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	if (!GM) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(GM->Mesh(MeshName));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetupAttachment(GetMesh(), Bone);
	C->SetRelativeLocationAndRotation(Offset, Rot);
	C->SetRelativeScale3D(FVector(Scale));
	C->RegisterComponent();
	AddInstanceComponent(C);
	HeldProps.Add(C);
	return C;
}

void AVigilFigure::ClearHeld()
{
	for (UStaticMeshComponent* C : HeldProps) if (C) C->DestroyComponent();
	HeldProps.Empty();
}

void AVigilFigure::WalkTo(FVector Target, float Speed)
{
	MoveTarget = Target;
	MoveSpeed = Speed;
	bMoving = true;
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

void AVigilFigure::Stop()
{
	bMoving = false;
	GetCharacterMovement()->StopMovementImmediately();
}

void AVigilFigure::FaceYaw(float Yaw, bool bInstant)
{
	TargetYaw = Yaw;
	bHasYaw = true;
	if (bInstant) SetActorRotation(FRotator(0, Yaw, 0));
}

void AVigilFigure::FacePoint(FVector P, bool bInstant)
{
	const FVector D = P - GetActorLocation();
	if (D.Size2D() > 1.f) FaceYaw(D.Rotation().Yaw, bInstant);
}

void AVigilFigure::SetFade(float Alpha)
{
	Fade = Alpha;
	const bool bVis = Alpha > 0.02f;
	if (Garment)
	{
		Garment->SetVisibility(bVis);
		Garment->SetScalarParameterValueOnMaterials(TEXT("Strength"), 1.6f * Alpha);
	}
	if (GetMesh()->CastShadow || bPlayer) GetMesh()->SetVisibility(bVis && GetMesh()->CastShadow);
}

void AVigilFigure::Tick(float Dt)
{
	Super::Tick(Dt);
	AnimTime += Dt;
	if (bMoving)
	{
		FVector D = MoveTarget - GetActorLocation();
		D.Z = 0.f;
		if (D.Size() < 25.f)
		{
			bMoving = false;
			StuckTime = 0.f;
		}
		else
		{
			AddMovementInput(D.GetSafeNormal(), 1.f);
			// scripted walkers never hold a scene hostage: if a wall stops them, they arrive anyway
			StuckTime = GetVelocity().Size2D() < 15.f ? StuckTime + Dt : 0.f;
			if (!bPlayer && StuckTime > 1.5f)
			{
				SetActorLocation(FVector(MoveTarget.X, MoveTarget.Y, GetActorLocation().Z + 30.f), false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogTemp, Display, TEXT("VIGIL %s was blocked on a scripted walk and was moved to its mark"), *GetName());
				bMoving = false;
				StuckTime = 0.f;
			}
		}
	}
	else if (bHasYaw && !bPlayer)
	{
		const FRotator R = GetActorRotation();
		const float NewYaw = FMath::FixedTurn(R.Yaw, TargetYaw, 300.f * Dt);
		SetActorRotation(FRotator(0, NewYaw, 0));
	}
	// ease pose layers
	auto Ease = [Dt](float& Now, float Target) { Now = FMath::FInterpTo(Now, Target, Dt, 7.f); };
	Ease(PoseNow.Cross, Pose.Cross); Ease(PoseNow.Pray, Pose.Pray); Ease(PoseNow.Sprinkle, Pose.Sprinkle);
	Ease(PoseNow.Reach, Pose.Reach); Ease(PoseNow.Cense, Pose.Cense); Ease(PoseNow.Bow, Pose.Bow);
	Ease(PoseNow.Point, Pose.Point); Ease(PoseNow.Lunge, Pose.Lunge);
	PoseNow.Time = AnimTime;
	if (UVigilAnimInstance* A = Anim())
	{
		A->Speed = GetVelocity().Size2D();
		A->Layers = PoseNow;
	}
}

// ================================================================ prop

AVigilProp::AVigilProp()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);
}

UStaticMeshComponent* AVigilProp::AddMesh(UStaticMesh* Mesh, FVector Loc, FRotator Rot, FVector Scale, bool bCollide, UMaterialInterface* Mat)
{
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	// scenery is Static: movable meshes lose complex collision, and the walls depend on it
	C->SetMobility(Root->Mobility);
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(Root);
	C->SetRelativeLocationAndRotation(Loc, Rot);
	C->SetRelativeScale3D(Scale);
	if (bCollide)
	{
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		C->SetCollisionProfileName(TEXT("NoCollision"));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	C->SetGenerateOverlapEvents(false);
	if (Mat) C->SetMaterial(0, Mat);
	C->RegisterComponent();
	AddInstanceComponent(C);
	Meshes.Add(C);
	return C;
}

// ================================================================ zone

AVigilZone::AVigilZone()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVigilZone::SetBox(FVector Center, FVector HalfExtent, float Yaw)
{
	Box = FTransform(FRotator(0, Yaw, 0), Center);
	Half = HalfExtent;
	SetActorLocation(Center);
}

bool AVigilZone::Contains(FVector P) const
{
	const FVector L = Box.InverseTransformPosition(P);
	return FMath::Abs(L.X) <= Half.X && FMath::Abs(L.Y) <= Half.Y && FMath::Abs(L.Z) <= Half.Z;
}

void AVigilZone::Tick(float Dt)
{
	Super::Tick(Dt);
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	if (!GM || !GM->Priest) return;
	const bool bNow = Contains(GM->Priest->GetActorLocation());
	if (bNow != bInside)
	{
		bInside = bNow;
		for (const TWeakObjectPtr<UPrimitiveComponent>& C : HideWhenInside)
			if (C.IsValid()) C->SetVisibility(!bInside);
		GM->OnZone(Id, bInside);
	}
}
