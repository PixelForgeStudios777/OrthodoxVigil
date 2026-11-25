#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "VigilAnim.generated.h"

class UAnimSequence;

// Procedural layers on top of the locomotion, each 0..1.
struct FVigilPoseLayers
{
	float Cross = 0.f;     // right arm raised forward holding the cross
	float Pray = 0.f;      // both hands at the chest with the prayer rope
	float Sprinkle = 0.f;  // right arm flicking forward
	float Reach = 0.f;     // right arm reaching out (turning an icon, taking something)
	float Cense = 0.f;     // right arm swinging the censer
	float Bow = 0.f;       // head and shoulders bowed (faltering, grief)
	float Point = 0.f;     // left arm pointing (Thecla)
	float Lunge = 0.f;     // both arms up (Besa with the knife)
	float Time = 0.f;
};

// Code-only animation: samples idle / walk / jog by speed with matched play rates, then bends arm bones.
// There is no AnimGraph, because one cannot be authored from a script; the proxy evaluates the pose itself.
struct FVigilAnimProxy : public FAnimInstanceProxy
{
	FVigilAnimProxy() {}
	FVigilAnimProxy(UAnimInstance* Inst) : FAnimInstanceProxy(Inst) {}
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

	TObjectPtr<UAnimSequence> Idle, Walk, Jog;
	float WalkSpeed = 165.f, JogSpeed = 380.f;   // stride speed of each clip in cm/s (measured at startup)
	float Speed = 0.f;
	float IdleTime = 0.f, WalkTime = 0.f, JogTime = 0.f;
	FVigilPoseLayers L;

private:
	void Sample(UAnimSequence* Seq, float Time, FPoseContext& Out);
	void Bend(FPoseContext& Out, FName Bone, const FQuat& ComponentDelta);
};

UCLASS(Transient)
class ORTHODOXVIGIL_API UVigilAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeInitializeAnimation() override;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVigilAnimProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }

	float Speed = 0.f;
	float PlayRateScale = 1.f;
	FVigilPoseLayers Layers;

	UPROPERTY() TObjectPtr<UAnimSequence> IdleSeq;
	UPROPERTY() TObjectPtr<UAnimSequence> WalkSeq;
	UPROPERTY() TObjectPtr<UAnimSequence> JogSeq;

	// Stride speeds measured once from the clips' root motion.
	static void MeasureClips(UAnimSequence* Walk, UAnimSequence* Jog);
	static float WalkStride();
	static float JogStride();
};
