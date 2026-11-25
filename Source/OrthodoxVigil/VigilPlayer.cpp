#include "VigilPlayer.h"
#include "VigilGameMode.h"
#include "VigilActors.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Kismet/GameplayStatics.h"

// ================================================================ camera rig

AVigilCameraRig::AVigilCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	RootComponent = Cam;
	Cam->SetFieldOfView(32.f);
	Cam->bConstrainAspectRatio = false;
	Cam->PostProcessBlendWeight = 0.f;
}

void AVigilCameraRig::Follow(float InDist, float InPitch, float InYaw, float InFov, float InBlend)
{
	bFollow = true;
	Dist = InDist; Pitch = InPitch; Yaw = InYaw; Fov = InFov; Blend = InBlend;
}

void AVigilCameraRig::Fixed(FVector Loc, FRotator Rot, float InFov, float InBlend)
{
	bFollow = false;
	FixedLoc = Loc; FixedRot = Rot; Fov = InFov; Blend = InBlend;
}

void AVigilCameraRig::Snap()
{
	bSnapped = false;
}

void AVigilCameraRig::Tick(float Dt)
{
	Super::Tick(Dt);
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	FVector WantLoc;
	FRotator WantRot;
	if (bFollow)
	{
		if (GM && GM->Priest)
		{
			const FVector P = GM->Priest->GetActorLocation() + FVector(0, 0, 40);
			const FVector V = GM->Priest->GetVelocity() * 0.25f * LookAhead;
			Focus = FMath::VInterpTo(Focus.IsNearlyZero() ? P : Focus, P + FVector(V.X, V.Y, 0), Dt, 5.f);
		}
		WantRot = FRotator(Pitch, Yaw, 0);
		WantLoc = Focus - WantRot.Vector() * Dist;
	}
	else
	{
		WantLoc = FixedLoc;
		WantRot = FixedRot;
	}
	if (!bSnapped)
	{
		CurLoc = WantLoc; CurRot = WantRot; CurFov = Fov;
		bSnapped = true;
	}
	else
	{
		const float D = FMath::Min(Dt, 0.1f);
		CurLoc = FMath::VInterpTo(CurLoc, WantLoc, D, Blend * 2.f);
		CurRot = FMath::RInterpTo(CurRot, WantRot, D, Blend * 2.f);
		CurFov = FMath::FInterpTo(CurFov, Fov, D, Blend * 2.f);
	}
	FVector Shake = FVector::ZeroVector;
	if (GM && GM->ShakeAmount > 0.f)
	{
		const float T = GetWorld()->GetRealTimeSeconds();
		Shake = FVector(FMath::Sin(T * 37.f), FMath::Sin(T * 29.f + 1.f), FMath::Sin(T * 41.f + 2.f)) * GM->ShakeAmount * 12.f;
	}
	SetActorLocationAndRotation(CurLoc + Shake, CurRot);
	Cam->SetFieldOfView(CurFov);
}

// ================================================================ controller

AVigilPlayerController::AVigilPlayerController()
{
	bShowMouseCursor = false;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bAutoManageActiveCameraTarget = false;
}

void AVigilPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
}

bool AVigilPlayerController::AnyPressed(std::initializer_list<FKey> Keys) const
{
	for (const FKey& K : Keys) if (WasInputKeyJustPressed(K)) return true;
	return false;
}

bool AVigilPlayerController::AnyDown(std::initializer_list<FKey> Keys) const
{
	for (const FKey& K : Keys) if (IsInputKeyDown(K)) return true;
	return false;
}

void AVigilPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	if (!GM) return;
	if (GM->Rig && GetViewTarget() != GM->Rig) SetViewTarget(GM->Rig);
	if (GM->bAutoPilot) return;

	const bool bUp = AnyPressed({ EKeys::W, EKeys::Up, EKeys::Gamepad_DPad_Up });
	const bool bDown = AnyPressed({ EKeys::S, EKeys::Down, EKeys::Gamepad_DPad_Down });
	const float StickY = GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	StickRepeat -= Dt;
	int32 StickDir = 0;
	if (FMath::Abs(StickY) > 0.6f && StickRepeat <= 0.f) { StickDir = StickY > 0 ? -1 : 1; StickRepeat = 0.25f; }
	if (FMath::Abs(StickY) < 0.3f) StickRepeat = 0.f;
	const bool bAccept = AnyPressed({ EKeys::E, EKeys::Enter, EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom });
	const bool bBack = AnyPressed({ EKeys::Escape, EKeys::Gamepad_Special_Right });

	if (GM->Screen == EVScreen::Title || GM->Screen == EVScreen::Paused || GM->Screen == EVScreen::End)
	{
		if (bUp || StickDir < 0) GM->OnMenuMove(-1);
		if (bDown || StickDir > 0) GM->OnMenuMove(1);
		if (bAccept) GM->OnMenuPick();
		if (bBack && GM->Screen == EVScreen::Paused) GM->OnPause();
		return;
	}
	if (GM->Screen == EVScreen::Journal)
	{
		if (Down(EKeys::W) || Down(EKeys::Up) || StickY > 0.5f) GM->JournalScroll = FMath::Max(0.f, GM->JournalScroll - Dt * 600.f);
		if (Down(EKeys::S) || Down(EKeys::Down) || StickY < -0.5f) GM->JournalScroll += Dt * 600.f;
		if (bBack || AnyPressed({ EKeys::J, EKeys::Tab, EKeys::Gamepad_FaceButton_Top, EKeys::E, EKeys::Gamepad_FaceButton_Right })) GM->OnJournal();
		return;
	}

	// playing
	if (bBack) { GM->OnPause(); return; }
	const bool bChoice = GM->Queue.Num() > 0 && GM->Queue[0].Kind == EVStep::Choice;
	if (bChoice)
	{
		if (bUp || StickDir < 0) GM->ChoiceMove(-1);
		if (bDown || StickDir > 0) GM->ChoiceMove(1);
		const FKey Nums[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six };
		for (int32 i = 0; i < 6; ++i) if (Pressed(Nums[i])) GM->ChoicePick(i);
		if (AnyPressed({ EKeys::E, EKeys::Enter, EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom })) GM->ChoicePick();
		GM->MoveInput(FVector2D::ZeroVector, false);
		return;
	}
	const bool bLineUp = GM->Queue.Num() > 0 && (GM->Queue[0].Kind == EVStep::Line || GM->Queue[0].Kind == EVStep::Doc || GM->Queue[0].Kind == EVStep::Card);
	if (AnyPressed({ EKeys::J, EKeys::Tab, EKeys::Gamepad_FaceButton_Top })) { GM->OnJournal(); return; }

	GM->OnPrayHeld(AnyDown({ EKeys::Q, EKeys::Gamepad_LeftTrigger, EKeys::Gamepad_LeftShoulder }));
	GM->OnCrossHeld(AnyDown({ EKeys::F, EKeys::RightMouseButton, EKeys::Gamepad_RightTrigger, EKeys::Gamepad_RightShoulder }));

	if (GM->bPraying && AnyPressed({ EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom }))
	{
		GM->OnKnotPressed();
	}
	else if (bLineUp && bAccept)
	{
		GM->Advance();
	}
	else if (AnyPressed({ EKeys::E, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Bottom }))
	{
		GM->OnInteractPressed();
	}
	if (AnyPressed({ EKeys::G, EKeys::Gamepad_FaceButton_Left })) GM->OnHolyWater();

	FVector2D Axis(0, 0);
	if (AnyDown({ EKeys::W, EKeys::Up })) Axis.Y += 1;
	if (AnyDown({ EKeys::S, EKeys::Down })) Axis.Y -= 1;
	if (AnyDown({ EKeys::D, EKeys::Right })) Axis.X += 1;
	if (AnyDown({ EKeys::A, EKeys::Left })) Axis.X -= 1;
	const FVector2D Stick(GetInputAnalogKeyState(EKeys::Gamepad_LeftX), GetInputAnalogKeyState(EKeys::Gamepad_LeftY));
	if (Stick.Size() > 0.2f) Axis = Stick;
	GM->MoveInput(Axis, AnyDown({ EKeys::LeftShift, EKeys::Gamepad_LeftThumbstick }));
}

// ================================================================ HUD helpers

static const FLinearColor Gold(0.93f, 0.74f, 0.42f);
static const FLinearColor Bone(0.93f, 0.89f, 0.8f);
static const FLinearColor Ink(0.16f, 0.1f, 0.05f);
static const FLinearColor Dim(0.62f, 0.58f, 0.52f);

void AVigilHUD::Text(const FString& S, float X, float Y, const FLinearColor& C, float Size, bool bCenter, bool bSerif, bool bBold, float Shadow)
{
	if (S.IsEmpty() || C.A <= 0.01f) return;
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	UFont* F = (bSerif && GM) ? GM->SerifFont(bBold) : nullptr;
	if (!F) F = Roboto;
	const FSlateFontInfo Info(F, FMath::Max(6, FMath::RoundToInt(Size * U)));
	const FText Txt = FText::FromString(S);
	if (bCenter && FSlateApplication::IsInitialized())
	{
		X -= FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Info).X * 0.5f;
	}
	if (Shadow > 0.f)
	{
		FCanvasTextItem Sh(FVector2D(X + 2.f * U, Y + 2.f * U), Txt, Info, FLinearColor(0, 0, 0, C.A * Shadow));
		Canvas->DrawItem(Sh);
	}
	FCanvasTextItem It(FVector2D(X, Y), Txt, Info, C);
	Canvas->DrawItem(It);
}

float AVigilHUD::TextWidth(const FString& S, float Size, bool bSerif, bool bBold)
{
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	UFont* F = (bSerif && GM) ? GM->SerifFont(bBold) : nullptr;
	if (!F) F = Roboto;
	if (!FSlateApplication::IsInitialized()) return S.Len() * Size * 0.5f * U;
	const FSlateFontInfo Info(F, FMath::Max(6, FMath::RoundToInt(Size * U)));
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Info).X;
}

TArray<FString> AVigilHUD::Wrap(const FString& S, float Size, float MaxW, bool bSerif)
{
	TArray<FString> Out;
	TArray<FString> Paras;
	S.ParseIntoArray(Paras, TEXT("\n"), false);
	for (const FString& P : Paras)
	{
		TArray<FString> Words;
		P.ParseIntoArray(Words, TEXT(" "), true);
		FString Line;
		for (const FString& Wd : Words)
		{
			const FString Try = Line.IsEmpty() ? Wd : Line + TEXT(" ") + Wd;
			if (TextWidth(Try, Size, bSerif) > MaxW && !Line.IsEmpty())
			{
				Out.Add(Line);
				Line = Wd;
			}
			else Line = Try;
		}
		Out.Add(Line);
	}
	return Out;
}

void AVigilHUD::Image(UTexture2D* T, float X, float Y, float Wd, float Ht, const FLinearColor& Tint, EBlendMode Blend)
{
	if (!T) return;
	FCanvasTileItem It(FVector2D(X, Y), T->GetResource(), FVector2D(Wd, Ht), Tint);
	It.BlendMode = FCanvas::BlendToSimpleElementBlend(Blend);
	Canvas->DrawItem(It);
}

void AVigilHUD::Frame(float X, float Y, float Wd, float Ht, float A)
{
	DrawRect(FLinearColor(0.02f, 0.015f, 0.012f, 0.82f * A), X, Y, Wd, Ht);
	const FLinearColor L(Gold.R, Gold.G, Gold.B, 0.55f * A);
	DrawRect(L, X, Y, Wd, 1.5f * U);
	DrawRect(L, X, Y + Ht - 1.5f * U, Wd, 1.5f * U);
	DrawRect(L, X, Y, 1.5f * U, Ht);
	DrawRect(L, X + Wd - 1.5f * U, Y, 1.5f * U, Ht);
}

FString AVigilHUD::Coptic(const FString& S, float Reveal, float T) const
{
	// Alexios does not speak Coptic yet: the subtitle flickers, half legible
	FString Out = S;
	const int32 Seed = FMath::FloorToInt(T * 9.f);
	for (int32 i = 0; i < Out.Len(); ++i)
	{
		const TCHAR C = Out[i];
		if (!FChar::IsAlpha(C)) continue;
		const uint32 Hh = (uint32)(i * 2654435761u) ^ (uint32)(Seed * 40503u);
		if ((Hh % 100) < 38) Out[i] = TCHAR('a' + (Hh / 100) % 26);
	}
	return Out;
}

// ================================================================ HUD

void AVigilHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	AVigilGameMode* GM = AVigilGameMode::Get(this);
	if (!GM) return;
	W = Canvas->ClipX; H = Canvas->ClipY;
	U = H / 1080.f;
	Now = GetWorld()->GetRealTimeSeconds();

	if (GM->Screen == EVScreen::Title) { DrawTitle(GM); return; }

	DrawComposure(GM);
	if (GM->Screen == EVScreen::Playing || GM->Screen == EVScreen::Paused)
	{
		DrawWorldPrompt(GM);
		// place and objective, top left
		if (!GM->PlaceText.IsEmpty() && GM->ChapterTime < 9.f)
		{
			const float A = FMath::Clamp(GM->ChapterTime * 0.8f, 0.f, 1.f) * FMath::Clamp((9.f - GM->ChapterTime) * 0.6f, 0.f, 1.f);
			Text(GM->PlaceText, 60 * U, 50 * U, FLinearColor(Bone.R, Bone.G, Bone.B, A * 0.9f), 26, false, true);
		}
		if (!GM->ObjectiveText.IsEmpty() && !GM->Locked())
		{
			const float Pulse = GM->ObjectiveFlash > 0.f ? 0.5f + 0.5f * FMath::Sin(Now * 8.f) : 0.f;
			const FLinearColor C = FMath::Lerp(FLinearColor(Bone.R, Bone.G, Bone.B, 0.75f), Gold, Pulse);
			DrawRect(FLinearColor(Gold.R, Gold.G, Gold.B, 0.7f), 60 * U, 102 * U, 8 * U, 8 * U);
			Text(GM->ObjectiveText, 78 * U, 92 * U, C, 22, false, true);
		}
		// holy water and items, bottom left
		if (GM->bHasFlask && !GM->Locked())
		{
			Text(TEXT("Holy Water"), 60 * U, H - 96 * U, FLinearColor(Bone.R, Bone.G, Bone.B, 0.7f), 18, false, true);
			for (int32 i = 0; i < 3; ++i)
			{
				const bool bFull = i < GM->HolyWater;
				DrawRect(bFull ? FLinearColor(0.55f, 0.72f, 0.95f, 0.85f) : FLinearColor(0.3f, 0.3f, 0.3f, 0.45f), 62 * U + i * 22 * U, H - 66 * U, 14 * U, 14 * U);
			}
			Text(TEXT("[G]"), 140 * U, H - 72 * U, FLinearColor(Dim.R, Dim.G, Dim.B, 0.6f), 16, false, false);
		}
		// rite checklist
		if (GM->Rite.bActive)
		{
			static const TCHAR* Steps[] = { TEXT("Remove the defiled cloth and burn it in the brazier"), TEXT("Turn the icons to face the nave: Christ, the Theotokos, the Archangel, the Forerunner"),
				TEXT("Fill and light the lamp before the Theotokos"), TEXT("Read Psalm 50 and sprinkle the four walls"), TEXT("Cense the altar and the walls"),
				TEXT("Read the Patriarch's Letter of Blessing at the altar"), TEXT("Carry the semantron to its frame by the door and strike it") };
			const float X0 = W - 640 * U, Y0 = 60 * U;
			Frame(X0 - 20 * U, Y0 - 16 * U, 600 * U, 7 * 46 * U + 54 * U, 0.8f);
			Text(TEXT("The Cleansing of the Church"), X0, Y0 - 6 * U, Gold, 22, false, true, true);
			for (int32 i = 0; i < 7; ++i)
			{
				const bool bDone = GM->Rite.Step > i + 1;
				const bool bNowStep = GM->Rite.Step == i + 1;
				FLinearColor C = bDone ? FLinearColor(0.55f, 0.52f, 0.46f, 0.7f) : (bNowStep ? Bone : FLinearColor(0.6f, 0.57f, 0.52f, 0.45f));
				FString S = FString::Printf(TEXT("%d.  %s"), i + 1, Steps[i]);
				if (bNowStep && i == 1) S += FString::Printf(TEXT("  (%d/4)"), GM->Rite.IconsTurned);
				if (bNowStep && (i == 3 || i == 4)) S += FString::Printf(TEXT("  (%d/%d)"), GM->Rite.Count, i == 4 ? 5 : 4);
				if (bNowStep && i == 6 && GM->Rite.Strikes > 0) S += FString::Printf(TEXT("  (%d/12)"), GM->Rite.Strikes);
				const TArray<FString> Lines = Wrap(S, 17, 560 * U, true);
				float Yy = Y0 + 34 * U + i * 46 * U;
				for (int32 k = 0; k < Lines.Num() && k < 2; ++k) Text(Lines[k], X0, Yy + k * 19 * U, C, 17, false, true);
				if (bDone) DrawRect(FLinearColor(0.55f, 0.52f, 0.46f, 0.5f), X0, Yy + 11 * U, FMath::Min(TextWidth(Lines[0], 17), 560 * U), 1.2f * U);
			}
		}
	}

	// prayer rope and cross rings, bottom centre
	{
		const float Cx = W * 0.5f, Cy = H - 250 * U;
		if (GM->bPraying)
		{
			const int32 N = 33;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 2 * PI * i / N - PI / 2;
				const float R = 70 * U;
				const bool bLit = i < (GM->KnotsTotal % N);
				DrawRect(bLit ? FLinearColor(0.9f, 0.7f, 0.4f, 0.9f) : FLinearColor(0.25f, 0.22f, 0.2f, 0.8f), Cx + FMath::Cos(A) * R - 4 * U, Cy + FMath::Sin(A) * R - 4 * U, 8 * U, 8 * U);
			}
			const float Ph = GM->KnotPhase;
			const float Rr = FMath::Lerp(150.f, 70.f, FMath::Clamp(Ph, 0.f, 1.f)) * U;
			const bool bWindow = Ph > 0.82f;
			Image(GM->Tex(TEXT("T_RingSoft")), Cx - Rr * 1.2f, Cy - Rr * 1.2f, Rr * 2.4f, Rr * 2.4f, bWindow ? FLinearColor(1, 0.85f, 0.5f, 1) : FLinearColor(0.7f, 0.6f, 0.5f, 0.6f), BLEND_Translucent);
			if (GM->KnotFlash > 0.f) Image(GM->Tex(TEXT("T_Glow")), Cx - 90 * U, Cy - 90 * U, 180 * U, 180 * U, FLinearColor(1, 0.8f, 0.5f, GM->KnotFlash), BLEND_Translucent);
			Text(TEXT("[Space] as the ring meets the knots"), Cx, Cy + 100 * U, FLinearColor(Bone.R, Bone.G, Bone.B, 0.6f), 17, true, true);
		}
		else if (GM->bCrossUp)
		{
			const int32 N = 48;
			const int32 Lit = FMath::CeilToInt(GM->CrossRing * N);
			for (int32 i = 0; i < N; ++i)
			{
				if (i >= Lit) break;
				const float A = 2 * PI * i / N - PI / 2;
				const float R = 80 * U;
				const float Fl = 0.75f + 0.25f * FMath::Sin(Now * 20.f + i);
				DrawRect(FLinearColor(1.f, 0.62f * Fl, 0.25f, 0.9f), Cx + FMath::Cos(A) * R - 3 * U, Cy + FMath::Sin(A) * R - 6 * U, 6 * U, 12 * U * Fl);
			}
			if (GM->bCrossPressed) Text(TEXT("It presses forward. The cross holds it."), Cx, Cy + 100 * U, FLinearColor(Bone.R, Bone.G, Bone.B, 0.7f), 18, true, true);
		}
	}

	// fade first, so dialogue, cards and hints can be read over black
	if (GM->FadeAlpha > 0.001f) DrawRect(FLinearColor(0, 0, 0, GM->FadeAlpha), 0, 0, W, H);
	if (!GM->PlaceText.IsEmpty() && GM->ChapterTime < 9.f && GM->FadeAlpha > 0.5f)
	{
		const float A = FMath::Clamp(GM->ChapterTime * 0.8f, 0.f, 1.f) * FMath::Clamp((9.f - GM->ChapterTime) * 0.6f, 0.f, 1.f);
		Text(GM->PlaceText, 60 * U, 50 * U, FLinearColor(Bone.R, Bone.G, Bone.B, A * 0.9f), 26, false, true);
	}

	DrawDialogue(GM);

	// tutorial hint
	if (GM->HintTime > 0.f && !GM->HintText.IsEmpty())
	{
		const float A = FMath::Clamp(GM->HintTime * 2.f, 0.f, 1.f);
		const float Wd = TextWidth(GM->HintText, 21) + 60 * U;
		const float Y = GM->Queue.Num() > 0 && GM->Queue[0].Kind == EVStep::Line ? H - 420 * U : H - 140 * U;
		Frame(W * 0.5f - Wd * 0.5f, Y, Wd, 48 * U, A);
		Text(GM->HintText, W * 0.5f, Y + 12 * U, FLinearColor(Bone.R, Bone.G, Bone.B, A), 21, true, true);
	}

	// title cards
	if (GM->CardTime > 0.f)
	{
		const float T = GM->CardTotal - GM->CardTime;
		const float A = FMath::Clamp(T * 1.2f, 0.f, 1.f) * FMath::Clamp(GM->CardTime * 1.2f, 0.f, 1.f);
		DrawRect(FLinearColor(0, 0, 0, 0.55f * A), 0, H * 0.36f, W, H * 0.28f);
		Text(GM->CardTitle, W * 0.5f, H * 0.42f, FLinearColor(Gold.R, Gold.G, Gold.B, A), GM->CardSub.IsEmpty() ? 64 : 58, true, true, true);
		Text(GM->CardSub, W * 0.5f, H * 0.53f, FLinearColor(Bone.R, Bone.G, Bone.B, A * 0.9f), 28, true, true);
	}

	// flash
	if (GM->FlashTime > 0.f)
	{
		FLinearColor C = GM->FlashColor;
		C.A *= GM->FlashTime / FMath::Max(0.01f, GM->FlashTotal);
		DrawRect(C, 0, 0, W, H);
	}

	if (GM->Screen == EVScreen::Journal) DrawJournal(GM);
	if (GM->Screen == EVScreen::Paused || GM->Screen == EVScreen::End)
	{
		DrawRect(FLinearColor(0, 0, 0, GM->Screen == EVScreen::End ? 0.9f : 0.6f), 0, 0, W, H);
		const bool bEnd = GM->Screen == EVScreen::End;
		Text(bEnd ? TEXT("ORTHODOX VIGIL") : TEXT("Paused"), W * 0.5f, H * 0.3f, Gold, bEnd ? 76 : 54, true, true, true);
		if (bEnd)
		{
			Text(TEXT("Here the vertical slice ends. The night goes on in Act II: The Pattern."), W * 0.5f, H * 0.42f, Bone, 26, true, true);
			Text(TEXT("PixelForge Studios"), W * 0.5f, H * 0.47f, Dim, 20, true, true);
		}
		const TCHAR* Opts[] = { bEnd ? TEXT("Return to the title") : TEXT("Resume"), bEnd ? TEXT("Quit") : TEXT("Journal"), bEnd ? TEXT("") : TEXT("Return to the title") };
		const int32 N = bEnd ? 2 : 3;
		for (int32 i = 0; i < N; ++i)
		{
			const bool bSel = GM->MenuSel == i;
			Text(Opts[i], W * 0.5f, H * 0.58f + i * 52 * U, bSel ? Gold : FLinearColor(Bone.R, Bone.G, Bone.B, 0.6f), 32, true, true, bSel);
		}
	}
}

void AVigilHUD::DrawTitle(AVigilGameMode* GM)
{
	DrawRect(FLinearColor(0.008f, 0.006f, 0.005f, 1), 0, 0, W, H);
	UTexture2D* Cover = GM->Tex(TEXT("T_Cover"));
	const float Ch = H, Cw = H * 1024.f / 1536.f;
	Image(Cover, W * 0.5f - Cw * 0.5f, 0, Cw, Ch, FLinearColor::White, BLEND_Opaque);
	// soften the edges into the black
	for (int32 i = 0; i < 24; ++i)
	{
		const float A = 1.f - i / 24.f;
		DrawRect(FLinearColor(0.008f, 0.006f, 0.005f, A * 0.08f), W * 0.5f - Cw * 0.5f + i * 6 * U, 0, 6 * U, H);
		DrawRect(FLinearColor(0.008f, 0.006f, 0.005f, A * 0.08f), W * 0.5f + Cw * 0.5f - (i + 1) * 6 * U, 0, 6 * U, H);
	}
	DrawRect(FLinearColor(0, 0, 0, 0.55f), W * 0.5f - 260 * U, H * 0.72f, 520 * U, 200 * U);
	const TCHAR* Opts[] = { TEXT("Begin the vigil"), TEXT("Quit") };
	for (int32 i = 0; i < 2; ++i)
	{
		const bool bSel = GM->MenuSel == i;
		Text(Opts[i], W * 0.5f, H * 0.75f + i * 56 * U, bSel ? Gold : FLinearColor(Bone.R, Bone.G, Bone.B, 0.6f), 34, true, true, bSel);
	}
	Text(TEXT("Vertical slice: the first night in Tarsenis"), W * 0.5f, H * 0.75f + 128 * U, FLinearColor(Dim.R, Dim.G, Dim.B, 0.8f), 18, true, true);
	Text(TEXT("WASD move    E use    Q hold the prayer rope    F raise the cross    G holy water    J journal"), W * 0.5f, H - 40 * U, FLinearColor(Dim.R, Dim.G, Dim.B, 0.6f), 15, true, false);
	if (GM->FadeAlpha > 0.001f) DrawRect(FLinearColor(0, 0, 0, GM->FadeAlpha), 0, 0, W, H);
}

void AVigilHUD::DrawDialogue(AVigilGameMode* GM)
{
	// subtitle line (non-blocking speech)
	if (GM->SubTime > 0.f && !GM->SubText.IsEmpty() && !(GM->Queue.Num() > 0 && GM->Queue[0].Kind == EVStep::Line))
	{
		const float A = FMath::Clamp(GM->SubTime * 2.f, 0.f, 1.f);
		const FString Body = GM->bSubCoptic ? Coptic(GM->SubText, 1.f, Now) : GM->SubText;
		const TArray<FString> L = Wrap(Body, 25, 1200 * U, true);
		float Y = H - 200 * U - L.Num() * 32 * U;
		if (!GM->SubSpeaker.IsEmpty()) { Text(GM->SubSpeaker, W * 0.5f, Y - 34 * U, FLinearColor(Gold.R, Gold.G, Gold.B, A * 0.9f), 19, true, true, true); }
		for (const FString& S : L) { Text(S, W * 0.5f, Y, FLinearColor(Bone.R, Bone.G, Bone.B, A), 25, true, true); Y += 32 * U; }
	}
	if (GM->Queue.Num() == 0) return;
	const FVStep& S = GM->Queue[0];
	if (S.Kind == EVStep::Line)
	{
		const float Bw = 1380 * U, Bh = 220 * U;
		const float Bx = W * 0.5f - Bw * 0.5f, By = H - Bh - 50 * U;
		Frame(Bx, By, Bw, Bh, 1.f);
		float Tx = Bx + 40 * U;
		UTexture2D* P = S.Portrait.IsEmpty() ? nullptr : GM->Tex(S.Portrait);
		if (P)
		{
			const float Ps = 180 * U;
			DrawRect(FLinearColor(Gold.R, Gold.G, Gold.B, 0.6f), Bx + 20 * U - 2 * U, By + 20 * U - 2 * U, Ps + 4 * U, Ps + 4 * U);
			Image(P, Bx + 20 * U, By + 20 * U, Ps, Ps, FLinearColor::White, BLEND_Opaque);
			Tx = Bx + Ps + 50 * U;
		}
		Text(S.Speaker, Tx, By + 22 * U, Gold, 22, false, true, true);
		const int32 Chars = FMath::Clamp(FMath::FloorToInt(GM->StepAge * 55.f), 0, S.Text.Len());
		FString Shown = S.Text.Left(Chars);
		if (S.bCoptic) Shown = Coptic(Shown, 1.f, Now);
		const TArray<FString> L = Wrap(Shown, 27, Bx + Bw - Tx - 40 * U, true);
		float Y = By + 62 * U;
		for (const FString& Ln : L) { Text(Ln, Tx, Y, Bone, 27, false, true); Y += 34 * U; }
		if (Chars >= S.Text.Len()) Text(TEXT("E"), Bx + Bw - 40 * U, By + Bh - 40 * U, FLinearColor(Gold.R, Gold.G, Gold.B, 0.5f + 0.4f * FMath::Sin(Now * 4.f)), 18, false, false);
	}
	else if (S.Kind == EVStep::Choice)
	{
		TArray<int32> Vis;
		for (int32 i = 0; i < S.Opts.Num(); ++i) if (S.Opts[i].OnceFlag.IsNone() || !GM->Has(S.Opts[i].OnceFlag)) Vis.Add(i);
		const float Bw = 900 * U;
		const float Bh = (Vis.Num() * 50 + 40) * U;
		const float Bx = W * 0.5f - Bw * 0.5f, By = H - Bh - 60 * U;
		Frame(Bx, By, Bw, Bh, 1.f);
		for (int32 k = 0; k < Vis.Num(); ++k)
		{
			const bool bSel = GM->ChoiceSel == k;
			const FString T = FString::Printf(TEXT("%d.  %s"), k + 1, *S.Opts[Vis[k]].Text);
			if (bSel) DrawRect(FLinearColor(Gold.R, Gold.G, Gold.B, 0.12f), Bx + 10 * U, By + 20 * U + k * 50 * U, Bw - 20 * U, 46 * U);
			Text(T, Bx + 40 * U, By + 28 * U + k * 50 * U, bSel ? Gold : FLinearColor(Bone.R, Bone.G, Bone.B, 0.75f), 24, false, true, bSel);
		}
	}
	else if (S.Kind == EVStep::Doc)
	{
		const float Pw = 900 * U, Ph = 900 * U;
		const float Px = W * 0.5f - Pw * 0.5f, Py = H * 0.5f - Ph * 0.5f;
		DrawRect(FLinearColor(0, 0, 0, 0.6f), 0, 0, W, H);
		Image(GM->Tex(TEXT("T_Parchment")), Px, Py, Pw, Ph, FLinearColor::White, BLEND_Opaque);
		Text(S.Text, W * 0.5f, Py + 60 * U, Ink, 32, true, true, true, 0.f);
		const int32 Pg = FMath::Clamp(GM->DocPage, 0, S.Pages.Num() - 1);
		const TArray<FString> L = Wrap(S.Pages.IsValidIndex(Pg) ? S.Pages[Pg] : FString(), 27, Pw - 160 * U, true);
		float Y = Py + 140 * U;
		for (const FString& Ln : L) { Text(Ln, Px + 80 * U, Y, Ink, 27, false, true, false, 0.f); Y += 38 * U; }
		Text(FString::Printf(TEXT("%d / %d      E  %s"), Pg + 1, S.Pages.Num(), Pg + 1 < S.Pages.Num() ? TEXT("turn the page") : TEXT("close")), W * 0.5f, Py + Ph - 70 * U, FLinearColor(Ink.R, Ink.G, Ink.B, 0.7f), 18, true, true, false, 0.f);
	}
}

void AVigilHUD::DrawWorldPrompt(AVigilGameMode* GM)
{
	if (GM->Locked() || !GM->Priest || !PlayerOwner) return;
	AVigilInteract* Best = nullptr;
	float BestD = 1e9f;
	const FVector P = GM->Priest->GetActorLocation();
	for (AVigilInteract* I : GM->Usables)
	{
		if (!I || !I->bEnabled || I->IsHidden()) continue;
		const float D = FVector::Dist2D(I->GetActorLocation(), P);
		const float Score = I->bGlint ? D * 0.35f : D;
		if (D < I->Radius && Score < BestD && FMath::Abs(I->GetActorLocation().Z - P.Z) < 400.f) { Best = I; BestD = Score; }
		if (I->bGlint && D < 2500.f && FMath::Abs(I->GetActorLocation().Z - P.Z) < 400.f)
		{
			FVector2D S;
			if (PlayerOwner->ProjectWorldLocationToScreen(I->GetActorLocation() + FVector(0, 0, I->Height), S))
			{
				const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 3.f + I->GetActorLocation().X * 0.01f);
				Image(GM->Tex(TEXT("T_Glow")), S.X - 16 * U, S.Y - 16 * U, 32 * U, 32 * U, FLinearColor(1, 0.85f, 0.6f, 0.35f + 0.3f * Pulse), BLEND_Translucent);
			}
		}
	}
	if (!Best) return;
	FVector2D S;
	if (!PlayerOwner->ProjectWorldLocationToScreen(Best->GetActorLocation() + FVector(0, 0, Best->Height), S)) return;
	const FString T = FString::Printf(TEXT("E   %s"), *Best->Prompt);
	const float Wd = TextWidth(T, 20) + 36 * U;
	Frame(S.X - Wd * 0.5f, S.Y - 20 * U, Wd, 40 * U, 0.9f);
	Text(T, S.X, S.Y - 10 * U, Bone, 20, true, true);
}

void AVigilHUD::DrawComposure(AVigilGameMode* GM)
{
	// Composure is never a number on screen: the edges close in, the rope trembles
	const float Lost = FMath::Clamp(1.f - GM->Composure / 100.f, 0.f, 1.f);
	const float A = 0.28f + 0.7f * FMath::Pow(Lost, 1.1f);
	Image(GM->Tex(TEXT("T_Vignette")), 0, 0, W, H, FLinearColor(0, 0, 0, A), BLEND_Translucent);
	if (Lost > 0.35f) Image(GM->Tex(TEXT("T_Vignette")), 0, 0, W, H, FLinearColor(0.05f, 0, 0, (Lost - 0.35f) * 0.8f), BLEND_Translucent);
	if (GM->Screen != EVScreen::Playing || GM->Locked() || !GM->bHasRope) return;
	// the prayer rope in the corner, trembling with the hand that holds it
	const float Tr = Lost * Lost * 7.f * U;
	const float Cx = W - 110 * U + FMath::Sin(Now * 31.f) * Tr, Cy = H - 110 * U + FMath::Sin(Now * 27.f + 1.f) * Tr;
	for (int32 i = 0; i < 20; ++i)
	{
		const float An = 2 * PI * i / 20;
		DrawRect(FLinearColor(0.75f, 0.62f, 0.45f, 0.55f), Cx + FMath::Cos(An) * 34 * U - 3 * U, Cy + FMath::Sin(An) * 34 * U - 3 * U, 6 * U, 6 * U);
	}
	DrawRect(FLinearColor(0.75f, 0.62f, 0.45f, 0.55f), Cx - 2 * U, Cy + 34 * U, 4 * U, 22 * U);
	Text(TEXT("Q"), Cx, Cy - 12 * U, FLinearColor(Dim.R, Dim.G, Dim.B, 0.6f), 16, true, false);
}

void AVigilHUD::DrawJournal(AVigilGameMode* GM)
{
	DrawRect(FLinearColor(0, 0, 0, 0.75f), 0, 0, W, H);
	const float Pw = 1500 * U, Ph = 900 * U, Px = W * 0.5f - Pw * 0.5f, Py = H * 0.5f - Ph * 0.5f;
	Image(GM->Tex(TEXT("T_Parchment")), Px, Py, Pw, Ph, FLinearColor::White, BLEND_Opaque);
	Text(TEXT("Journal"), Px + 80 * U, Py + 50 * U, Ink, 40, false, true, true, 0.f);
	Text(TEXT("Carried"), Px + Pw - 420 * U, Py + 50 * U, Ink, 32, false, true, true, 0.f);
	// entries, newest first, scrollable
	float Y = Py + 120 * U - GM->JournalScroll;
	const float Top = Py + 110 * U, Bottom = Py + Ph - 60 * U;
	for (int32 i = GM->JournalEntries.Num() - 1; i >= 0; --i)
	{
		const FVJournal& E = GM->JournalEntries[i];
		if (Y > Top - 10 && Y < Bottom) Text(E.Title, Px + 80 * U, Y, FLinearColor(0.35f, 0.08f, 0.05f, 1), 24, false, true, true, 0.f);
		Y += 34 * U;
		for (const FString& L : Wrap(E.Text, 21, 880 * U, true))
		{
			if (Y > Top && Y < Bottom) Text(L, Px + 80 * U, Y, Ink, 21, false, true, false, 0.f);
			Y += 28 * U;
		}
		Y += 20 * U;
	}
	GM->JournalScroll = FMath::Min(GM->JournalScroll, FMath::Max(0.f, Y + GM->JournalScroll - Bottom));
	float Yi = Py + 120 * U;
	for (const FName& It : GM->Items)
	{
		const FString* N = GM->ItemNames.Find(It);
		Text(N ? *N : It.ToString(), Px + Pw - 420 * U, Yi, Ink, 22, false, true, false, 0.f);
		Yi += 34 * U;
	}
	if (GM->bHasFlask) Text(FString::Printf(TEXT("Holy Water: %d of 3"), GM->HolyWater), Px + Pw - 420 * U, Yi + 10 * U, FLinearColor(0.1f, 0.15f, 0.3f, 1), 22, false, true, false, 0.f);
	Text(TEXT("J  close      W / S  scroll"), W * 0.5f, Py + Ph - 46 * U, FLinearColor(Ink.R, Ink.G, Ink.B, 0.6f), 17, true, true, false, 0.f);
}
