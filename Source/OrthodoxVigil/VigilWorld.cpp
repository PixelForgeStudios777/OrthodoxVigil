// Tarsenis, built from code. Distances below are metres in Unreal axes: +X east, +Y south.
// The canal runs north-south on the east. The church of the Archangel stands west of the square,
// the burial ground and bone house behind it, the tombs on the ridge far west, the convent across the canal.
#include "VigilGameMode.h"
#include "VigilActors.h"
#include "VigilPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

static FVector M(float X, float Y, float Z = 0.f) { return FVector(X * 100.f, Y * 100.f, Z * 100.f); }
static float YawTo(FVector From, FVector To) { return (To - From).Rotation().Yaw; }

// ---------------------------------------------------------------- helpers

float AVigilGameMode::GroundZ(float X, float Y, float Fallback) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VigilGround), true);
	if (Priest) Q.AddIgnoredActor(Priest);
	for (const auto& KV : Figures) if (KV.Value) Q.AddIgnoredActor(KV.Value);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(X, Y, 3000.f), FVector(X, Y, -1500.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		return Hit.ImpactPoint.Z;
	return Fallback;
}

FVector AVigilGameMode::Ground(float Xm, float Ym, float Lift) const
{
	return FVector(Xm * 100.f, Ym * 100.f, GroundZ(Xm * 100.f, Ym * 100.f) + Lift * 100.f);
}

AVigilProp* AVigilGameMode::Prop(const FString& MeshName, FVector Loc, float Yaw, float Scale, bool bCollide, bool bStatic)
{
	const FTransform T(FRotator(0, Yaw, 0), Loc, FVector(Scale));
	AVigilProp* P = GetWorld()->SpawnActorDeferred<AVigilProp>(AVigilProp::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!P) return nullptr;
	P->Root->SetMobility(bStatic ? EComponentMobility::Static : EComponentMobility::Movable);
	P->FinishSpawning(T);
	if (!MeshName.IsEmpty()) P->AddMesh(Mesh(MeshName), FVector::ZeroVector, FRotator::ZeroRotator, FVector(1), bCollide);
	return P;
}

AVigilInteract* AVigilGameMode::Usable(const FString& Id, const FString& Prompt, FVector Loc, const FString& MeshName, float Yaw, float Scale, float Radius)
{
	const FTransform T(FRotator(0, Yaw, 0), Loc, FVector(Scale));
	AVigilInteract* I = GetWorld()->SpawnActorDeferred<AVigilInteract>(AVigilInteract::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!I) return nullptr;
	I->Root->SetMobility(EComponentMobility::Movable);
	I->FinishSpawning(T);
	I->Id = Id;
	I->Prompt = Prompt;
	I->Radius = Radius;
	if (!MeshName.IsEmpty()) I->AddMesh(Mesh(MeshName));
	Usables.Add(I);
	return I;
}

AVigilFigure* AVigilGameMode::Figure(const FString& Name, const FString& Garment, FVector Loc, float Yaw, bool bShowBody, float Scale)
{
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVigilFigure* F = GetWorld()->SpawnActor<AVigilFigure>(AVigilFigure::StaticClass(), Loc + FVector(0, 0, 92.f * Scale), FRotator(0, Yaw, 0), P);
	if (!F) return nullptr;
	F->SpawnDefaultController();
	F->SetActorScale3D(FVector(Scale));
	F->Setup(Garment, bShowBody);
	F->FaceYaw(Yaw, true);
	Figures.Add(Name, F);
	return F;
}

UPointLightComponent* AVigilGameMode::Lamp(AActor* LightOwner, FVector Loc, float Candela, float Radius, FLinearColor Color, bool bShadows)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(LightOwner);
	L->SetMobility(EComponentMobility::Movable);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Candela);
	L->SetAttenuationRadius(Radius);
	L->SetLightColor(Color);
	L->SetCastShadows(bShadows);
	L->SetSourceRadius(2.f);
	L->SetUseInverseSquaredFalloff(true);
	if (LightOwner->GetRootComponent()) L->SetupAttachment(LightOwner->GetRootComponent());
	L->RegisterComponent();
	L->SetWorldLocation(Loc);
	LightOwner->AddInstanceComponent(L);
	return L;
}

AVigilZone* AVigilGameMode::Zone(const FString& Id, FVector Center, FVector Half, float Yaw)
{
	AVigilZone* Z = GetWorld()->SpawnActor<AVigilZone>(AVigilZone::StaticClass(), Center, FRotator::ZeroRotator);
	Z->Id = Id;
	Z->SetBox(Center, Half, Yaw);
	Zones.Add(Z);
	return Z;
}

void AVigilGameMode::Blocker(FVector Center, FVector Half, float Yaw, const FString& Tag)
{
	AVigilProp* P = Prop(FString(), Center, Yaw, 1.f, false, false);
	UBoxComponent* B = NewObject<UBoxComponent>(P);
	B->SetBoxExtent(Half);
	B->SetupAttachment(P->Root);
	B->SetCollisionProfileName(TEXT("BlockAll"));
	B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	B->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	B->SetCollisionObjectType(ECC_WorldDynamic);   // ground traces look for WorldStatic only
	B->RegisterComponent();
	P->AddInstanceComponent(B);
	if (!Tag.IsEmpty()) Blockers.Add(Tag, B);
}

void AVigilGameMode::SetBlocker(const FString& Tag, bool bOn)
{
	if (TObjectPtr<UPrimitiveComponent>* B = Blockers.Find(Tag))
		if (*B) (*B)->SetCollisionEnabled(bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AVigilGameMode::SetIconFacing(int32 Icon, float Turn)
{
	if (!Icons.IsValidIndex(Icon) || !Icons[Icon]) return;
	FRotator R = Icons[Icon]->GetActorRotation();
	R.Yaw = 180.f * FMath::Clamp(Turn, 0.f, 1.f);
	Icons[Icon]->SetActorRotation(R);
}

void AVigilGameMode::GoTo(FVector WhereCm, float Yaw, EVLight Light, bool bUnder)
{
	if (!Priest) return;
	Priest->Stop();
	Priest->SetActorLocation(WhereCm + FVector(0, 0, 95.f), false, nullptr, ETeleportType::TeleportPhysics);
	Priest->SetActorRotation(FRotator(0, Yaw, 0));
	Priest->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	bUnderground = bUnder;
	SetLighting(Light);
	if (Rig) { Rig->Focus = Priest->GetActorLocation(); Rig->Snap(); }
}

// ---------------------------------------------------------------- lighting

void AVigilGameMode::SetLighting(EVLight P)
{
	LightNow = P;
	if (!Moon) return;
	struct FPreset { float Pitch, Yaw, Lux; FLinearColor Col; float Sky; FLinearColor FogCol; float FogDen; float EV; };
	FPreset S = { -42.f, 60.f, 0.42f, FLinearColor::White, 0.2f, FLinearColor::Black, 0.02f, -3.f };
	switch (P)
	{
	case EVLight::Dusk:   S = { -9.f, 250.f, 1.6f, FLinearColor(0.95f, 0.55f, 0.42f), 0.45f, FLinearColor(0.16f, 0.09f, 0.08f), 0.03f, -1.2f }; break;
	case EVLight::Night:  S = { -42.f, 60.f, 0.42f, FLinearColor(0.52f, 0.64f, 1.0f), 0.22f, FLinearColor(0.012f, 0.018f, 0.032f), 0.028f, -3.0f }; break;
	case EVLight::Day:    S = { -48.f, 140.f, 5.0f, FLinearColor(1.0f, 0.93f, 0.82f), 1.0f, FLinearColor(0.42f, 0.4f, 0.36f), 0.012f, 1.4f }; break;
	case EVLight::Sunset: S = { -7.f, 255.f, 3.2f, FLinearColor(1.0f, 0.5f, 0.26f), 0.4f, FLinearColor(0.28f, 0.14f, 0.07f), 0.025f, -0.4f }; break;
	case EVLight::Dawn:   S = { -16.f, 95.f, 3.2f, FLinearColor(1.0f, 0.8f, 0.64f), 0.9f, FLinearColor(0.45f, 0.38f, 0.32f), 0.02f, -0.6f }; break;
	case EVLight::Under:  S = { -60.f, 60.f, 0.0f, FLinearColor::White, 0.0f, FLinearColor::Black, 0.0f, -3.4f }; break;
	case EVLight::Vision: S = { -60.f, 60.f, 0.0f, FLinearColor::White, 0.0f, FLinearColor::Black, 0.0f, -1.6f }; break;
	}
	Moon->SetWorldRotation(FRotator(S.Pitch, S.Yaw, 0));
	Moon->SetIntensity(S.Lux);
	Moon->SetLightColor(S.Col);
	Moon->SetVisibility(S.Lux > 0.f);
	if (Sky) { Sky->SetIntensity(S.Sky); Sky->SetVisibility(S.Sky > 0.f); Sky->RecaptureSky(); }
	if (Fog)
	{
		Fog->SetFogInscatteringColor(S.FogCol);
		Fog->SetFogDensity(S.FogDen);
		Fog->SetVisibility(S.FogDen > 0.f);
	}
	if (PPV)
	{
		FPostProcessSettings& PS = PPV->Settings;
		PS.bOverride_AutoExposureMethod = true;
		PS.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		PS.bOverride_AutoExposureMinBrightness = PS.bOverride_AutoExposureMaxBrightness = true;
		PS.AutoExposureMinBrightness = S.EV;
		PS.AutoExposureMaxBrightness = S.EV;
		PS.bOverride_AutoExposureBias = true;
		PS.AutoExposureBias = 0.f;
		PS.bOverride_AutoExposureSpeedUp = PS.bOverride_AutoExposureSpeedDown = true;
		PS.AutoExposureSpeedUp = PS.AutoExposureSpeedDown = 20.f;
	}
}

// ---------------------------------------------------------------- the world

void AVigilGameMode::BuildWorld()
{
	// sky, moon, fog, exposure
	AVigilProp* Env = Prop(FString(), FVector::ZeroVector, 0.f, 1.f, false, false);
	Moon = NewObject<UDirectionalLightComponent>(Env);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetupAttachment(Env->Root);
	Moon->SetCastShadows(true);
	Moon->SetDynamicShadowDistanceMovableLight(6000.f);
	Moon->RegisterComponent();
	Env->AddInstanceComponent(Moon);
	Sky = NewObject<USkyLightComponent>(Env);
	Sky->SetMobility(EComponentMobility::Movable);
	Sky->SetupAttachment(Env->Root);
	Sky->bRealTimeCapture = false;   // no sky atmosphere here: capture the fogged scene instead
	Sky->bLowerHemisphereIsBlack = true;
	Sky->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	Sky->RegisterComponent();
	Env->AddInstanceComponent(Sky);
	Fog = NewObject<UExponentialHeightFogComponent>(Env);
	Fog->SetupAttachment(Env->Root);
	Fog->SetFogHeightFalloff(0.08f);
	Fog->SetStartDistance(300.f);
	Fog->RegisterComponent();
	Env->AddInstanceComponent(Fog);
	PPV = NewObject<UPostProcessComponent>(Env);
	PPV->SetupAttachment(Env->Root);
	PPV->bUnbound = true;
	PPV->Settings.bOverride_BloomIntensity = true;  PPV->Settings.BloomIntensity = 0.9f;
	PPV->Settings.bOverride_VignetteIntensity = true; PPV->Settings.VignetteIntensity = 0.f;
	PPV->Settings.bOverride_FilmGrainIntensity = true; PPV->Settings.FilmGrainIntensity = 0.12f;
	PPV->Settings.bOverride_ColorSaturation = true; PPV->Settings.ColorSaturation = FVector4(0.92f, 0.92f, 0.92f, 1.f);
	PPV->Settings.bOverride_ColorContrast = true; PPV->Settings.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.f);
	PPV->RegisterComponent();
	Env->AddInstanceComponent(PPV);

	// terrain and canal
	Prop(TEXT("SM_Ground"), FVector::ZeroVector, 0.f, 1.f, true);
	if (AVigilProp* W = Prop(TEXT("SM_Water"), M(35, 0, -1.2f))) W->Meshes[0]->SetCastShadow(false);
	Prop(TEXT("SM_TombRidge"), M(-56, 0, -0.5f), 0.f, 1.f, false);
	{
		AVigilProp* TL = Prop(FString(), M(-50, 0), 0, 1, false, false);
		const float Ys[] = { -27.f, -9.f, 0.f, 9.f, 27.f };
		for (float Y : Ys) Lamp(TL, M(-52.f + (FMath::Abs(Y) * 0.02f), Y, 3.f), 1.2f, 400.f, FLinearColor(1.f, 0.6f, 0.3f), false);
	}

	// ------------------------------------------------ village houses, every door sealed with the closed eye
	struct FHouse { const TCHAR* Mesh; float X, Y; float DoorX; bool bBread; };
	const FHouse Houses[] = {
		{ TEXT("SM_HouseA"), 11.0f, -6.5f, 2.5f, false },   // 0: Thecla's house, by the well
		{ TEXT("SM_HouseB"), 1.5f, -8.0f, 2.2f, true },
		{ TEXT("SM_HouseC"), -1.0f, 6.5f, 2.32f, true },
		{ TEXT("SM_HouseB"), 12.0f, 8.0f, 2.2f, true },
		{ TEXT("SM_HouseA"), 4.5f, 12.5f, 2.5f, false },
		{ TEXT("SM_HouseA"), 18.0f, -1.5f, 2.5f, true },
		{ TEXT("SM_HouseC"), -3.5f, -9.0f, 2.32f, false },
		{ TEXT("SM_HouseA"), -6.0f, 12.5f, 2.5f, true },
		{ TEXT("SM_HouseC"), 19.0f, 10.0f, 2.32f, false },
		{ TEXT("SM_HouseB"), 8.0f, -14.5f, 2.2f, true },
	};
	const FVector Well = M(6, 0);
	for (int32 i = 0; i < UE_ARRAY_COUNT(Houses); ++i)
	{
		const FHouse& Hh = Houses[i];
		const FVector C = M(Hh.X, Hh.Y);
		const float Yaw = YawTo(C, Well);
		const FRotator R(0, Yaw, 0);
		// sink into the higher side of the slope so nothing floats
		float Zmin = 1e9f;
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector Corner = C + R.RotateVector(FVector((k & 1) ? 250.f : -250.f, (k & 2) ? 210.f : -210.f, 0));
			Zmin = FMath::Min(Zmin, GroundZ(Corner.X, Corner.Y));
		}
		const FVector Base(C.X, C.Y, Zmin - 6.f);
		AVigilProp* House = Prop(Hh.Mesh, Base, Yaw, 1.f, true);
		if (i == 0) Named.Add(TEXT("house_thecla"), House);
		const FVector DoorLocal(Hh.DoorX * 100.f, 0, 0);
		const FVector DoorW = Base + R.RotateVector(DoorLocal);
		const float DoorZ = GroundZ(DoorW.X + R.Vector().X * 60.f, DoorW.Y + R.Vector().Y * 60.f, Base.Z);
		AVigilProp* Door = Prop(TEXT("SM_Door"), Base + R.RotateVector(DoorLocal + FVector(6, 47, 4)), Yaw, 1.f, false, false);
		Named.Add(FString::Printf(TEXT("door_%d"), i), Door);
		AVigilProp* Sign = Prop(TEXT("SM_AshSign"), Base + R.RotateVector(DoorLocal + FVector(14, 0, 212)), Yaw, 0.85f, false);
		Sign->Meshes[0]->SetCastShadow(false);
		AVigilInteract* Knock = Usable(FString::Printf(TEXT("door_%d"), i), i == 0 ? TEXT("Knock") : TEXT("Knock"), Base + R.RotateVector(DoorLocal + FVector(70, 0, 0)), FString(), Yaw, 1.f, 150.f);
		Knock->Height = 150.f;
		if (Hh.bBread)
		{
			const FVector BL = Base + R.RotateVector(DoorLocal + FVector(38, -32, 0));
			AVigilInteract* Bread = Usable(FString::Printf(TEXT("bread_%d"), i), TEXT("Look at the bread"), FVector(BL.X, BL.Y, DoorZ + 9.f), TEXT("SM_Bread"), Yaw + 30.f, 1.f, 110.f);
			Bread->Height = 40.f;
		}
	}
	// Pakhom watches from beside his mother's door
	Prop(TEXT("SM_Dovecote"), Ground(15.5f, -11.0f, -0.05f), 20.f, 1.f, true);
	Usable(TEXT("dovecote"), TEXT("Look at the dovecote"), Ground(14.0f, -10.0f), FString(), 0, 1, 160.f)->Height = 220.f;
	Prop(TEXT("SM_Goat"), Ground(13.3f, -12.6f), 35.f, 1.f, false);
	Usable(TEXT("goat"), TEXT("Look at the goat"), Ground(13.3f, -12.0f), FString(), 0, 1, 150.f)->Height = 120.f;
	Prop(TEXT("SM_Laundry"), Ground(5.0f, -11.6f), 90.f, 1.f, false);
	Usable(TEXT("laundry"), TEXT("Look at the laundry"), Ground(5.0f, -10.8f), FString(), 0, 1, 170.f)->Height = 170.f;
	for (const FVector2D& P : { FVector2D(9.5f, -3.0f), FVector2D(-1.5f, -3.2f), FVector2D(14.5f, 3.5f) })
		Prop(TEXT("SM_Pot"), Ground(P.X, P.Y), P.X * 30.f, 0.9f, false);
	Prop(TEXT("SM_Basket"), Ground(12.8f, -4.6f), 0, 1, false);
	Prop(TEXT("SM_Jar"), Ground(13.2f, -4.2f), 0, 1, false);
	Prop(TEXT("SM_Mat"), Ground(16.0f, 1.8f, 0.01f), 70, 1, false);

	// the well
	Prop(TEXT("SM_Well"), FVector(Well.X, Well.Y, GroundZ(Well.X, Well.Y) - 8.f), 0.f, 1.f, true);
	if (AVigilProp* Dark = Prop(TEXT("SM_WellWater"), FVector(Well.X, Well.Y, GroundZ(Well.X, Well.Y) + 12.f), 0, 1, false))
		Dark->Meshes[0]->SetMaterial(0, Mat(TEXT("MI_Dark")));
	Named.Add(TEXT("bucket"), Prop(TEXT("SM_Bucket"), FVector(Well.X + 70.f, Well.Y + 20.f, GroundZ(Well.X, Well.Y) + 76.f), 0, 1, false, false));
	Named.Add(TEXT("rope"), Prop(TEXT("SM_Rope"), FVector(Well.X, Well.Y, GroundZ(Well.X, Well.Y) + 120.f), 0, 1, false, false));
	Usable(TEXT("well"), TEXT("Draw water"), FVector(Well.X, Well.Y, GroundZ(Well.X, Well.Y)), FString(), 0, 1, 190.f)->Height = 120.f;

	// palms, reeds, low walls
	const FVector2D Palms[] = { {20.5f, 5.5f}, {21.0f, -7.0f}, {-9.0f, -10.5f}, {-10.5f, 10.5f}, {8.5f, 16.0f}, {25.0f, -18.0f}, {24.5f, 15.0f},
		{-21.0f, 12.0f}, {-22.5f, -10.0f}, {14.0f, -17.0f}, {2.5f, 17.0f}, {-1.5f, -16.5f}, {26.0f, -4.0f}, {26.5f, 18.5f}, {-30.0f, 3.0f}, {16.5f, 15.5f}, {-15.0f, -14.5f} };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Palms); ++i)
	{
		const TCHAR* Kinds[] = { TEXT("SM_PalmA"), TEXT("SM_PalmB"), TEXT("SM_PalmC") };
		Prop(Kinds[i % 3], Ground(Palms[i].X, Palms[i].Y, -0.1f), i * 47.f, 0.9f + 0.05f * (i % 4), i % 2 == 0);
	}
	for (int32 k = 0; k < 26; ++k)
	{
		const float Y = -32.f + k * 2.6f;
		if (Y > 6.0f && Y < 10.5f) continue;
		if (Y > -15.5f && Y < -11.5f) continue;
		const float X = 27.2f + (k % 3) * 0.9f;
		Prop(TEXT("SM_Reeds"), Ground(X, Y, -0.1f), k * 33.f, 0.8f + 0.1f * (k % 3), false);
	}
	for (int32 k = 0; k < 8; ++k)
		Prop(TEXT("SM_Reeds"), Ground(41.0f + (k % 2), -20.f + k * 5.f, -0.1f), k * 41.f, 0.9f, false);
	struct FWl { float X, Y, Z, W; };
	const FWl Walls[] = { {-33, -6, 90, 0}, {-33, -2, 90, 0}, {-33, 2, 90, 0}, {-33, 6, 90, 0}, {-33, 10, 90, 0},
		{-29, 13.5, 0, 0}, {-25, 13.5, 0, 0}, {-29, -12.5, 0, 0}, {-25, -12.5, 0, 0}, {22.5f, 2.5f, 90, 0}, {-12, -16.5f, 0, 0}, {-8, -17.0f, 10, 0} };
	for (const FWl& Wl : Walls) Prop(TEXT("SM_LowWall"), Ground(Wl.X, Wl.Y, -0.15f), Wl.Z, 1.f, true);

	// ------------------------------------------------ canal: landing, boats, Besa's shed
	Prop(TEXT("SM_LandingStone"), FVector(3020.f, 800.f, -175.f), 0.f, 1.f, true);
	{
		AVigilProp* Boat = Prop(TEXT("SM_Felucca"), FVector(3420.f, 820.f, -150.f), 90.f, 1.f, false, false);
		Named.Add(TEXT("felucca"), Boat);
	}
	Prop(TEXT("SM_Rowboat"), FVector(2880.f, -1380.f, -150.f), 80.f, 1.f, false, false);
	{
		const FVector ShedLoc = Ground(24.5f, -13.5f, -0.05f);
		AVigilProp* Shed = Prop(TEXT("SM_BoatShed"), ShedLoc, -90.f, 1.f, true);
		AVigilProp* Roof = Prop(TEXT("SM_BoatShedRoof"), ShedLoc, -90.f, 1.f, false);
		Roof->Meshes[0]->bCastHiddenShadow = true;
		Prop(TEXT("SM_Nets"), Ground(25.6f, -11.8f, 0.0f), 20.f, 1.f, false);
		AVigilProp* ShedLight = Prop(FString(), ShedLoc, 0, 1, false, false);
		Lights.Add(TEXT("shed"), Lamp(ShedLight, ShedLoc + FVector(-160, 150, 150), 0.25f, 380.f, FLinearColor(1.f, 0.55f, 0.25f), true));
		AVigilZone* Z = Zone(TEXT("shed"), ShedLoc + FVector(0, 0, 150), FVector(240, 300, 200), 0.f);
		Z->HideWhenInside.Add(Roof->Meshes[0]);
		Usable(TEXT("shed"), TEXT("Step inside"), ShedLoc + FVector(0, 300, 0), FString(), 0, 1, 220.f)->Height = 160.f;
	}

	// ------------------------------------------------ the church of the Archangel Michael
	{
		const FVector C = M(-14, 0);
		float Zmax = -1e9f;
		for (int32 k = 0; k < 4; ++k) Zmax = FMath::Max(Zmax, GroundZ(C.X + ((k & 1) ? 560.f : -560.f), C.Y + ((k & 2) ? 360.f : -360.f)));
		const FVector B(C.X, C.Y, Zmax + 4.f);
		// stone plinth so the floor never meets the slope
		{
			AVigilProp* Pl = Prop(FString(), B, 0, 1, false, true);
			UStaticMeshComponent* Cube = Pl->AddMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")), FVector(-60, 0, -60), FRotator::ZeroRotator, FVector(13.4f, 7.2f, 1.2f), true, Mat(TEXT("MI_Stone")));
			Cube->SetCastShadow(true);
			for (int32 s = 0; s < 3; ++s)
				Pl->AddMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")), FVector(620.f + s * 35.f, 0, -18.f - s * 22.f), FRotator::ZeroRotator, FVector(0.4f, 2.0f, 0.36f), true, Mat(TEXT("MI_Stone")));
		}
		Prop(TEXT("SM_ChurchFloor"), B + FVector(0, 0, 2), 0, 1, true);
		AVigilProp* WN = Prop(TEXT("SM_ChurchWallN"), B, 0, 1, true);
		AVigilProp* WS = Prop(TEXT("SM_ChurchWallS"), B, 0, 1, true);
		AVigilProp* WE = Prop(TEXT("SM_ChurchWallE"), B, 0, 1, true);
		AVigilProp* WW = Prop(TEXT("SM_ChurchWallW"), B, 0, 1, true);
		AVigilProp* Roof = Prop(TEXT("SM_ChurchRoof"), B, 0, 1, true);
		AVigilProp* Dome = Prop(TEXT("SM_ChurchDome"), B + FVector(80, 0, 620), 0, 1, false);
		(void)WN; (void)WW;
		AVigilZone* Z = Zone(TEXT("church"), B + FVector(0, 0, 250), FVector(500, 300, 300));
		for (AVigilProp* P : { WS, WE, Roof, Dome })
			for (UStaticMeshComponent* Mc : P->Meshes) { Mc->bCastHiddenShadow = true; Z->HideWhenInside.Add(Mc); }
		// doors, chain and the ash sign
		AVigilProp* DoorA = Prop(TEXT("SM_ChurchDoor"), B + FVector(553, 75, 0), 0, 1, false, false);
		AVigilProp* DoorB = Prop(TEXT("SM_ChurchDoor"), B + FVector(553, -75, 0), 0, 1, false, false);
		DoorB->SetActorScale3D(FVector(1, -1, 1));
		Named.Add(TEXT("church_door_a"), DoorA);
		Named.Add(TEXT("church_door_b"), DoorB);
		AVigilProp* Chain = Prop(TEXT("SM_Chain"), B + FVector(566, 0, 0), 0, 1, false, false);
		Named.Add(TEXT("chain"), Chain);
		AVigilProp* CSign = Prop(TEXT("SM_AshSign"), B + FVector(568, 0, 150), 0, 0.9f, false, false);
		Named.Add(TEXT("church_sign"), CSign);
		for (AVigilProp* P : { DoorA, DoorB, Chain, CSign }) for (UStaticMeshComponent* Mc : P->Meshes) { Mc->bCastHiddenShadow = true; Z->HideWhenInside.Add(Mc); }
		Blocker(B + FVector(560, 0, 140), FVector(30, 80, 140), 0.f, TEXT("church_door"));
		Usable(TEXT("church_door"), TEXT("Try the doors"), B + FVector(640, 0, 0), FString(), 0, 1, 170.f)->Height = 170.f;
		Usable(TEXT("church_exit"), TEXT("Open the doors"), B + FVector(470, 0, 0), FString(), 0, 1, 140.f)->Height = 170.f;
		// lattice windows, high in the long walls
		for (float Lx : { -260.f, 220.f })
			for (float Ly : { -320.f, 320.f })
			{
				AVigilProp* Lt = Prop(TEXT("SM_Lattice"), B + FVector(Lx, Ly, 300.f), 90.f, 1.f, false);
				if (Ly > 0) for (UStaticMeshComponent* Mc : Lt->Meshes) { Mc->bCastHiddenShadow = true; Z->HideWhenInside.Add(Mc); }
			}
		Usable(TEXT("church_window"), TEXT("Look through the lattice"), B + FVector(220, 420, 0), FString(), 0, 1, 170.f)->Height = 200.f;
		// interior: the icon screen, icons turned to the wall
		Prop(TEXT("SM_Templon"), B + FVector(-230, 0, 14), 0, 1, true);
		const float IconY[] = { -102.5f, 102.5f, 212.5f, -212.5f };   // Christ, Theotokos, Archangel Michael, Forerunner
		const TCHAR* IconMat[] = { TEXT("MI_Icon_Christ"), TEXT("MI_Icon_Theotokos"), TEXT("MI_Icon_Michael"), TEXT("MI_Icon_Forerunner") };
		const TCHAR* IconId[] = { TEXT("icon_christ"), TEXT("icon_theotokos"), TEXT("icon_michael"), TEXT("icon_forerunner") };
		for (int32 i = 0; i < 4; ++i)
		{
			AVigilProp* Ic = Prop(FString(), B + FVector(-215, IconY[i], 98), 180.f, 1.f, false, false);
			UStaticMeshComponent* Mc = Ic->AddMesh(Mesh(TEXT("SM_IconBoard")));
			// the painted face is slot 1 (back is wood, front is the icon, frame gold)
			for (int32 s = 0; s < Mc->GetNumMaterials(); ++s)
				if (Mc->GetMaterialSlotNames().IsValidIndex(s) && Mc->GetMaterialSlotNames()[s] == FName(TEXT("IconFront"))) Mc->SetMaterial(s, Mat(IconMat[i]));
			Icons.Add(Ic);
			AVigilInteract* U = Usable(IconId[i], TEXT("Look at the icon"), B + FVector(-140, IconY[i], 14), FString(), 0, 1, 85.f);
			U->Height = 150.f;
		}
		Prop(TEXT("SM_Altar"), B + FVector(-420, 0, 14), 0, 1, true);
		Named.Add(TEXT("altar_cloth"), Prop(TEXT("SM_AltarCloth"), B + FVector(-420, 0, 74), 0, 1, false, false));
		Named.Add(TEXT("ash_bowl"), Prop(TEXT("SM_Bowl"), B + FVector(-430, 20, 121), 0, 1, false, false));
		Named.Add(TEXT("altar_bread"), Prop(TEXT("SM_Bread"), B + FVector(-410, -30, 121), 60, 1.1f, false, false));
		Usable(TEXT("altar"), TEXT("Look at the altar"), B + FVector(-300, 0, 14), FString(), 0, 1, 110.f)->Height = 140.f;
		Prop(TEXT("SM_Lampada"), B + FVector(-160, 102.5f, 150), 0, 1, false);
		Usable(TEXT("lampada"), TEXT("Look at the lamp"), B + FVector(-40, 160, 14), FString(), 0, 1, 90.f)->Height = 170.f;
		{
			AVigilProp* LL = Prop(FString(), B, 0, 1, false, false);
			UPointLightComponent* L = Lamp(LL, B + FVector(-150, 102.5f, 160), 0.f, 900.f, FLinearColor(1.f, 0.5f, 0.22f), true);
			Lights.Add(TEXT("lampada"), L);
			Lights.Add(TEXT("chapel_fill"), Lamp(LL, B + FVector(150, 0, 380), 0.f, 1400.f, FLinearColor(0.6f, 0.7f, 1.f), false));
			Lights.Add(TEXT("templon_glow"), Lamp(LL, B + FVector(-60, 0, 160), 0.f, 700.f, FLinearColor(0.7f, 0.75f, 1.f), false));
			for (int32 s = 0; s < 2; ++s) Lights.Add(FString::Printf(TEXT("candles_%d"), s), Lamp(LL, B + FVector(-140, s ? 230 : -230, 135), 0.f, 450.f, FLinearColor(1.f, 0.6f, 0.3f), true));
			Lights.Add(TEXT("brazier"), Lamp(LL, B + FVector(200, -220, 110), 0.f, 600.f, FLinearColor(1.f, 0.45f, 0.15f), true));
		}
		for (int32 s = 0; s < 2; ++s)
		{
			Prop(TEXT("SM_CandleStand"), B + FVector(-140, s ? 230.f : -230.f, 14), 0, 1, false);
			AVigilProp* Cd = Prop(FString(), B + FVector(-140, s ? 230.f : -230.f, 125), 0, 1, false, false);
			for (int32 c = 0; c < 5; ++c)
			{
				const float A = c * 72.f;
				const FVector O(FMath::Cos(FMath::DegreesToRadians(A)) * 12.f, FMath::Sin(FMath::DegreesToRadians(A)) * 12.f, 0);
				Cd->AddMesh(Mesh(TEXT("SM_Candle")), O);
				UStaticMeshComponent* Fl = Cd->AddMesh(Mesh(TEXT("SM_Flame")), O + FVector(0, 0, 26), FRotator::ZeroRotator, FVector(1), false, Mat(TEXT("MI_Flame")));
				Fl->SetCastShadow(false);
			}
			Cd->SetActorHiddenInGame(true);
			Named.Add(FString::Printf(TEXT("candles_%d"), s), Cd);
			Usable(FString::Printf(TEXT("candlestand_%d"), s), TEXT("Relight the candles"), B + FVector(-140, s ? 230.f : -230.f, 14), FString(), 0, 1, 110.f)->Height = 150.f;
		}
		Prop(TEXT("SM_Brazier"), B + FVector(200, -220, 14), 0, 1, true);
		{
			AVigilProp* Fire = Prop(FString(), B + FVector(200, -220, 88), 0, 1, false, false);
			for (int32 f = 0; f < 6; ++f)
				Fire->AddMesh(Mesh(TEXT("SM_Flame")), FVector(FMath::Cos(f * 1.1f) * 12.f, FMath::Sin(f * 1.1f) * 12.f, 0), FRotator::ZeroRotator, FVector(1.4f + 0.5f * (f % 3)), false, Mat(TEXT("MI_Flame")))->SetCastShadow(false);
			Fire->SetActorHiddenInGame(true);
			Named.Add(TEXT("brazier_fire"), Fire);
		}
		Usable(TEXT("brazier"), TEXT("Look at the brazier"), B + FVector(200, -150, 14), FString(), 0, 1, 110.f)->Height = 120.f;
		Prop(TEXT("SM_ReadingStand"), B + FVector(100, 220, 14), -90, 1, true);
		Named.Add(TEXT("psalter"), Prop(TEXT("SM_Book"), B + FVector(100, 220, 132), -90, 1, false, false));
		Usable(TEXT("psalter"), TEXT("Take the Psalter"), B + FVector(100, 160, 14), FString(), 0, 1, 110.f)->Height = 150.f;
		Prop(TEXT("SM_Chest"), B + FVector(-460, -230, 14), 90, 1, true);
		Usable(TEXT("chest"), TEXT("Open the chest"), B + FVector(-460, -160, 14), FString(), 0, 1, 100.f)->Height = 100.f;
		Prop(TEXT("SM_Trapdoor"), B + FVector(-360, 210, 15), 0, 1, false);
		Usable(TEXT("trapdoor"), TEXT("The trapdoor"), B + FVector(-360, 210, 14), FString(), 0, 1, 100.f)->Height = 60.f;
		Prop(TEXT("SM_Mat"), B + FVector(300, 230, 6), 0, 0.8f, false);
		Prop(TEXT("SM_Mat"), B + FVector(-300, -200, 16), 90, 0.6f, false);
		// the font, set out once the church is clean
		AVigilProp* Font = Prop(FString(), B + FVector(380, -230, 14), 0, 1, false, false);
		Font->AddMesh(Mesh(TEXT("SM_CandleStand")));
		Font->AddMesh(Mesh(TEXT("SM_Bowl")), FVector(0, 0, 112), FRotator::ZeroRotator, FVector(2.4f, 2.4f, 2.4f));
		Font->SetActorHiddenInGame(true);
		Named.Add(TEXT("font"), Font);
		AVigilInteract* FontU = Usable(TEXT("font"), TEXT("Fill the flask at the font"), B + FVector(380, -160, 14), FString(), 0, 1, 110.f);
		FontU->Height = 140.f;
		FontU->bEnabled = false;
		// Ioannes's journal waits in a recess behind the Archangel's board
		Named.Add(TEXT("codex"), Prop(TEXT("SM_Codex"), B + FVector(-250, 212.5f, 110), 0, 1, false, false));
		Named[TEXT("codex")]->SetActorHiddenInGame(true);
		// rite marks on the four walls (sprinkling, censing)
		const FVector WallPts[] = { B + FVector(0, -250, 14), B + FVector(0, 250, 14), B + FVector(450, 120, 14), B + FVector(-150, -160, 14) };
		for (int32 w = 0; w < 4; ++w)
		{
			AVigilInteract* Wp = Usable(FString::Printf(TEXT("wall_%d"), w), TEXT("The wall"), WallPts[w], FString(), 0, 1, 130.f);
			Wp->bEnabled = false;
			Wp->Height = 160.f;
		}
		// the semantron frame, empty, by the door
		Prop(TEXT("SM_SemantronFrame"), B + FVector(660, 440, -12), 0, 1, true);
		AVigilProp* Sem = Prop(TEXT("SM_Semantron"), B + FVector(660, 440, 160), 0, 1, false, false);
		Sem->SetActorHiddenInGame(true);
		Named.Add(TEXT("semantron_hung"), Sem);
		Usable(TEXT("semantron_frame"), TEXT("The empty frame"), B + FVector(740, 440, -12), FString(), 0, 1, 150.f)->Height = 220.f;
		Named.Add(TEXT("church_base"), Prop(FString(), B, 0, 1, false, false));
	}

	// ------------------------------------------------ burial ground and bone house
	{
		int32 Seed = 543;
		auto Rand = [&Seed]() { Seed = Seed * 1103515245 + 12345; return ((Seed >> 8) & 0xffff) / 65535.f; };
		for (int32 gx = 0; gx < 5; ++gx)
			for (int32 gy = 0; gy < 7; ++gy)
			{
				const float X = -22.5f - gx * 2.0f + (Rand() - 0.5f) * 0.6f;
				const float Y = -9.0f + gy * 2.6f + (Rand() - 0.5f) * 0.8f;
				if (Y > 3.8f && Y < 8.6f && X > -27.f) continue;     // the path to the bone house
				const float Rk = Rand();
				if (Rk < 0.45f) Prop(TEXT("SM_Stele"), Ground(X, Y, -0.05f), 90.f + (Rand() - 0.5f) * 14.f, 1.f, true);
				else if (Rk < 0.75f) Prop(TEXT("SM_GraveCross"), Ground(X, Y, -0.05f), 90.f + (Rand() - 0.5f) * 20.f, 1.f, true);
				Prop(TEXT("SM_GraveMound"), Ground(X - 0.8f, Y, -0.08f), 0.f, 1.f, false);
			}
		const FVector BH = Ground(-25.5f, 6.4f, -0.05f);
		Prop(TEXT("SM_BoneHouseEntrance"), BH, 0.f, 1.f, true);
		Usable(TEXT("bonehouse"), TEXT("Go down the stair"), BH + FVector(170, 0, 0), FString(), 0, 1, 170.f)->Height = 200.f;
	}

	// ------------------------------------------------ convent of the Theotokos, across the canal
	{
		const FVector Cv = M(52, -4);
		float Zmin = 1e9f;
		for (int32 k = 0; k < 4; ++k) Zmin = FMath::Min(Zmin, GroundZ(Cv.X + ((k & 1) ? 800.f : -800.f), Cv.Y + ((k & 2) ? 700.f : -700.f)));
		const FVector B(Cv.X, Cv.Y, Zmin - 8.f);
		Prop(TEXT("SM_ConventWall"), B, 0, 1, true);
		Prop(TEXT("SM_Gate"), B + FVector(-800, 90, 0), 0, 1, false);
		AVigilProp* G2 = Prop(TEXT("SM_Gate"), B + FVector(-800, -90, 0), 0, 1, false, false);
		G2->SetActorScale3D(FVector(1, -1, 1));
		Prop(TEXT("SM_PalmB"), Ground(47.f, -1.f), 30, 0.9f, false);
		AVigilProp* CL = Prop(FString(), B, 0, 1, false, false);
		UPointLightComponent* L = Lamp(CL, B + FVector(-140, -150, 470), 0.f, 900.f, FLinearColor(1.f, 0.6f, 0.3f), false);
		Lights.Add(TEXT("convent"), L);
		Named.Add(TEXT("convent_base"), CL);
	}

	// ------------------------------------------------ underground sets, far below the village
	{
		// the bone house: main passage runs west from the stair, a side passage north to a chamber
		const FVector O = M(-14, 0, -40);
		AVigilProp* Oss = Prop(TEXT("SM_Ossuary"), O, 0, 1, true);
		if (Oss && Oss->Meshes.Num())
		{
			UStaticMeshComponent* Mc = Oss->Meshes[0];
			const TArray<FName> Slots = Mc->GetMaterialSlotNames();
			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				if (Slots[i].ToString().StartsWith(TEXT("Cutaway"))) Mc->SetMaterial(i, Mat(TEXT("MI_Cutaway")));
				UE_LOG(LogTemp, Display, TEXT("VIGIL ossuary slot %d %s -> %s"), i, *Slots[i].ToString(), Mc->GetMaterial(i) ? *Mc->GetMaterial(i)->GetName() : TEXT("none"));
			}
		}
		AVigilProp* Sk = Prop(TEXT("SM_OssSkulls"), O, 0, 1, false);
		(void)Sk;
		Prop(TEXT("SM_BonePile"), O + M(-8.2f, -9.2f), 30, 1, false);
		Prop(TEXT("SM_ScratchPanel"), O + M(-11.2f, -10.2f, 0.25f), 0, 1, false);
		Prop(TEXT("SM_Stole"), O + M(-7.6f, -12.2f, 0.55f), 90, 1, false);
		Named.Add(TEXT("stole"), Prop(TEXT("SM_Stole"), O + M(-7.6f, -12.2f, 0.55f), 90, 1, false, false));
		Named.Add(TEXT("wrapped"), Prop(TEXT("SM_SemantronWrapped"), O + M(-7.9f, -8.7f, 0.0f), 20, 1, false, false));
		Prop(TEXT("SM_Shelf"), O + M(-9.9f, -12.0f), 90, 1, true);
		Named.Add(TEXT("oiljar"), Prop(TEXT("SM_Jar"), O + M(-9.9f, -12.0f, 1.03f), 0, 0.6f, false, false));
		Usable(TEXT("skulls"), TEXT("Look at the skulls"), O + M(-4.0f, 0.7f), FString(), 0, 1, 150.f)->Height = 160.f;
		Usable(TEXT("scratch"), TEXT("Look at the wall"), O + M(-10.4f, -10.2f), FString(), 0, 1, 120.f)->Height = 140.f;
		Usable(TEXT("stole"), TEXT("Take the stole"), O + M(-7.6f, -11.6f), FString(), 0, 1, 110.f)->Height = 140.f;
		Usable(TEXT("bones"), TEXT("Search the bones"), O + M(-8.0f, -8.9f), FString(), 0, 1, 120.f)->Height = 90.f;
		Usable(TEXT("oiljar"), TEXT("Take the jar of oil"), O + M(-9.9f, -11.4f), FString(), 0, 1, 110.f)->Height = 140.f;
		Usable(TEXT("trapdoor_up"), TEXT("Push up the trapdoor"), O + M(-10.6f, -11.2f, 2.4f), FString(), 0, 1, 130.f)->Height = 60.f;
		Usable(TEXT("oss_stair"), TEXT("Climb back up"), O + M(0.6f, 0, 0.9f), FString(), 0, 1, 120.f)->Height = 100.f;
		Zone(TEXT("oss_encounter"), O + M(-15.0f, 0, 1.2f), FVector(250, 160, 250));
		Zone(TEXT("oss_side"), O + M(-9.0f, -3.2f, 1.2f), FVector(150, 140, 250));
		AVigilProp* OL = Prop(FString(), O, 0, 1, false, false);
		Lights.Add(TEXT("oss_far"), Lamp(OL, O + M(-26.0f, 0, 1.8f), 0.f, 600.f, FLinearColor(0.8f, 0.85f, 1.f), false));
		AVigilFigure* Br = Figure(TEXT("oss_bright"), TEXT("SK_BrightOne"), O + M(-25.5f, 0), 0.f, false, 1.3f);
		if (Br) { Br->SetFade(0.f); Br->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); Br->GetCharacterMovement()->SetMovementMode(MOVE_Flying); Br->GetCharacterMovement()->MaxFlySpeed = 130.f; }
		OssBright = Br;
		// blockers at both ends of the passage so nobody walks into the rock
		Blocker(O + M(2.0f, 0, 1.5f), FVector(40, 200, 200));
	}
	{
		// the well, seen from inside: a reflection and a figure that is not there
		const FVector V = M(6, 0, -70);
		Prop(TEXT("SM_Well"), V, 0, 1, false);
		AVigilProp* VL = Prop(FString(), V, 0, 1, false, false);
		Lights.Add(TEXT("vision"), Lamp(VL, V + FVector(0, 0, -280), 0.f, 400.f, FLinearColor(0.7f, 0.8f, 1.f), false));
		Lights.Add(TEXT("vision_wall"), Lamp(VL, V + FVector(45, 0, -400), 0.f, 120.f, FLinearColor(1.f, 0.7f, 0.4f), false));
		if (AVigilProp* Wd = Prop(TEXT("SM_WellWater"), V + FVector(0, 0, -360), 0, 1, false, false))
		{
			Wd->Meshes[0]->SetMaterial(0, Mat(TEXT("MI_Water")));
			Wd->Meshes[0]->SetCastShadow(false);
			Wd->SetActorHiddenInGame(true);
		}
		// figures hang upside down below the waterline
		for (const TCHAR* Nm : { TEXT("refl_alexios"), TEXT("refl_bright") })
		{
			const bool bB = FString(Nm) == TEXT("refl_bright");
			AVigilFigure* F = Figure(Nm, bB ? TEXT("SK_BrightOne") : TEXT("SK_Alexios"), V + FVector(bB ? 40.f : 0.f, bB ? 35.f : 0.f, -540.f), 0.f, !bB, bB ? 1.25f : 1.f);
			if (!F) continue;
			F->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			F->GetCharacterMovement()->SetMovementMode(MOVE_None);
			F->SetActorLocation(V + FVector(bB ? 40.f : 0.f, bB ? 35.f : 0.f, bB ? -480.f : -450.f));
			F->SetActorRotation(FRotator(0, 45.f, 180.f));
			F->SetActorHiddenInGame(true);
		}
	}

	// ------------------------------------------------ people
	{
		AVigilFigure* Hor = Figure(TEXT("hor"), TEXT("SK_Besa"), FVector(3420.f, 560.f, -100.f), 90.f, true);
		if (Hor)
		{
			Hor->GetCharacterMovement()->SetMovementMode(MOVE_None);
			Hor->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Hor->SetActorLocation(FVector(3420.f, 560.f, -100.f + 92.f));
			Hor->AttachToActor(Named[TEXT("felucca")], FAttachmentTransformRules::KeepWorldTransform);
		}
		AVigilFigure* Hd = Figure(TEXT("hooded"), TEXT("SK_Hooded"), Ground(14.0f, 16.0f), -90.f, true);
		if (Hd)
		{
			Hd->Hold(TEXT("SM_CrookLamp"), TEXT("hand_r"), FVector(-4, 2, 0), FRotator(0, 0, 180));
			AVigilProp* HL = Prop(FString(), Hd->GetActorLocation(), 0, 1, false, false);
			HL->AttachToActor(Hd, FAttachmentTransformRules::KeepWorldTransform);
			Lights.Add(TEXT("hooded"), Lamp(HL, Hd->GetActorLocation() + FVector(40, 0, 10), 2.4f, 600.f, FLinearColor(1.f, 0.6f, 0.3f), true));
			Hd->SetActorHiddenInGame(true);
		}
		AVigilFigure* Th = Figure(TEXT("thecla"), TEXT("SK_Thecla"), Ground(9.0f, -5.4f), 130.f, true, 0.95f);
		if (Th) Th->SetActorHiddenInGame(true);
		AVigilFigure* Pk = Figure(TEXT("pakhom"), TEXT("SK_Pakhom"), Ground(8.3f, -6.6f), 130.f, true, 0.66f);
		if (Pk) Pk->SetActorHiddenInGame(true);
		AVigilFigure* Bs = Figure(TEXT("besa"), TEXT("SK_Besa"), Ground(23.6f, -14.8f), 90.f, true, 1.03f);
		if (Bs) Bs->Hold(TEXT("SM_Key"), TEXT("hand_r"), FVector(0, 3, 0), FRotator(0, 0, 90), 2.5f);
		AVigilFigure* An = Figure(TEXT("anastasia"), TEXT("SK_Anastasia"), Ground(43.4f, -4.0f), 180.f, true, 0.96f);
		if (An) { An->SetActorHiddenInGame(true); An->Hold(TEXT("SM_Bread"), TEXT("hand_r"), FVector(0, 6, 0), FRotator(0, 0, 0), 1.6f); }
		AVigilFigure* Wt = Figure(TEXT("watcher"), TEXT("SK_BrightOne"), M(-14 + 2.2f, 3.95f) + FVector(0, 0, GroundZ(-1180.f, 395.f)), -90.f, false, 1.55f);
		if (Wt) { Wt->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); Wt->GetCharacterMovement()->SetMovementMode(MOVE_None); Wt->SetFade(0.f); }
		// the Guest, true form, for one frame at the very end
		AVigilFigure* Gs = Figure(TEXT("guest"), TEXT("SK_Guest"), M(0, 0, -100), 0.f, true, 1.15f);
		if (Gs)
		{
			Gs->GetMesh()->SetMaterial(0, Mat(TEXT("MI_DemonSkin")));
			for (int32 i = 0; i < Gs->GetMesh()->GetNumMaterials(); ++i) Gs->GetMesh()->SetMaterial(i, Mat(TEXT("MI_DemonSkin")));
			Gs->GetCharacterMovement()->SetMovementMode(MOVE_None);
			Gs->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// the eyes glow: same emissive as the candle flames, on the eye slot of its model
			if (Gs->Garment)
			{
				Gs->Garment->SetMaterialByName(TEXT("DemonEye"), Mat(TEXT("MI_DemonEye")));
				const TArray<FName> Sl = Gs->Garment->GetMaterialSlotNames();
				for (int32 i = 0; i < Sl.Num(); ++i)
					UE_LOG(LogTemp, Display, TEXT("VIGIL guest slot %d %s -> %s"), i, *Sl[i].ToString(), Gs->Garment->GetMaterial(i) ? *Gs->Garment->GetMaterial(i)->GetPathName() : TEXT("none"));
			}
			AVigilProp* GL = Prop(FString(), Gs->GetActorLocation(), 0, 1, false, false);
			Lights.Add(TEXT("guest_fill"), Lamp(GL, Gs->GetActorLocation() + FVector(230, 60, 120), 0.f, 800.f, FLinearColor(0.75f, 0.6f, 0.6f), false));
			Lights.Add(TEXT("guest_under"), Lamp(GL, Gs->GetActorLocation() + FVector(110, 70, -70), 0.f, 400.f, FLinearColor(1.f, 0.35f, 0.15f), false));   // from below, like a brazier at its feet
			Lights.Add(TEXT("guest"), Lamp(GL, Gs->GetActorLocation() + FVector(-170, -120, 150), 0.f, 900.f, FLinearColor(1.f, 0.22f, 0.08f), true));   // behind it: a silhouette
		}
		// bright ones in the robes give light that falls on nothing
		for (const auto& KV : Figures)
		{
			if (KV.Value && KV.Value->Garment && (KV.Key.Contains(TEXT("bright")) || KV.Key == TEXT("watcher")))
			{
				KV.Value->Garment->SetCastShadow(false);
				KV.Value->Garment->bAffectDynamicIndirectLighting = false;
				KV.Value->Garment->bAffectDistanceFieldLighting = false;
			}
		}
	}

	// ------------------------------------------------ bounds: the canal bank, the fields, the ridge
	Blocker(M(27.3f, -16.f, 1.f), FVector(30, 1450, 300));    // bank south of the landing
	Blocker(M(27.3f, 20.f, 1.f), FVector(30, 1050, 300));     // bank north of the landing
	Blocker(M(29.0f, 6.7f, 1.f), FVector(200, 20, 300));      // sides of the landing stair
	Blocker(M(29.0f, 9.3f, 1.f), FVector(200, 20, 300));
	Blocker(M(-4.f, -22.f, 1.f), FVector(3200, 30, 400));
	Blocker(M(-4.f, 22.f, 1.f), FVector(3200, 30, 400));
	Blocker(M(-34.5f, 0.f, 1.f), FVector(30, 2300, 400));
	Blocker(M(32.5f, 8.f, 1.f), FVector(30, 300, 400));       // beyond the landing stone

	UE_LOG(LogTemp, Display, TEXT("VIGIL world built: %d usables, %d figures, %d zones"), Usables.Num(), Figures.Num(), Zones.Num());
}
