#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "VigilPlayer.generated.h"

class UCameraComponent;
class UFont;
class UTexture2D;

// Isometric follow camera with fixed shots for scripted moments.
UCLASS()
class ORTHODOXVIGIL_API AVigilCameraRig : public AActor
{
	GENERATED_BODY()
public:
	AVigilCameraRig();
	virtual void Tick(float Dt) override;
	void Follow(float Dist = 2500.f, float Pitch = -58.f, float Yaw = -135.f, float Fov = 32.f, float Blend = 3.f);
	void Fixed(FVector Loc, FRotator Rot, float Fov = 40.f, float Blend = 2.5f);
	void Snap();
	UPROPERTY() TObjectPtr<UCameraComponent> Cam;
	bool bFollow = true;
	float Dist = 2500.f, Pitch = -58.f, Yaw = -135.f, Fov = 32.f, Blend = 3.f;
	FVector FixedLoc = FVector::ZeroVector;
	FRotator FixedRot = FRotator::ZeroRotator;
	FVector Focus = FVector::ZeroVector;
	float LookAhead = 1.f;
private:
	FVector CurLoc = FVector::ZeroVector;
	FRotator CurRot = FRotator::ZeroRotator;
	float CurFov = 32.f;
	bool bSnapped = false;
};

UCLASS()
class ORTHODOXVIGIL_API AVigilPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AVigilPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float Dt) override;
private:
	bool Pressed(const FKey& K) const { return WasInputKeyJustPressed(K); }
	bool Down(const FKey& K) const { return IsInputKeyDown(K); }
	bool AnyPressed(std::initializer_list<FKey> Keys) const;
	bool AnyDown(std::initializer_list<FKey> Keys) const;
	float StickRepeat = 0.f;
};

UCLASS()
class ORTHODOXVIGIL_API AVigilHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	void Text(const FString& S, float X, float Y, const FLinearColor& C, float Size, bool bCenter = false, bool bSerif = true, bool bBold = false, float Shadow = 0.85f);
	float TextWidth(const FString& S, float Size, bool bSerif = true, bool bBold = false);
	TArray<FString> Wrap(const FString& S, float Size, float MaxW, bool bSerif = true);
	void Image(UTexture2D* T, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White, EBlendMode Blend = BLEND_Translucent);
	void Frame(float X, float Y, float W, float H, float Alpha);
	void DrawTitle(class AVigilGameMode* GM);
	void DrawDialogue(class AVigilGameMode* GM);
	void DrawJournal(class AVigilGameMode* GM);
	void DrawWorldPrompt(class AVigilGameMode* GM);
	void DrawComposure(class AVigilGameMode* GM);
	FString Coptic(const FString& S, float Reveal, float T) const;
	float U = 1.f, W = 1920.f, H = 1080.f, Now = 0.f;
};
