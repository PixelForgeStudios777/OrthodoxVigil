#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "VigilAnim.h"
#include "VigilActors.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UBoxComponent;
class UVigilAnimInstance;

// Any figure: Epic's mannequin body with clothes from Blender layered on top (leader pose).
UCLASS()
class ORTHODOXVIGIL_API AVigilFigure : public ACharacter
{
	GENERATED_BODY()
public:
	AVigilFigure();
	virtual void Tick(float Dt) override;

	// Body visible (false for the bright ones, who are robes and nothing else).
	void Setup(const FString& Garment, bool bShowBody, const FString& SkinMat = TEXT("MI_Skin"));
	UStaticMeshComponent* Hold(const FString& MeshName, FName Bone, FVector Offset, FRotator Rot, float Scale = 1.f);
	void ClearHeld();
	UVigilAnimInstance* Anim() const;

	// scripted movement
	void WalkTo(FVector Target, float Speed = 150.f);
	void Stop();
	bool Arrived() const { return !bMoving; }
	void FaceYaw(float Yaw, bool bInstant = false);
	void FacePoint(FVector P, bool bInstant = false);
	void SetFade(float Alpha);   // 0 = gone. Bright ones dissolve; others hide.

	UPROPERTY() TObjectPtr<USkeletalMeshComponent> Garment;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HeldProps;

	FVigilPoseLayers Pose;        // targets; eased into the anim instance each frame
	FVigilPoseLayers PoseNow;
	bool bPlayer = false;
	bool bMoving = false;
	FVector MoveTarget = FVector::ZeroVector;
	float MoveSpeed = 150.f;
	float TargetYaw = 0.f;
	bool bHasYaw = false;
	float Fade = 1.f;
	float AnimTime = 0.f;
	float StuckTime = 0.f;
};

// A static decoration built from one or more meshes.
UCLASS()
class ORTHODOXVIGIL_API AVigilProp : public AActor
{
	GENERATED_BODY()
public:
	AVigilProp();
	UStaticMeshComponent* AddMesh(UStaticMesh* Mesh, FVector Loc = FVector::ZeroVector, FRotator Rot = FRotator::ZeroRotator, FVector Scale = FVector(1), bool bCollide = false, UMaterialInterface* Mat = nullptr);
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
};

// Something the priest can use: E within Radius shows the Prompt and calls the game mode with Id.
UCLASS()
class ORTHODOXVIGIL_API AVigilInteract : public AVigilProp
{
	GENERATED_BODY()
public:
	FString Id;
	FString Prompt;
	float Radius = 170.f;
	bool bEnabled = true;
	bool bGlint = false;        // a faint mote marks rite targets
	float Height = 100.f;       // where the prompt floats
};

// Box trigger. While the player is inside, listed components are hidden (walls and roofs between camera and player).
UCLASS()
class ORTHODOXVIGIL_API AVigilZone : public AActor
{
	GENERATED_BODY()
public:
	AVigilZone();
	virtual void Tick(float Dt) override;
	void SetBox(FVector Center, FVector HalfExtent, float Yaw = 0.f);
	bool Contains(FVector P) const;
	FString Id;
	FTransform Box;
	FVector Half = FVector(100);
	bool bInside = false;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HideWhenInside;
};
