// Development only: a robot that plays the slice from the river to the end card through the same entry points
// the player's keys use, answers prompts, logs every beat and takes screenshots. Run with -VigilAutoPlay.
#include "VigilGameMode.h"
#include "VigilActors.h"
#include "VigilPlayer.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"

static FVector M(float X, float Y, float Z = 0.f) { return FVector(X * 100.f, Y * 100.f, Z * 100.f); }
static const FVector OSS = FVector(-1400.f, 0.f, -4000.f);

struct FVAuto
{
	FString Name;
	TFunction<bool(AVigilGameMode*, float)> Fn;   // returns true when done
	float Timeout = 90.f;
};

static TArray<FVAuto> GSteps;
static int32 GStep = 0;
static float GStepTime = 0.f;
static int32 GFailures = 0;
static FVector GLastPos = FVector::ZeroVector;
static float GStuckTime = 0.f;
static int32 GStuckTries = 0;
static float GPrayHold = 0.f;
static bool GQuitQueued = false;

void AVigilGameMode::Shot(const FString& Name)
{
	if (!bShots) return;
	FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("vigil_%s.png"), *Name), true, false);
	UE_LOG(LogTemp, Display, TEXT("VIGILAUTO shot %s composure=%.0f water=%d chapter=%s"), *Name, Composure, HolyWater, *Chapter);
	if (Rig && Priest)
	{
		// what stands between the camera and the priest?
		TArray<FHitResult> Hits;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(VigilShot), true);
		Q.AddIgnoredActor(Priest);
		GetWorld()->LineTraceMultiByChannel(Hits, Rig->GetActorLocation(), Priest->GetActorLocation(), ECC_Camera, Q);
		for (const FHitResult& H : Hits)
			UE_LOG(LogTemp, Display, TEXT("VIGILAUTO   between camera and priest: %s / %s at z=%.0f"), H.GetActor() ? *H.GetActor()->GetName() : TEXT("?"), H.GetComponent() ? *H.GetComponent()->GetName() : TEXT("?"), H.ImpactPoint.Z);
		UE_LOG(LogTemp, Display, TEXT("VIGILAUTO   camera at (%.0f, %.0f, %.0f) priest at (%.0f, %.0f, %.0f) lamp=%.2f"), Rig->GetActorLocation().X, Rig->GetActorLocation().Y, Rig->GetActorLocation().Z,
			Priest->GetActorLocation().X, Priest->GetActorLocation().Y, Priest->GetActorLocation().Z, HandLamp ? HandLamp->Intensity : -1.f);
	}
}

// walk the priest towards a point; true when within Tol. Recovers from walls by sidestepping, then reports STUCK.
static bool Walk(AVigilGameMode* GM, FVector Target, float Dt, float Tol = 80.f)
{
	AVigilFigure* P = GM->Priest;
	if (!P) return true;
	const FVector Here = P->GetActorLocation();
	if (FVector::Dist2D(Here, Target) < Tol) { P->Stop(); GStuckTime = 0.f; GStuckTries = 0; return true; }
	if (FVector::Dist2D(Here, GLastPos) < 3.f) GStuckTime += Dt; else GStuckTime = 0.f;
	GLastPos = Here;
	if (GStuckTime > 1.5f)
	{
		GStuckTime = 0.f;
		if (++GStuckTries > 4)
		{
			GFailures++;
			UE_LOG(LogTemp, Warning, TEXT("VIGILAUTO STUCK at (%.0f, %.0f, %.0f) heading to (%.0f, %.0f) in step %s"), Here.X, Here.Y, Here.Z, Target.X, Target.Y, GSteps.IsValidIndex(GStep) ? *GSteps[GStep].Name : TEXT("?"));
			P->SetActorLocation(FVector(Target.X, Target.Y, Here.Z + 60.f), false, nullptr, ETeleportType::TeleportPhysics);
			GStuckTries = 0;
			return true;
		}
		// sidestep, alternating sides
		const FVector Dir = (Target - Here).GetSafeNormal2D();
		const FVector Side = FVector(-Dir.Y, Dir.X, 0) * ((GStuckTries % 2) ? 1.f : -1.f);
		P->WalkTo(Here + Side * 160.f - Dir * 40.f, 250.f);
		return false;
	}
	if (!P->bMoving || FVector::Dist2D(P->MoveTarget, Target) > 5.f)
	{
		if (GStuckTime == 0.f || !P->bMoving) P->WalkTo(FVector(Target.X, Target.Y, Here.Z), 300.f);
	}
	return false;
}

static bool Idle(AVigilGameMode* GM) { return GM->Queue.Num() == 0 && GM->Staging.Num() == 0; }

static FVAuto Go(const FString& Name, FVector T, float Tol = 80.f)
{
	return { Name, [T, Tol](AVigilGameMode* GM, float Dt) { return Walk(GM, T, Dt, Tol); }, 60.f };
}

// walk to a usable and press E there, then wait for what it starts to finish
static FVAuto Use(const FString& Id, float Timeout = 120.f)
{
	struct FState { int32 Phase = 0; };
	TSharedRef<FState> S = MakeShared<FState>();
	return { FString::Printf(TEXT("use %s"), *Id), [Id, S](AVigilGameMode* GM, float Dt) {
		AVigilInteract* U = GM->FindUsable(Id);
		if (!U) { UE_LOG(LogTemp, Warning, TEXT("VIGILAUTO no usable %s"), *Id); GFailures++; return true; }
		if (S->Phase == 0)
		{
			if (!Idle(GM)) return false;
			if (Walk(GM, U->GetActorLocation(), Dt, FMath::Min(U->Radius * 0.55f, 80.f))) S->Phase = 1;
			return false;
		}
		if (S->Phase == 1)
		{
			if (!U->bEnabled) { UE_LOG(LogTemp, Warning, TEXT("VIGILAUTO %s is disabled"), *Id); GFailures++; return true; }
			GM->OnInteractPressed();
			S->Phase = 2;
			return false;
		}
		return Idle(GM);
	}, Timeout };
}

static FVAuto WaitFor(const FString& Name, TFunction<bool(AVigilGameMode*)> P, float Timeout = 120.f)
{
	return { Name, [P](AVigilGameMode* GM, float) { return P(GM); }, Timeout };
}

void AVigilGameMode::AutoBegin()
{
	GSteps.Reset();
	GStep = 0; GStepTime = 0.f; GFailures = 0; GQuitQueued = false;
	const FVector B = M(-14, 0);
	GSteps.Add(WaitFor(TEXT("reach the village"), [](AVigilGameMode* GM) { return GM->Chapter == TEXT("village") && Idle(GM); }, 400.f));
	GSteps.Add({ TEXT("shot landing"), [](AVigilGameMode* GM, float) { GM->Shot(TEXT("landing")); return true; } });
	GSteps.Add(Go(TEXT("down the landing stair"), M(26.5f, 8.0f)));
	GSteps.Add(Go(TEXT("into the village"), M(21.5f, 4.5f)));
	GSteps.Add(Use(TEXT("door_5")));
	GSteps.Add(Use(TEXT("bread_5")));
	GSteps.Add({ TEXT("shot village"), [](AVigilGameMode* GM, float) { GM->Shot(TEXT("village")); return true; } });
	GSteps.Add(WaitFor(TEXT("meet the hooded man"), [](AVigilGameMode* GM) { return GM->Has("met_hooded_man") && Idle(GM); }, 150.f));
	GSteps.Add(Use(TEXT("well"), 200.f));
	GSteps.Add(WaitFor(TEXT("well cleansed"), [](AVigilGameMode* GM) { return GM->Has("well_cleansed") && Idle(GM); }, 100.f));
	GSteps.Add(Go(TEXT("towards the church"), M(-4.0f, 3.5f)));
	GSteps.Add(Use(TEXT("church_door")));
	GSteps.Add(Go(TEXT("round to the south wall"), M(-6.0f, 5.0f)));
	GSteps.Add(Use(TEXT("church_window")));
	GSteps.Add(Go(TEXT("back through the square"), M(5.0f, 4.0f)));
	GSteps.Add(Go(TEXT("down to the canal"), M(18.0f, -9.0f)));
	GSteps.Add(Go(TEXT("at the shed"), M(24.5f, -10.5f)));
	GSteps.Add(Go(TEXT("into the shed"), M(24.5f, -12.8f), 60.f));
	GSteps.Add(WaitFor(TEXT("meet Besa"), [](AVigilGameMode* GM) { return GM->Has("met_besa") && Idle(GM); }, 200.f));
	GSteps.Add(Go(TEXT("out of the shed"), M(22.0f, -9.0f)));
	GSteps.Add(Go(TEXT("back to the square"), M(5.0f, 4.0f)));
	GSteps.Add(Go(TEXT("past the church"), M(-6.5f, 5.2f)));
	GSteps.Add(Go(TEXT("south of the church"), M(-20.5f, 5.4f)));
	GSteps.Add(Use(TEXT("bonehouse")));
	GSteps.Add(WaitFor(TEXT("in the bone house"), [](AVigilGameMode* GM) { return GM->Chapter == TEXT("ossuary") && Idle(GM); }));
	GSteps.Add(Use(TEXT("skulls")));
	GSteps.Add(Go(TEXT("west along the passage"), OSS + M(-14.6f, 0)));
	GSteps.Add(WaitFor(TEXT("the bright one comes"), [](AVigilGameMode* GM) { return GM->OssState == 1 && Idle(GM); }, 30.f));
	GSteps.Add({ TEXT("raise the cross and back away"), [](AVigilGameMode* GM, float Dt) {
		AVigilFigure* P = GM->Priest;
		if (GM->OssState != 1) { GM->OnCrossHeld(false); return true; }
		if (GM->OssBright) P->FacePoint(GM->OssBright->GetActorLocation(), true);
		P->GetCharacterMovement()->bOrientRotationToMovement = false;
		GM->OnCrossHeld(true);
		const FVector Here = P->GetActorLocation();
		const FVector T = Here.X > OSS.X - 950.f ? OSS + M(-9.0f, -3.4f) : OSS + M(-9.0f, 0.f);
		P->WalkTo(FVector(T.X, T.Y, Here.Z), 300.f);
		return false;
	}, 60.f });
	GSteps.Add({ TEXT("cross down"), [](AVigilGameMode* GM, float) { GM->OnCrossHeld(false); GM->Priest->GetCharacterMovement()->bOrientRotationToMovement = true; return true; } });
	GSteps.Add(Go(TEXT("up the side passage"), OSS + M(-9.0f, -7.6f)));
	GSteps.Add(Use(TEXT("scratch")));
	GSteps.Add(Use(TEXT("stole")));
	GSteps.Add(Use(TEXT("bones")));
	GSteps.Add(Use(TEXT("oiljar")));
	GSteps.Add(Use(TEXT("trapdoor_up")));
	GSteps.Add(WaitFor(TEXT("in the church"), [](AVigilGameMode* GM) { return GM->Chapter == TEXT("church_night") && Idle(GM); }));
	GSteps.Add(Use(TEXT("altar"), 200.f));
	GSteps.Add(WaitFor(TEXT("the second day"), [](AVigilGameMode* GM) { return GM->Chapter == TEXT("day2") && Idle(GM); }, 120.f));
	GSteps.Add(Use(TEXT("chest")));
	GSteps.Add(Use(TEXT("psalter")));
	GSteps.Add(Use(TEXT("church_exit")));
	GSteps.Add(Use(TEXT("candles")));
	GSteps.Add({ TEXT("shot day"), [](AVigilGameMode* GM, float) { GM->Shot(TEXT("day_outside")); return true; } });
	GSteps.Add(Use(TEXT("altar")));
	GSteps.Add(WaitFor(TEXT("the rite begins"), [](AVigilGameMode* GM) { return GM->Rite.bActive && Idle(GM); }, 60.f));
	GSteps.Add(Use(TEXT("altar")));
	GSteps.Add(Use(TEXT("brazier")));
	GSteps.Add(Use(TEXT("icon_christ")));
	GSteps.Add(Use(TEXT("icon_theotokos")));
	GSteps.Add(Use(TEXT("icon_michael")));
	GSteps.Add(Use(TEXT("icon_forerunner")));
	GSteps.Add(WaitFor(TEXT("candles steady"), [](AVigilGameMode* GM) { return !GM->Rite.bWind; }, 30.f));
	GSteps.Add(Use(TEXT("lampada")));
	for (int32 w = 0; w < 4; ++w) GSteps.Add(Use(FString::Printf(TEXT("wall_%d"), w)));
	GSteps.Add({ TEXT("shot rite"), [](AVigilGameMode* GM, float) { GM->Shot(TEXT("rite")); return true; } });
	GSteps.Add(Use(TEXT("altar")));
	for (int32 w = 0; w < 4; ++w) GSteps.Add(Use(FString::Printf(TEXT("wall_%d"), w)));
	GSteps.Add(Use(TEXT("icon_theotokos"), 10.f));
	GSteps.Add({ TEXT("sprinkle the icon"), [](AVigilGameMode* GM, float Dt) {
		if (!GM->Rite.bIconTurning) return true;
		if (Walk(GM, GM->Icons[1]->GetActorLocation() + FVector(160, 0, 0), Dt, 90.f)) GM->OnHolyWater();
		return !GM->Rite.bIconTurning;
	}, 30.f });
	GSteps.Add(WaitFor(TEXT("step six"), [](AVigilGameMode* GM) { return GM->Rite.Step >= 6 || (GM->Rite.Count >= 5 && !GM->Rite.bIconTurning); }, 10.f));
	GSteps.Add(Use(TEXT("wall_0"), 5.f));   // re-check the step if the icon was set right after the last wall
	GSteps.Add(Use(TEXT("altar")));
	GSteps.Add(Go(TEXT("out of the church"), B + FVector(780, 250, 0)));
	GSteps.Add(Use(TEXT("semantron_frame"), 200.f));
	GSteps.Add(WaitFor(TEXT("church cleansed"), [](AVigilGameMode* GM) { return GM->Has("church_cleansed") && Idle(GM); }, 120.f));
	GSteps.Add(Go(TEXT("back inside"), B + FVector(300, 0, 0)));
	GSteps.Add(Use(TEXT("font")));
	GSteps.Add(Use(TEXT("icon_michael"), 300.f));
	GSteps.Add(WaitFor(TEXT("the end card"), [](AVigilGameMode* GM) { return GM->Screen == EVScreen::End; }, 300.f));
}

void AVigilGameMode::AutoTick(float Dt)
{
	// answer prompts the way a player would
	const bool bUntil = Queue.Num() > 0 && Queue[0].Kind == EVStep::Until;
	const FString HintNow = bUntil ? Queue[0].Text : HintText;
	if (Queue.Num() > 0 && Queue[0].Kind == EVStep::Choice && StepAge > 0.3f) { ChoicePick(0); }
	if (Queue.Num() > 0 && (Queue[0].Kind == EVStep::Card) && StepAge > 1.0f) { Advance(); }
	const bool bWantPray = (bUntil && HintNow.Contains(TEXT("Hold Q"))) || Has("faltering") || (Rite.bWind && HintTime > 0.f);
	if (bWantPray)
	{
		if (!bPraying) OnPrayHeld(true);
		if (KnotPhase > 0.88f) OnKnotPressed();
		GPrayHold = 0.4f;
	}
	else if (GPrayHold > 0.f) { GPrayHold -= Dt; if (GPrayHold <= 0.f) OnPrayHeld(false); }
	if (bUntil && HintNow.Contains(TEXT("Hold F"))) OnCrossHeld(true);
	if (bUntil && HintNow.Contains(TEXT("Press G"))) OnHolyWater();
	if (bUntil && HintNow.Contains(TEXT("strike")))
	{
		static float K = 0.f;
		K -= Dt;
		if (K <= 0.f) { OnInteractPressed(); K = FMath::Max(0.25f, 0.9f - Rite.Strikes * 0.06f); }
	}
	if (Rite.bCandlesOut)
	{
		for (int32 s = 0; s < 2; ++s)
			if (AVigilInteract* C = FindUsable(FString::Printf(TEXT("candlestand_%d"), s)); C && C->bEnabled)
			{
				if (Walk(this, C->GetActorLocation(), Dt, 70.f)) OnInteractPressed();
				return;
			}
	}

	if (GStep >= GSteps.Num())
	{
		if (!GQuitQueued && Screen == EVScreen::End)
		{
			GQuitQueued = true;
			UE_LOG(LogTemp, Display, TEXT("VIGILAUTO done: %d of %d steps failed. journal=%d items=%d composure=%.0f flags: cleansed=%d journal=%d"),
				GFailures, GSteps.Num(), JournalEntries.Num(), Items.Num(), Composure, Has("church_cleansed") ? 1 : 0, Has("read_ioannes_journal") ? 1 : 0);
			FTimerHandle Th;
			GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() {
				Shot(TEXT("end"));
				FTimerHandle T2;
				GetWorldTimerManager().SetTimer(T2, FTimerDelegate::CreateWeakLambda(this, [this]() { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); }), 2.f, false);
			}), 1.5f, false);
		}
		return;
	}
	FVAuto& S = GSteps[GStep];
	GStepTime += Dt;
	const bool bDone = S.Fn(this, Dt);
	if (bDone || GStepTime > S.Timeout)
	{
		if (!bDone) { GFailures++; UE_LOG(LogTemp, Warning, TEXT("VIGILAUTO TIMEOUT step %d %s (queue %d, head %d)"), GStep, *S.Name, Queue.Num(), Queue.Num() ? (int32)Queue[0].Kind : -1); }
		else
		{
			const FVector P = Priest ? Priest->GetActorLocation() : FVector::ZeroVector;
			UE_LOG(LogTemp, Display, TEXT("VIGILAUTO step %d ok: %s (%.1fs) at (%.0f, %.0f, %.0f) composure=%.0f"), GStep, *S.Name, GStepTime, P.X, P.Y, P.Z, Composure);
		}
		GStep++;
		GStepTime = 0.f;
		GStuckTries = 0;
	}
}
