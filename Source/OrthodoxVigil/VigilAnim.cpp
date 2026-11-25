#include "VigilAnim.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"

static float GMeasuredWalk = 0.f;
static float GMeasuredJog = 0.f;
float UVigilAnimInstance::WalkStride() { return GMeasuredWalk; }
float UVigilAnimInstance::JogStride() { return GMeasuredJog; }

static float StrideSpeed(UAnimSequence* Seq, float Fallback)
{
	if (!Seq) return Fallback;
	const double Len = Seq->GetPlayLength();
	if (Len <= 0.0) return Fallback;
	const FTransform Root = Seq->ExtractRootMotionFromRange(0.0, Len, FAnimExtractContext());
	const float S = Root.GetTranslation().Size() / Len;
	return S > 20.f ? S : Fallback;
}

void UVigilAnimInstance::MeasureClips(UAnimSequence* Walk, UAnimSequence* Jog)
{
	if (GMeasuredWalk > 0.f) return;
	GMeasuredWalk = StrideSpeed(Walk, 165.f);
	GMeasuredJog = StrideSpeed(Jog, 380.f);
	UE_LOG(LogTemp, Display, TEXT("VIGIL anim stride speeds: walk %.1f cm/s, jog %.1f cm/s"), GMeasuredWalk, GMeasuredJog);
}

void UVigilAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if (!IdleSeq) IdleSeq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	if (!WalkSeq) WalkSeq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd.MF_Unarmed_Walk_Fwd"));
	if (!JogSeq) JogSeq = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd.MF_Unarmed_Jog_Fwd"));
	MeasureClips(WalkSeq, JogSeq);
}

void FVigilAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	if (UVigilAnimInstance* I = Cast<UVigilAnimInstance>(InAnimInstance))
	{
		Idle = I->IdleSeq;
		Walk = I->WalkSeq;
		Jog = I->JogSeq;
		Speed = I->Speed;
		L = I->Layers;
		WalkSpeed = GMeasuredWalk > 0.f ? GMeasuredWalk : 165.f;
		JogSpeed = GMeasuredJog > 0.f ? GMeasuredJog : 380.f;
		WalkSpeed *= I->PlayRateScale;
		JogSpeed *= I->PlayRateScale;
	}
}

void FVigilAnimProxy::Update(float Dt)
{
	IdleTime += Dt;
	if (Idle && IdleTime > Idle->GetPlayLength()) IdleTime = FMath::Fmod(IdleTime, Idle->GetPlayLength());
	// one shared stride phase so walk and jog keep their feet in step while blending
	const float WJog = FMath::Clamp((Speed - WalkSpeed) / FMath::Max(1.f, JogSpeed - WalkSpeed), 0.f, 1.f);
	const float WalkLen = Walk ? Walk->GetPlayLength() : 1.f;
	const float JogLen = Jog ? Jog->GetPlayLength() : 1.f;
	const float RateWalk = FMath::Clamp(Speed / WalkSpeed, 0.35f, 1.5f) / WalkLen;
	const float RateJog = FMath::Clamp(Speed / JogSpeed, 0.5f, 1.5f) / JogLen;
	float Phase = WalkTime;   // reuse WalkTime as the normalized phase
	Phase += Dt * FMath::Lerp(RateWalk, RateJog, WJog);
	WalkTime = FMath::Fmod(Phase, 1.f);
}

void FVigilAnimProxy::Sample(UAnimSequence* Seq, float Time, FPoseContext& Out)
{
	if (!Seq) { Out.ResetToRefPose(); return; }
	FAnimationPoseData Data(Out);
	Seq->GetAnimationPose(Data, FAnimExtractContext(double(Time), false));
	// lock the root: the clips carry root motion but the capsule does the moving
	const FCompactPoseBoneIndex Root(0);
	Out.Pose[Root] = Out.Pose.GetRefPose(Root);
}

void FVigilAnimProxy::Bend(FPoseContext& Out, FName Bone, const FQuat& Delta)
{
	const FBoneContainer& BC = Out.Pose.GetBoneContainer();
	const int32 MeshIndex = BC.GetPoseBoneIndexForBoneName(Bone);
	if (MeshIndex == INDEX_NONE) return;
	const FCompactPoseBoneIndex CI = BC.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	if (!CI.IsValid()) return;
	const FCompactPoseBoneIndex Parent = Out.Pose.GetParentBoneIndex(CI);
	FQuat ParentRot = FQuat::Identity;
	if (Parent.IsValid())
	{
		FCSPose<FCompactPose> CS;
		CS.InitPose(Out.Pose);
		ParentRot = CS.GetComponentSpaceTransform(Parent).GetRotation();
	}
	FTransform& Local = Out.Pose[CI];
	Local.SetRotation((ParentRot.Inverse() * Delta * ParentRot * Local.GetRotation()).GetNormalized());
}

static FQuat AxisDeg(const FVector& Axis, float Deg)
{
	return FQuat(Axis, FMath::DegreesToRadians(Deg));
}

bool FVigilAnimProxy::Evaluate(FPoseContext& Output)
{
	// locomotion: idle, walk, jog by speed
	const float WIdle = 1.f - FMath::Clamp(Speed / (WalkSpeed * 0.55f), 0.f, 1.f);
	const float WJog = FMath::Clamp((Speed - WalkSpeed) / FMath::Max(1.f, JogSpeed - WalkSpeed), 0.f, 1.f);
	FPoseContext Move(this);
	if (WJog > 0.01f && Jog)
	{
		FPoseContext A(this), B(this);
		Sample(Walk, WalkTime * (Walk ? Walk->GetPlayLength() : 1.f), A);
		Sample(Jog, WalkTime * Jog->GetPlayLength(), B);
		FAnimationPoseData PA(A), PB(B), PM(Move);
		FAnimationRuntime::BlendTwoPosesTogether(PA, PB, 1.f - WJog, PM);
	}
	else
	{
		Sample(Walk, WalkTime * (Walk ? Walk->GetPlayLength() : 1.f), Move);
	}
	if (WIdle > 0.99f)
	{
		Sample(Idle, IdleTime, Output);
	}
	else if (WIdle > 0.01f)
	{
		FPoseContext IdlePose(this);
		Sample(Idle, IdleTime, IdlePose);
		FAnimationPoseData DataIdle(IdlePose), DataMove(Move), DataOut(Output);
		FAnimationRuntime::BlendTwoPosesTogether(DataIdle, DataMove, WIdle, DataOut);
	}
	else
	{
		Output = Move;
	}

	// procedural layers, in mesh component space: +Y forward, +Z up, +X the figure's left
	const FVector Side(1, 0, 0), Fwd(0, 1, 0), Up(0, 0, 1);
	const float T = L.Time;
	if (L.Bow > 0.01f)
	{
		Bend(Output, TEXT("spine_03"), AxisDeg(Side, 12.f * L.Bow));
		Bend(Output, TEXT("neck_01"), AxisDeg(Side, 22.f * L.Bow));
	}
	if (L.Pray > 0.01f)
	{
		const float k = L.Pray;
		Bend(Output, TEXT("upperarm_r"), AxisDeg(Side, 38.f * k) * AxisDeg(Fwd, 14.f * k));
		Bend(Output, TEXT("upperarm_l"), AxisDeg(Side, 38.f * k) * AxisDeg(Fwd, -14.f * k));
		Bend(Output, TEXT("lowerarm_r"), AxisDeg(Side, 78.f * k) * AxisDeg(Up, 30.f * k));
		Bend(Output, TEXT("lowerarm_l"), AxisDeg(Side, 78.f * k) * AxisDeg(Up, -30.f * k));
		Bend(Output, TEXT("neck_01"), AxisDeg(Side, 14.f * k));
	}
	float RightRaise = 0.f, RightBend = 0.f;
	if (L.Cross > 0.01f) { RightRaise += 95.f * L.Cross; RightBend += 10.f * L.Cross; }
	if (L.Sprinkle > 0.01f) { RightRaise += (60.f + 25.f * FMath::Sin(T * 14.f)) * L.Sprinkle; RightBend += 20.f * L.Sprinkle; }
	if (L.Reach > 0.01f) { RightRaise += 70.f * L.Reach; RightBend += 15.f * L.Reach; }
	if (L.Cense > 0.01f) { RightRaise += (35.f + 22.f * FMath::Sin(T * 3.2f)) * L.Cense; RightBend += 25.f * L.Cense; }
	if (L.Lunge > 0.01f)
	{
		Bend(Output, TEXT("upperarm_l"), AxisDeg(Side, 80.f * L.Lunge));
		RightRaise += 120.f * L.Lunge;
	}
	if (RightRaise > 0.1f)
	{
		Bend(Output, TEXT("upperarm_r"), AxisDeg(Side, RightRaise) * AxisDeg(Fwd, 8.f));
		if (RightBend > 0.1f) Bend(Output, TEXT("lowerarm_r"), AxisDeg(Side, RightBend));
	}
	if (L.Point > 0.01f)
	{
		Bend(Output, TEXT("upperarm_l"), AxisDeg(Side, 85.f * L.Point) * AxisDeg(Fwd, -25.f * L.Point));
	}
	return true;
}
