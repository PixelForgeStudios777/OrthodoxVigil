#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VigilGameMode.generated.h"

class AVigilFigure;
class AVigilProp;
class AVigilInteract;
class AVigilZone;
class AVigilCameraRig;
class UStaticMesh;
class UMaterialInterface;
class UTexture2D;
class USoundBase;
class UAudioComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UFont;

enum class EVStep : uint8 { Line, Choice, Wait, Fade, Card, Call, Until, Doc };

struct FVChoice
{
	FString Text;
	TFunction<void()> Fn;
	FName OnceFlag;   // option disappears once this flag is set (dialogue menus)
};

struct FVStep
{
	EVStep Kind = EVStep::Call;
	FString Speaker, Text, Portrait;
	bool bCoptic = false;
	bool bLock = true;        // blocks movement while it runs
	float Time = 0.f, Value = 0.f;
	TFunction<void()> Fn;
	TFunction<bool()> Pred;
	TArray<FVChoice> Opts;
	TArray<FString> Pages;
};

struct FVJournal { FString Title, Text; };

enum class EVScreen : uint8 { Title, Playing, Paused, Journal, End };
enum class EVLight : uint8 { Night, Dusk, Day, Sunset, Under, Vision, Dawn };

// Rite of cleansing (I-6): seven steps with interruptions.
struct FVRite
{
	bool bActive = false;
	int32 Step = 0;          // 1..7
	int32 Count = 0;         // sub-progress inside a step
	bool bClothCarried = false;
	int32 IconsTurned = 0;
	bool bVoice = false;     // false voice calling from the trapdoor
	float VoiceTimer = 0.f;
	bool bWind = false;      // candles guttering
	float WindTimer = 0.f, WindCooldown = 25.f;
	bool bCandlesOut = false;
	bool bIconTurning = false;
	float IconTurn = 0.f;    // 0 faces the nave, 1 faces the wall
	bool bIconDone = false;
	bool bWatcher = false;   // a bright one at the lattice window
	bool bWallDone[4] = { false, false, false, false };
	int32 Strikes = 0;
	float StrikeTimer = 0.f;
};

UCLASS()
class ORTHODOXVIGIL_API AVigilGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AVigilGameMode();
	static AVigilGameMode* Get(const UObject* Ctx);
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	// ------------------------------------------------ assets
	UStaticMesh* Mesh(const FString& Name);
	UMaterialInterface* Mat(const FString& Name);
	UTexture2D* Tex(const FString& Name);
	USoundBase* Sound(const FString& Name);
	UFont* SerifFont(bool bBold = false);
	void Play(const FString& Name, float Vol = 1.f, float Pitch = 1.f);
	void PlayAt(const FString& Name, FVector Where, float Vol = 1.f, float Pitch = 1.f);
	void Loop(const FString& Channel, const FString& Sound, float Vol, float FadeTime = 1.5f);
	void StopLoop(const FString& Channel, float FadeTime = 1.5f);

	// ------------------------------------------------ world building (VigilWorld.cpp)
	void BuildWorld();
	float GroundZ(float X, float Y, float Fallback = 0.f) const;
	FVector Ground(float Xm, float Ym, float Lift = 0.f) const;    // metres in, cm out, snapped to the ground
	AVigilProp* Prop(const FString& MeshName, FVector Loc, float Yaw = 0.f, float Scale = 1.f, bool bCollide = false, bool bStatic = true);
	AVigilInteract* Usable(const FString& Id, const FString& Prompt, FVector Loc, const FString& MeshName = FString(), float Yaw = 0.f, float Scale = 1.f, float Radius = 170.f);
	AVigilFigure* Figure(const FString& Name, const FString& Garment, FVector Loc, float Yaw, bool bShowBody = true, float Scale = 1.f);
	UPointLightComponent* Lamp(AActor* LightOwner, FVector Loc, float Candela, float Radius, FLinearColor Color, bool bShadows = true);
	AVigilZone* Zone(const FString& Id, FVector Center, FVector Half, float Yaw = 0.f);
	void Blocker(FVector Center, FVector Half, float Yaw = 0.f, const FString& Tag = FString());
	void SetBlocker(const FString& Tag, bool bOn);
	void SetLighting(EVLight Preset);
	void SetIconFacing(int32 Icon, float Turn);   // 0 = facing the nave, 1 = facing the wall

	// ------------------------------------------------ story queue
	void Say(const FString& Speaker, const FString& Text, bool bCoptic = false);
	void Choose(TArray<FVChoice> Opts);
	void Wait(float Seconds, bool bLock = true);
	void FadeTo(float Alpha, float Seconds = 1.f);
	void Card(const FString& Title, const FString& Sub, float Seconds = 4.f);
	void Do(TFunction<void()> Fn);
	void Until(TFunction<bool()> Pred, const FString& Hint = FString(), bool bLock = false);
	void Doc(const FString& Title, TArray<FString> Pages);
	bool Busy() const { return Queue.Num() > 0 || Staging.Num() > 0; }
	bool Locked() const;
	void Advance();                 // E / Space / click on a line, card or document
	void ChoiceMove(int32 Dir);
	void ChoicePick(int32 Index = -1);

	// ------------------------------------------------ state
	bool Has(FName F) const { return Flags.Contains(F); }
	void Set(FName F) { Flags.Add(F); }
	void Journal(const FString& Title, const FString& Text);
	void Give(FName Item, const FString& Display);
	void Take(FName Item) { Items.Remove(Item); }
	bool Holds(FName Item) const { return Items.Contains(Item); }
	void Objective(const FString& Text) { ObjectiveText = Text; ObjectiveFlash = 3.f; }
	void Hint(const FString& Text, float Seconds = 6.f) { HintText = Text; HintTime = Seconds; }
	void Sub(const FString& Speaker, const FString& Text, float Seconds = 4.f, bool bCoptic = false);
	void Shake(float Amount);
	void Flash(FLinearColor C, float Seconds);
	void Hurt(float Composure);     // costs composure
	void Calm(float Composure);

	// ------------------------------------------------ input from the controller
	void OnInteractPressed();
	void OnPrayHeld(bool bHeld);
	void OnKnotPressed();
	void OnCrossHeld(bool bHeld);
	void OnHolyWater();
	void OnJournal();
	void OnPause();
	void OnMenuMove(int32 Dir);
	void OnMenuPick();
	void MoveInput(FVector2D Axis, bool bHurry);

	// ------------------------------------------------ handlers (VigilStory.cpp)
	void StartGame();
	void OnInteract(const FString& Id);
	void OnZone(const FString& Id, bool bInside);
	void StoryTick(float Dt);
	void BeginRite();
	void RiteTick(float Dt);
	void RiteInteract(const FString& Id);
	void RiteSprinkle();
	void OssuaryTick(float Dt);
	void GoTo(FVector WhereCm, float Yaw, EVLight Light, bool bUnder);
	AVigilInteract* FindUsable(const FString& Id) const;
	void Enable(const FString& Id, bool bOn);
	AVigilFigure* NPC(const FString& Name) const;

	// ------------------------------------------------ autopilot (VigilAuto.cpp, development only)
	void AutoBegin();
	void JumpTo(const FString& Where);   // dev: -VigilJump=ossuary|day2|rite|convent|guest|village
	void AutoTick(float Dt);
	void Shot(const FString& Name);

	// ------------------------------------------------ public state read by the HUD
	UPROPERTY() TObjectPtr<AVigilFigure> Priest;
	UPROPERTY() TObjectPtr<AVigilCameraRig> Rig;
	UPROPERTY() TArray<TObjectPtr<AVigilInteract>> Usables;
	UPROPERTY() TArray<TObjectPtr<AVigilZone>> Zones;
	UPROPERTY() TMap<FString, TObjectPtr<AVigilFigure>> Figures;
	UPROPERTY() TMap<FString, TObjectPtr<AVigilProp>> Named;      // props the story moves or hides
	UPROPERTY() TMap<FString, TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY() TArray<TObjectPtr<AVigilProp>> Icons;               // Christ, Theotokos, Michael, Forerunner
	UPROPERTY() TMap<FString, TObjectPtr<UPrimitiveComponent>> Blockers;

	EVScreen Screen = EVScreen::Title;
	int32 MenuSel = 0;
	TSet<FName> Flags;
	TArray<FVJournal> JournalEntries;
	TArray<FName> Items;
	TMap<FName, FString> ItemNames;
	FString ObjectiveText;
	float ObjectiveFlash = 0.f;
	FString HintText;
	float HintTime = 0.f;
	FString SubSpeaker, SubText;
	bool bSubCoptic = false;
	float SubTime = 0.f;
	FString PlaceText;        // small caption top-left (where and when)
	float Composure = 100.f;
	int32 HolyWater = 3;
	bool bHasCross = false, bHasFlask = false, bHasRope = true;
	bool bPraying = false;
	float KnotPhase = 0.f, KnotFlash = 0.f;
	int32 KnotsThisPrayer = 0, KnotsTotal = 0;
	bool bCrossUp = false;
	float CrossRing = 1.f;
	bool bCrossPressed = false;   // a bright one is being held by the cross right now
	float FadeAlpha = 1.f, FadeTarget = 1.f, FadeSpeed = 1.f;
	FString CardTitle, CardSub;
	float CardTime = 0.f, CardTotal = 0.f;
	float FlashTime = 0.f, FlashTotal = 0.f;
	FLinearColor FlashColor = FLinearColor::White;
	float ShakeAmount = 0.f;
	float StepAge = 0.f;
	float ChapterTime = 0.f;
	int32 ChoiceSel = 0;
	int32 DocPage = 0;
	TArray<FVStep> Queue;
	TArray<FVStep> Staging;
	FVRite Rite;
	FString Chapter;              // which part of the night we are in
	float JournalScroll = 0.f;
	bool bAutoPilot = false;
	bool bShots = false;
	float GuestFlash = 0.f;       // the one-frame true form at the end
	float ShotAt = -1.f;          // dev: -VigilShotAt=seconds takes a screenshot then quits

	// ossuary encounter
	int32 OssState = 0;           // 0 dark walk, 1 bright one comes, 2 withdrawn
	float OssCooldown = 0.f;
	UPROPERTY() TObjectPtr<AVigilFigure> OssBright;

	// lighting
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY() TObjectPtr<USkyLightComponent> Sky;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY() TObjectPtr<UPostProcessComponent> PPV;
	UPROPERTY() TObjectPtr<UPointLightComponent> HandLamp;
	EVLight LightNow = EVLight::Night;
	bool bUnderground = false;

private:
	void RunQueue(float Dt);
	void Commit();
	void PopFront();
	void SystemsTick(float Dt);
	UObject* LoadCached(const FString& Path, UClass* Class);
	UPROPERTY() TMap<FString, TObjectPtr<UObject>> Cache;
	UPROPERTY() TMap<FString, TObjectPtr<UAudioComponent>> Loops;
	UPROPERTY() TObjectPtr<UFont> Serif;
	UPROPERTY() TObjectPtr<UFont> SerifBold;
	float StepTimer = 0.f;
};
