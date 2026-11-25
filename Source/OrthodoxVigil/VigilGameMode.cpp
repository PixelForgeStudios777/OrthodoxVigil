#include "VigilGameMode.h"
#include "VigilActors.h"
#include "VigilPlayer.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Sound/SoundBase.h"

AVigilGameMode::AVigilGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	DefaultPawnClass = AVigilFigure::StaticClass();
	PlayerControllerClass = AVigilPlayerController::StaticClass();
	HUDClass = AVigilHUD::StaticClass();
}

AVigilGameMode* AVigilGameMode::Get(const UObject* Ctx)
{
	UWorld* W = (Ctx && GEngine) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
	return W ? Cast<AVigilGameMode>(W->GetAuthGameMode()) : nullptr;
}

// ---------------------------------------------------------------- assets

UObject* AVigilGameMode::LoadCached(const FString& Path, UClass* Class)
{
	if (TObjectPtr<UObject>* F = Cache.Find(Path)) return F->Get();
	UObject* O = StaticLoadObject(Class, nullptr, *Path);
	if (!O) UE_LOG(LogTemp, Warning, TEXT("VIGIL missing asset %s"), *Path);
	Cache.Add(Path, O);
	return O;
}

UStaticMesh* AVigilGameMode::Mesh(const FString& N)
{
	return Cast<UStaticMesh>(LoadCached(FString::Printf(TEXT("/Game/Vigil/Meshes/%s.%s"), *N, *N), UStaticMesh::StaticClass()));
}

UMaterialInterface* AVigilGameMode::Mat(const FString& N)
{
	return Cast<UMaterialInterface>(LoadCached(FString::Printf(TEXT("/Game/Vigil/Materials/%s.%s"), *N, *N), UMaterialInterface::StaticClass()));
}

UTexture2D* AVigilGameMode::Tex(const FString& N)
{
	return Cast<UTexture2D>(LoadCached(FString::Printf(TEXT("/Game/Vigil/Textures/%s.%s"), *N, *N), UTexture2D::StaticClass()));
}

USoundBase* AVigilGameMode::Sound(const FString& N)
{
	return Cast<USoundBase>(LoadCached(FString::Printf(TEXT("/Game/Vigil/Audio/%s.%s"), *N, *N), USoundBase::StaticClass()));
}

UFont* AVigilGameMode::SerifFont(bool bBold)
{
	TObjectPtr<UFont>& Slot = bBold ? SerifBold : Serif;
	if (Slot) return Slot;
	const TCHAR* Face = bBold ? TEXT("/Game/Vigil/Fonts/GaramondBold.GaramondBold") : TEXT("/Game/Vigil/Fonts/Garamond.Garamond");
	UFontFace* FF = LoadObject<UFontFace>(nullptr, Face);
	if (!FF) return nullptr;
	UFont* F = NewObject<UFont>(this);
	F->FontCacheType = EFontCacheType::Runtime;
	FTypefaceEntry E(TEXT("Regular"));
	E.Font = FFontData(FF);
	F->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(E);
	Slot = F;
	return F;
}

void AVigilGameMode::Play(const FString& N, float Vol, float Pitch)
{
	if (USoundBase* S = Sound(N)) UGameplayStatics::PlaySound2D(this, S, Vol, Pitch);
}

void AVigilGameMode::PlayAt(const FString& N, FVector Where, float Vol, float Pitch)
{
	if (USoundBase* S = Sound(N)) UGameplayStatics::PlaySoundAtLocation(this, S, Where, Vol, Pitch);
}

void AVigilGameMode::Loop(const FString& Channel, const FString& SoundName, float Vol, float FadeTime)
{
	if (TObjectPtr<UAudioComponent>* Old = Loops.Find(Channel))
	{
		if (*Old && (*Old)->Sound == Sound(SoundName))
		{
			(*Old)->AdjustVolume(FadeTime, Vol);
			return;
		}
		if (*Old) (*Old)->FadeOut(FadeTime, 0.f);
	}
	USoundBase* S = Sound(SoundName);
	if (!S) return;
	UAudioComponent* A = UGameplayStatics::CreateSound2D(this, S, Vol, 1.f, 0.f, nullptr, false, false);
	if (A)
	{
		A->bIsUISound = true;   // keeps playing over pause menus
		A->FadeIn(FadeTime, Vol);
		Loops.Add(Channel, A);
	}
}

void AVigilGameMode::StopLoop(const FString& Channel, float FadeTime)
{
	if (TObjectPtr<UAudioComponent>* Old = Loops.Find(Channel))
	{
		if (*Old) (*Old)->FadeOut(FadeTime, 0.f);
		Loops.Remove(Channel);
	}
}

// ---------------------------------------------------------------- lifecycle

APawn* AVigilGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// the priest is built in BeginPlay; spawn him here whatever the terrain says
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVigilFigure* F = GetWorld()->SpawnActor<AVigilFigure>(AVigilFigure::StaticClass(), FVector(3000, 800, 300), FRotator::ZeroRotator, P);
	Priest = F;
	return F;
}

void AVigilGameMode::BeginPlay()
{
	Super::BeginPlay();
	bAutoPilot = FParse::Param(FCommandLine::Get(), TEXT("VigilAutoPlay"));
	bShots = bAutoPilot || FParse::Param(FCommandLine::Get(), TEXT("VigilShots"));
	Rig = GetWorld()->SpawnActor<AVigilCameraRig>(AVigilCameraRig::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	BuildWorld();
	if (Priest)
	{
		Priest->bPlayer = true;
		Priest->Setup(TEXT("SK_Alexios"), true);
		// the clay lamp in his left hand, and its small light
		UStaticMeshComponent* L = Priest->Hold(TEXT("SM_HandLamp"), TEXT("hand_l"), FVector(4, 6, -2), FRotator(0, 90, 0));
		HandLamp = Lamp(Priest, FVector(0, 0, 0), 3.5f, 650.f, FLinearColor(1.f, 0.62f, 0.32f), true);
		if (L && HandLamp)
		{
			HandLamp->AttachToComponent(L, FAttachmentTransformRules::KeepRelativeTransform);
			HandLamp->SetRelativeLocation(FVector(8, 0, 8));
		}
		if (UStaticMeshComponent* Fl = Priest->Hold(TEXT("SM_Flame"), TEXT("hand_l"), FVector(4, 6, -2), FRotator::ZeroRotator))
		{
			Fl->AttachToComponent(L, FAttachmentTransformRules::KeepRelativeTransform);
			Fl->SetRelativeLocation(FVector(8, 0, 3));
			Fl->SetCastShadow(false);
			Fl->SetMaterial(0, Mat(TEXT("MI_Flame")));
		}
	}
	SetLighting(EVLight::Dusk);
	Screen = EVScreen::Title;
	FadeAlpha = 1.f; FadeTarget = 0.f; FadeSpeed = 0.6f;
	Loop(TEXT("music"), TEXT("mus_title"), 0.6f, 3.f);
	if (bAutoPilot) { StartGame(); AutoBegin(); }
	FString Jump;
	if (FParse::Value(FCommandLine::Get(), TEXT("VigilJump="), Jump)) JumpTo(Jump);
	FParse::Value(FCommandLine::Get(), TEXT("VigilShotAt="), ShotAt);
}

void AVigilGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, FadeTarget, Dt, FadeSpeed);
	if (Screen == EVScreen::Title) return;
	ChapterTime += Dt;
	Commit();
	RunQueue(Dt);
	SystemsTick(Dt);
	StoryTick(Dt);
	if (Rite.bActive) RiteTick(Dt);
	if (bUnderground) OssuaryTick(Dt);
	if (bAutoPilot) AutoTick(Dt);
	if (ShotAt > 0.f && ChapterTime > ShotAt)
	{
		ShotAt = -1.f;
		bShots = true;
		Shot(TEXT("jump"));
		FTimerHandle Th;
		GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); }), 1.5f, false);
	}
}

// ---------------------------------------------------------------- story queue

static FVStep MakeStep(EVStep K) { FVStep S; S.Kind = K; return S; }

void AVigilGameMode::Say(const FString& Speaker, const FString& Text, bool bCoptic)
{
	FVStep S = MakeStep(EVStep::Line);
	S.Speaker = Speaker;
	S.Text = Text;
	S.bCoptic = bCoptic;
	static const TMap<FString, FString> Portraits = {
		{ TEXT("ALEXIOS"), TEXT("T_Portrait_Alexios") }, { TEXT("PATRIARCH"), TEXT("T_Portrait_Patriarch") }, { TEXT("ANASTASIA"), TEXT("T_Portrait_Anastasia") },
		{ TEXT("HOODED MAN"), TEXT("T_Portrait_Hooded") }, { TEXT("THECLA"), TEXT("T_Portrait_Thecla") }, { TEXT("BESA"), TEXT("T_Portrait_Besa") },
		{ TEXT("HOR"), TEXT("T_Portrait_Hor") }, { TEXT("KOSMAS"), TEXT("T_Portrait_Kosmas") }, { TEXT("BRIGHT ONE"), TEXT("T_Portrait_Bright") } };
	if (const FString* P = Portraits.Find(Speaker)) S.Portrait = *P;
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Choose(TArray<FVChoice> Opts)
{
	FVStep S = MakeStep(EVStep::Choice);
	S.Opts = MoveTemp(Opts);
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Wait(float Seconds, bool bLock)
{
	FVStep S = MakeStep(EVStep::Wait);
	S.Time = Seconds;
	S.bLock = bLock;
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::FadeTo(float Alpha, float Seconds)
{
	FVStep S = MakeStep(EVStep::Fade);
	S.Value = Alpha;
	S.Time = Seconds;
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Card(const FString& Title, const FString& Sub, float Seconds)
{
	FVStep S = MakeStep(EVStep::Card);
	S.Text = Title;
	S.Speaker = Sub;
	S.Time = Seconds;
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Do(TFunction<void()> Fn)
{
	FVStep S = MakeStep(EVStep::Call);
	S.Fn = MoveTemp(Fn);
	S.bLock = false;
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Until(TFunction<bool()> Pred, const FString& InHint, bool bLock)
{
	FVStep S = MakeStep(EVStep::Until);
	S.Pred = MoveTemp(Pred);
	S.Text = InHint;
	S.bLock = bLock;
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Doc(const FString& Title, TArray<FString> Pages)
{
	FVStep S = MakeStep(EVStep::Doc);
	S.Text = Title;
	S.Pages = MoveTemp(Pages);
	Staging.Add(MoveTemp(S));
}

void AVigilGameMode::Commit()
{
	if (Staging.Num() == 0) return;
	TArray<FVStep> NewQ = MoveTemp(Staging);
	Staging.Reset();
	NewQ.Append(MoveTemp(Queue));
	Queue = MoveTemp(NewQ);
}

void AVigilGameMode::PopFront()
{
	if (Queue.Num() > 0) Queue.RemoveAt(0);
	StepAge = 0.f;
	StepTimer = 0.f;
	ChoiceSel = 0;
	DocPage = 0;
}

bool AVigilGameMode::Locked() const
{
	return Queue.Num() > 0 && Queue[0].bLock;
}

void AVigilGameMode::RunQueue(float Dt)
{
	for (int32 Guard = 0; Guard < 64 && Queue.Num() > 0; ++Guard)
	{
		FVStep& S = Queue[0];
		const bool bFirst = StepAge == 0.f;
		switch (S.Kind)
		{
		case EVStep::Call:
		{
			TFunction<void()> Fn = MoveTemp(S.Fn);
			PopFront();
			if (Fn) Fn();
			Commit();
			continue;
		}
		case EVStep::Line:
			if (bFirst) Play(TEXT("page"), 0.18f, 1.4f);
			StepAge += Dt;
			if (bAutoPilot && StepAge > 0.15f + S.Text.Len() / 55.f) { PopFront(); continue; }
			return;
		case EVStep::Choice:
			StepAge += Dt;
			return;
		case EVStep::Wait:
			StepAge += Dt;
			if (StepAge >= S.Time) { PopFront(); continue; }
			return;
		case EVStep::Fade:
			if (bFirst) { FadeTarget = S.Value; FadeSpeed = 1.f / FMath::Max(0.05f, S.Time); }
			StepAge += Dt;
			if (FMath::IsNearlyEqual(FadeAlpha, FadeTarget, 0.01f)) { PopFront(); continue; }
			return;
		case EVStep::Card:
			if (bFirst) { CardTitle = S.Text; CardSub = S.Speaker; CardTime = CardTotal = S.Time; }
			StepAge += Dt;
			if (CardTime <= 0.f) { PopFront(); continue; }
			return;
		case EVStep::Until:
			StepAge += Dt;
			if (!S.Text.IsEmpty()) { HintText = S.Text; HintTime = 0.6f; }
			if (!S.Pred || S.Pred()) { HintTime = FMath::Min(HintTime, 0.3f); PopFront(); continue; }
			return;
		case EVStep::Doc:
			if (bFirst) Play(TEXT("page"), 0.5f);
			StepAge += Dt;
			if (bAutoPilot && StepAge > 0.6f) { DocPage++; StepAge = 0.0001f; if (DocPage >= S.Pages.Num()) { PopFront(); continue; } }
			return;
		}
	}
}

void AVigilGameMode::Advance()
{
	if (Queue.Num() == 0) return;
	FVStep& S = Queue[0];
	if (S.Kind == EVStep::Line)
	{
		if (StepAge * 55.f < S.Text.Len()) StepAge = S.Text.Len() / 55.f + 0.01f;   // first press finishes the line
		else PopFront();
	}
	else if (S.Kind == EVStep::Card)
	{
		if (CardTotal - CardTime > 0.8f) CardTime = FMath::Min(CardTime, 0.4f);
	}
	else if (S.Kind == EVStep::Doc)
	{
		Play(TEXT("page"), 0.4f);
		DocPage++;
		if (DocPage >= S.Pages.Num()) PopFront();
	}
}

void AVigilGameMode::ChoiceMove(int32 Dir)
{
	if (Queue.Num() == 0 || Queue[0].Kind != EVStep::Choice) return;
	int32 N = 0;
	for (const FVChoice& C : Queue[0].Opts) if (C.OnceFlag.IsNone() || !Has(C.OnceFlag)) ++N;
	if (N > 0) ChoiceSel = (ChoiceSel + Dir + N) % N;
	Play(TEXT("knot"), 0.3f, 1.3f);
}

void AVigilGameMode::ChoicePick(int32 Index)
{
	if (Queue.Num() == 0 || Queue[0].Kind != EVStep::Choice) return;
	TArray<int32> Vis;
	for (int32 i = 0; i < Queue[0].Opts.Num(); ++i)
		if (Queue[0].Opts[i].OnceFlag.IsNone() || !Has(Queue[0].Opts[i].OnceFlag)) Vis.Add(i);
	const int32 K = Index >= 0 ? Index : ChoiceSel;
	if (!Vis.IsValidIndex(K)) return;
	TFunction<void()> Fn = Queue[0].Opts[Vis[K]].Fn;
	const FName Once = Queue[0].Opts[Vis[K]].OnceFlag;
	PopFront();
	if (!Once.IsNone()) Set(Once);
	if (Fn) Fn();
	Commit();
}

// ---------------------------------------------------------------- state helpers

void AVigilGameMode::Journal(const FString& Title, const FString& Text)
{
	for (const FVJournal& E : JournalEntries) if (E.Title == Title) return;
	JournalEntries.Add({ Title, Text });
	Hint(FString::Printf(TEXT("Journal: %s"), *Title), 3.5f);
	Play(TEXT("page"), 0.5f, 0.9f);
}

void AVigilGameMode::Give(FName Item, const FString& Display)
{
	if (!Items.Contains(Item)) Items.Add(Item);
	ItemNames.Add(Item, Display);
	Hint(FString::Printf(TEXT("Taken: %s"), *Display), 3.f);
	Play(TEXT("pickup"), 0.5f);
}

void AVigilGameMode::Sub(const FString& Speaker, const FString& Text, float Seconds, bool bCoptic)
{
	SubSpeaker = Speaker;
	SubText = Text;
	SubTime = Seconds;
	bSubCoptic = bCoptic;
}

void AVigilGameMode::Shake(float Amount) { ShakeAmount = FMath::Max(ShakeAmount, Amount); }

void AVigilGameMode::Flash(FLinearColor C, float Seconds)
{
	FlashColor = C;
	FlashTime = FlashTotal = Seconds;
}

void AVigilGameMode::Hurt(float C) { Composure = FMath::Clamp(Composure - C, 0.f, 100.f); }
void AVigilGameMode::Calm(float C) { Composure = FMath::Clamp(Composure + C, 0.f, 100.f); }

// ---------------------------------------------------------------- systems

void AVigilGameMode::SystemsTick(float Dt)
{
	HintTime = FMath::Max(0.f, HintTime - Dt);
	SubTime = FMath::Max(0.f, SubTime - Dt);
	CardTime = FMath::Max(0.f, CardTime - Dt);
	FlashTime = FMath::Max(0.f, FlashTime - Dt);
	ObjectiveFlash = FMath::Max(0.f, ObjectiveFlash - Dt);
	ShakeAmount = FMath::Max(0.f, ShakeAmount - Dt * 1.5f);
	KnotFlash = FMath::Max(0.f, KnotFlash - Dt * 2.f);
	GuestFlash = FMath::Max(0.f, GuestFlash - Dt);

	// prayer rope: a ring closes on the knots; Space as it meets them completes a knot
	if (bPraying)
	{
		KnotPhase += Dt / 1.35f;
		if (KnotPhase > 1.08f) { KnotPhase = 0.f; }
	}
	// cross: the flame ring burns down only while something presses against it
	if (bCrossUp && bCrossPressed) CrossRing = FMath::Max(0.f, CrossRing - Dt / 8.f);
	else CrossRing = FMath::Min(1.f, CrossRing + Dt * (bCrossUp ? 0.05f : 0.25f));

	// composure: low composure is heard before it is seen
	const float Lost = 1.f - Composure / 100.f;
	if (Lost > 0.35f && Screen == EVScreen::Playing) Loop(TEXT("heart"), TEXT("heartbeat"), FMath::Clamp((Lost - 0.35f) * 1.6f, 0.f, 0.9f), 0.5f);
	else StopLoop(TEXT("heart"), 1.f);
	if (Composure <= 0.f && !Has("faltering") && Screen == EVScreen::Playing)
	{
		Set("faltering");
		Play(TEXT("sting"), 0.6f, 0.8f);
		Hint(TEXT("Alexios falters. Hold Q and pray."), 6.f);
	}
	if (Has("faltering"))
	{
		if (Priest) Priest->Pose.Bow = 1.f;
		if (KnotsThisPrayer >= 2) { Flags.Remove("faltering"); Composure = FMath::Max(Composure, 35.f); if (Priest) Priest->Pose.Bow = 0.f; }
		else { HintText = TEXT("Alexios falters. Hold Q and pray."); HintTime = 0.5f; }
	}

	// the hand lamp breathes
	if (HandLamp)
	{
		const float T = GetWorld()->GetTimeSeconds();
		const float Base = LightNow == EVLight::Day ? 0.f : (bUnderground ? 5.0f : 3.5f);
		HandLamp->SetIntensity(Base * (0.9f + 0.06f * FMath::Sin(T * 11.f) + 0.04f * FMath::Sin(T * 23.f)) * (0.55f + 0.45f * Composure / 100.f));
		HandLamp->SetAttenuationRadius(bUnderground ? 420.f + 180.f * Composure / 100.f : 650.f);
	}

	if (Priest)
	{
		Priest->Pose.Pray = bPraying ? 1.f : 0.f;
		Priest->Pose.Cross = (bCrossUp && !bPraying) ? 1.f : 0.f;
		// holding up the cross he keeps facing what he holds off, and backs away
		Priest->GetCharacterMovement()->bOrientRotationToMovement = !bCrossUp;
	}
}

// ---------------------------------------------------------------- input

void AVigilGameMode::MoveInput(FVector2D Axis, bool bHurry)
{
	if (!Priest) return;
	UCharacterMovementComponent* M = Priest->GetCharacterMovement();
	if (Screen != EVScreen::Playing || Locked() || Has("faltering") || Axis.IsNearlyZero())
	{
		return;
	}
	const float Yaw = Rig ? Rig->Yaw : -135.f;
	const FVector Fwd = FRotator(0, Yaw, 0).Vector();
	const FVector Right = FRotator(0, Yaw + 90.f, 0).Vector();
	FVector Dir = Fwd * Axis.Y + Right * Axis.X;
	const float Mag = FMath::Min(1.f, Dir.Size());
	Dir = Dir.GetSafeNormal();
	float Speed = bHurry ? 380.f : 200.f;
	if (bUnderground) Speed = FMath::Min(Speed, 190.f);
	if (bPraying) Speed *= 0.4f;
	if (bCrossUp) Speed *= 0.5f;
	Speed *= 0.65f + 0.35f * FMath::Clamp(Composure / 60.f, 0.f, 1.f);
	M->MaxWalkSpeed = Speed;
	Priest->AddMovementInput(Dir, Mag);
}

void AVigilGameMode::OnInteractPressed()
{
	if (Screen == EVScreen::Playing && Has("striking"))
	{
		// I-6 step 7: each press is one knock on the board
		Rite.Strikes++;
		Play(TEXT("semantron"), 1.f, 0.96f + 0.02f * (Rite.Strikes % 3));
		Shake(0.12f);
		if (Priest) { Priest->Pose.Reach = 1.f; FTimerHandle Th; GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Priest) Priest->Pose.Reach = 0.f; }), 0.15f, false); }
		return;
	}
	if (Screen != EVScreen::Playing || Locked() || !Priest || Has("faltering")) return;
	AVigilInteract* Best = nullptr;
	float BestD = 1e9f;
	const FVector P = Priest->GetActorLocation();
	for (AVigilInteract* I : Usables)
	{
		if (!I || !I->bEnabled || I->IsHidden()) continue;
		const float D = FVector::Dist2D(I->GetActorLocation(), P);
		const float Score = I->bGlint ? D * 0.35f : D;   // the current rite target wins over anything beside it
		if (D < I->Radius && Score < BestD && FMath::Abs(I->GetActorLocation().Z - P.Z) < 400.f) { Best = I; BestD = Score; }
	}
	if (!Best) return;
	Priest->FacePoint(Best->GetActorLocation(), false);
	if (Rite.bActive) RiteInteract(Best->Id);
	else OnInteract(Best->Id);
	Commit();
}

void AVigilGameMode::OnPrayHeld(bool bHeld)
{
	if (!bHasRope || Screen != EVScreen::Playing) { bPraying = false; return; }
	if (Locked() && !Has("faltering")) bHeld = false;
	if (bHeld && !bPraying) { KnotPhase = 0.f; KnotsThisPrayer = 0; }
	bPraying = bHeld;
}

void AVigilGameMode::OnKnotPressed()
{
	if (!bPraying) return;
	if (KnotPhase > 0.82f)
	{
		KnotsThisPrayer++;
		KnotsTotal++;
		KnotFlash = 0.6f;
		Calm(9.f);
		Play(TEXT("knot"), 0.6f);
		static const TCHAR* Prayer = TEXT("Lord Jesus Christ, Son of God, have mercy on me.");
		Sub(TEXT(""), Prayer, 2.2f);
		Set("prayed");
	}
	else
	{
		Play(TEXT("knot"), 0.25f, 0.7f);
	}
	KnotPhase = 0.f;
}

void AVigilGameMode::OnCrossHeld(bool bHeld)
{
	if (!bHasCross || Screen != EVScreen::Playing) { bCrossUp = false; return; }
	if (Locked() && !Has("cross_tutorial")) bHeld = false;
	if (bHeld && !bCrossUp) { Play(TEXT("cross_hum"), 0.0f); Loop(TEXT("cross"), TEXT("cross_hum"), 0.35f, 0.3f); }
	if (!bHeld && bCrossUp) StopLoop(TEXT("cross"), 0.5f);
	bCrossUp = bHeld;
}

void AVigilGameMode::OnHolyWater()
{
	if (!bHasFlask || Screen != EVScreen::Playing || !Priest) return;
	if (Locked() && !Has("await_sprinkle")) return;
	Priest->Pose.Sprinkle = 1.f;
	FTimerHandle Th;
	GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Priest) Priest->Pose.Sprinkle = 0.f; }), 0.9f, false);
	if (Rite.bActive) { Play(TEXT("sprinkle"), 0.8f); RiteSprinkle(); return; }
	if (Has("await_sprinkle"))
	{
		if (HolyWater <= 0) { Hint(TEXT("The flask is empty.")); return; }
		HolyWater--;
		Play(TEXT("sprinkle"), 0.9f);
		Flags.Remove("await_sprinkle");
		Set("sprinkled");
		return;
	}
	if (OssBright && OssState == 1 && FVector::Dist2D(OssBright->GetActorLocation(), Priest->GetActorLocation()) < 500.f && HolyWater > 0)
	{
		HolyWater--;
		Play(TEXT("sprinkle"), 0.9f);
		Set("oss_banished");
		return;
	}
	Hint(TEXT("Use it sparingly; there is no font on the road."), 3.f);
}

void AVigilGameMode::OnJournal()
{
	if (Screen == EVScreen::Journal) { Screen = EVScreen::Playing; UGameplayStatics::SetGamePaused(this, false); return; }
	if (Screen != EVScreen::Playing) return;
	Screen = EVScreen::Journal;
	JournalScroll = 0.f;
	Play(TEXT("page"), 0.5f);
	UGameplayStatics::SetGamePaused(this, true);
}

void AVigilGameMode::OnPause()
{
	if (Screen == EVScreen::Paused) { Screen = EVScreen::Playing; UGameplayStatics::SetGamePaused(this, false); return; }
	if (Screen != EVScreen::Playing) return;
	Screen = EVScreen::Paused;
	MenuSel = 0;
	UGameplayStatics::SetGamePaused(this, true);
}

void AVigilGameMode::OnMenuMove(int32 Dir)
{
	const int32 N = Screen == EVScreen::Paused ? 3 : 2;
	MenuSel = (MenuSel + Dir + N) % N;
	Play(TEXT("knot"), 0.3f, 1.2f);
}

void AVigilGameMode::OnMenuPick()
{
	Play(TEXT("semantron"), 0.35f, 1.4f);
	if (Screen == EVScreen::Title)
	{
		if (MenuSel == 0) StartGame();
		else UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	}
	else if (Screen == EVScreen::Paused)
	{
		if (MenuSel == 0) OnPause();
		else if (MenuSel == 1) { Screen = EVScreen::Playing; OnJournal(); }
		else { UGameplayStatics::SetGamePaused(this, false); UGameplayStatics::OpenLevel(this, TEXT("Tarsenis")); }
	}
	else if (Screen == EVScreen::End)
	{
		if (MenuSel == 0) UGameplayStatics::OpenLevel(this, TEXT("Tarsenis"));
		else UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
	}
}

AVigilInteract* AVigilGameMode::FindUsable(const FString& Id) const
{
	for (AVigilInteract* I : Usables) if (I && I->Id == Id) return I;
	return nullptr;
}

void AVigilGameMode::Enable(const FString& Id, bool bOn)
{
	for (AVigilInteract* I : Usables) if (I && I->Id == Id) I->bEnabled = bOn;
}

AVigilFigure* AVigilGameMode::NPC(const FString& Name) const
{
	const TObjectPtr<AVigilFigure>* F = Figures.Find(Name);
	return F ? F->Get() : nullptr;
}
