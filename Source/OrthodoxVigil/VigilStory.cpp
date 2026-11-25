// The story of the slice: Prologue P2 and P3 (condensed), then Act I, scenes I-1 to I-7, and the opening of II-1.
// Lines are taken from "Orthodox Vigil: Story Script" (8 Nov 2026). Lines marked ADDED are not in the script.
#include "VigilGameMode.h"
#include "VigilActors.h"
#include "VigilPlayer.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

static FVector M(float X, float Y, float Z = 0.f) { return FVector(X * 100.f, Y * 100.f, Z * 100.f); }
static const FVector OSS = FVector(-1400.f, 0.f, -4000.f);   // bone house origin
static const FVector VIS = FVector(600.f, 0.f, -7000.f);     // the well vision set

// small tweens for doors, icons and the boat
struct FTw { TWeakObjectPtr<AActor> A; FVector L0, L1; float Y0, Y1; float T = 0.f, Dur = 1.f; bool bLoc = false; };
static TArray<FTw> GTweens;
static void Tween(AActor* A, float ToYaw, float Dur)
{
	if (!A) return;
	FTw T; T.A = A; T.Y0 = A->GetActorRotation().Yaw; T.Y1 = ToYaw; T.Dur = Dur; T.L0 = T.L1 = A->GetActorLocation();
	GTweens.Add(T);
}
static void TweenLoc(AActor* A, FVector To, float Dur)
{
	if (!A) return;
	FTw T; T.A = A; T.Y0 = T.Y1 = A->GetActorRotation().Yaw; T.Dur = Dur; T.L0 = A->GetActorLocation(); T.L1 = To; T.bLoc = true;
	GTweens.Add(T);
}

static FVector ChurchB(AVigilGameMode* GM)
{
	TObjectPtr<AVigilProp>* P = GM->Named.Find(TEXT("church_base"));
	return (P && *P) ? (*P)->GetActorLocation() : M(-14, 0);
}

static void ShowProp(AVigilGameMode* GM, const TCHAR* Name, bool bShow)
{
	if (TObjectPtr<AVigilProp>* P = GM->Named.Find(Name)) if (*P) (*P)->SetActorHiddenInGame(!bShow);
}

static void LightOn(AVigilGameMode* GM, const TCHAR* Name, float Cd)
{
	if (TObjectPtr<UPointLightComponent>* L = GM->Lights.Find(Name)) if (*L) (*L)->SetIntensity(Cd);
}

static void Narrate(AVigilGameMode* GM, const FString& Text) { GM->Say(TEXT(""), Text); }

// ================================================================ start: prologue and the river

void AVigilGameMode::StartGame()
{
	GTweens.Reset();
	Screen = EVScreen::Playing;
	StopLoop(TEXT("music"), 2.f);
	FadeAlpha = 1.f; FadeTarget = 1.f;
	Chapter = TEXT("prologue");
	ChapterTime = 0.f;
	PlaceText = TEXT("Alexandria. The Patriarch's study. 11 November, AD 543.");
	bHasRope = true;
	if (Rig) Rig->Fixed(FVector(3900.f, 1700.f, 450.f), (FVector(3200.f, 650.f, 0.f) - FVector(3900.f, 1700.f, 450.f)).Rotation(), 40.f, 10.f);

	// P2. The Commission (over black, with portraits)
	Wait(1.0f);
	Say(TEXT("PATRIARCH"), TEXT("Sit. You served well this morning."));
	Say(TEXT("ALEXIOS"), TEXT("I shook, Your Beatitude."));
	Say(TEXT("PATRIARCH"), TEXT("Everyone shakes. The ones who stop shaking worry me more."));
	Say(TEXT("PATRIARCH"), TEXT("Tarsenis. Three days upriver on the western edge of the delta. Mostly fishermen, gathered around a church of the Archangel Michael. The great death took a third of them last year, and their priest with them. I sent Father Ioannes in the spring."));
	Say(TEXT("ALEXIOS"), TEXT("I remember him. He taught me the hours when I was a boy."));
	Say(TEXT("PATRIARCH"), TEXT("He wrote every month until the flood. Then this."));
	Doc(TEXT("Ioannes's Letter"), { TEXT("To His Beatitude, greetings in Christ. All is well in Tarsenis. The people are at peace. We have found a better shepherd than I could ever be, and the village wants for nothing.\n\nSend no one. Send no grain. Send no one. Pray for me. Pray for me. Pray") });
	Say(TEXT("ALEXIOS"), TEXT("He wrote \"send no one\" twice."));
	Say(TEXT("PATRIARCH"), TEXT("That was ninety days ago. The grain barge went up in the month of Paopi and never came back. The boatmen say lamps burn at night in the old tombs west of the village, and nobody rows near it now."));
	Say(TEXT("ALEXIOS"), TEXT("What would you have me do?"));
	Say(TEXT("PATRIARCH"), TEXT("Find Ioannes, and open the church if it has been closed. Write to me what you see, and tell me the truth even if it sounds like madness."));
	Say(TEXT("KOSMAS"), TEXT("Your Beatitude, with respect, there are older priests."));
	Say(TEXT("PATRIARCH"), TEXT("There are."));
	Say(TEXT("KOSMAS"), TEXT("Then why him?"));
	Wait(1.2f);
	Say(TEXT("PATRIARCH"), TEXT("Because he is young, and he has nothing to unlearn."));
	Say(TEXT("PATRIARCH"), TEXT("A cross with a relic of Saint Menas. Hold it up when you are afraid, and let him do the standing."));
	Do([this]() { bHasCross = true; Give("cross", TEXT("Cross of Saint Menas")); });
	Say(TEXT("PATRIARCH"), TEXT("Water from Theophany, in one of the saint's flasks. Use it sparingly; there is no font on the road."));
	Do([this]() { bHasFlask = true; HolyWater = 3; Give("flask", TEXT("Holy Water (Menas ampulla)")); });
	Say(TEXT("PATRIARCH"), TEXT("My blessing in writing. If the church in Tarsenis has been defiled, a bishop should cleanse it. This makes the rite mine as much as yours, so you may perform it."));
	Do([this]() { Give("blessing", TEXT("Letter of Blessing")); });
	Say(TEXT("PATRIARCH"), TEXT("And this, so you remember why you went."));
	Do([this]() { Give("ioannes_letter", TEXT("Ioannes's Letter")); Give("rope", TEXT("Prayer Rope")); Give("codex", TEXT("Codex")); });
	Say(TEXT("PATRIARCH"), TEXT("One more question. Do you dream, Alexios?"));
	Say(TEXT("ALEXIOS"), TEXT("Sometimes. Of water. Of steps going down into the dark."));
	Say(TEXT("PATRIARCH"), TEXT("Pray before you sleep. Every night."));
	Say(TEXT("PATRIARCH"), TEXT("Seven days. If I hear nothing from you in seven days, I will come myself."));
	Say(TEXT("KOSMAS"), TEXT("Don't look so solemn. You're being sent because the Church can spare you. Arithmetic, Father. Don't take it to heart."));
	Say(TEXT("ALEXIOS"), TEXT("I'll try to come back and spoil his sums."));

	// P3. The River, condensed to the last evening on the water
	Card(TEXT("The River"), TEXT("Three days upriver"), 3.5f);
	Do([this]() {
		Chapter = TEXT("river");
		ChapterTime = 0.f;
		PlaceText = TEXT("The canal below Tarsenis. 14 November. Dusk.");
		SetLighting(EVLight::Dusk);
		Loop(TEXT("amb"), TEXT("amb_water"), 0.6f, 2.f);
		Loop(TEXT("amb2"), TEXT("amb_night"), 0.35f, 2.f);
		if (Priest)
		{
			Priest->GetCharacterMovement()->SetMovementMode(MOVE_None);
			Priest->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Priest->SetActorLocationAndRotation(FVector(3420.f, 960.f, -100.f + 92.f), FRotator(0, 180.f, 0));
			Priest->AttachToActor(Named[TEXT("felucca")], FAttachmentTransformRules::KeepWorldTransform);
		}
	});
	FadeTo(0.f, 2.5f);
	Say(TEXT("HOR"), TEXT("You keep looking at the bank, Father."));
	Say(TEXT("ALEXIOS"), TEXT("There are no lamps in that village."));
	Say(TEXT("HOR"), TEXT("No lamps for two months. They went upriver to Tarsenis to sell fish and stopped coming home. Now nobody's left to light anything."));
	Say(TEXT("ALEXIOS"), TEXT("You know Tarsenis?"));
	Say(TEXT("HOR"), TEXT("I took the grain barge crew there in Paopi. Four men. I came back for them three days later. The barge was tied up at the landing, full of grain, with bread on the deck. Half a loaf in each place, like they'd been called away from the meal. Nobody on board."));
	Do([this]() { Loop(TEXT("voices"), TEXT("whisper"), 0.7f, 3.f); Hurt(45.f); });
	Wait(2.0f);
	Say(TEXT("VOICES IN THE REEDS"), TEXT("Pamoun. Pamoun. The table is laid."), true);
	Do([this]() { KnotsTotal = 0; });
	Until([this]() { return KnotsTotal >= 3; }, TEXT("Hold Q to take up the prayer rope. Press Space as the ring meets the knots."));
	Do([this]() { StopLoop(TEXT("voices"), 3.f); bPraying = false; });
	Say(TEXT("ALEXIOS"), TEXT("Lord Jesus Christ, Son of God, have mercy on me."));
	Say(TEXT("HOR"), TEXT("You hear them too. Good. Pray louder, Father. They hate a loud man."));
	Narrate(this, TEXT("The canal narrows. A landing stone rises out of the water, worn smooth, with a mooring ring rusted orange. No smoke. No lamps. Far to the west, on a ridge of old tombs, a few small lights are burning."));
	Say(TEXT("HOR"), TEXT("This is as far as I go."));
	Say(TEXT("ALEXIOS"), TEXT("The landing is right there."));
	Say(TEXT("HOR"), TEXT("And I'm right here, and I mean to stay here. Seven days, the old man said? I'll come back on the seventh day. If you're on this stone, I'll take you home. If you aren't, I'll leave a lamp burning on it and go."));
	FadeTo(1.f, 1.2f);
	Do([this]() {
		if (Priest)
		{
			Priest->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Priest->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			GoTo(FVector(3020.f, 800.f, -30.f), 180.f, EVLight::Dusk, false);
		}
		Set("boat_leaving");
		if (Rig) { Rig->Follow(); Rig->Snap(); }
		StopLoop(TEXT("amb"), 4.f);
	});
	FadeTo(0.f, 2.f);
	Wait(3.0f);
	Card(TEXT("ORTHODOX VIGIL"), TEXT(""), 4.f);
	Card(TEXT("The First Night"), TEXT("14 November"), 3.5f);
	Do([this]() {
		Chapter = TEXT("village");
		ChapterTime = 0.f;
		SetLighting(EVLight::Night);
		PlaceText = TEXT("Tarsenis. 14 November, AD 543. After dark.");
		Loop(TEXT("amb"), TEXT("amb_night"), 0.7f, 3.f);
		StopLoop(TEXT("amb2"), 2.f);
		Objective(TEXT("Walk up into the village."));
		Hint(TEXT("WASD to walk, Shift to hurry. E to look at things."), 7.f);
	});
}

// ================================================================ interactions

static void BesaMenu(AVigilGameMode* GM);

void AVigilGameMode::OnInteract(const FString& Id)
{
	const FVector B = ChurchB(this);

	// ------------------------------------------------ I-1 empty streets
	if (Id.StartsWith(TEXT("door_")))
	{
		Play(TEXT("knock"), 0.8f);
		if (Id == TEXT("door_0") && Has("well_cleansed"))
		{
			Narrate(this, TEXT("Thecla's door stays shut. From inside, very softly, the boy is still humming."));
			return;
		}
		Wait(1.4f);
		if (!Has("closed_eye"))
		{
			Set("closed_eye");
			Narrate(this, TEXT("No one answers. On the lintel someone has drawn a sign in ash: a circle with one vertical stroke through it, like an eye closed from top to bottom."));
			Do([this]() { Journal(TEXT("The closed eye"), TEXT("A sign in ash on every lintel: a circle, one stroke down through it, like an eye closed from top to bottom.")); });
		}
		else
		{
			static int32 K = 0;
			const TCHAR* Lines[] = { TEXT("No one answers. Someone inside holds their breath."), TEXT("Nothing. The house listens back."), TEXT("A shutter creaks somewhere, and then nothing.") };
			Narrate(this, Lines[(K++) % 3]);
		}
		return;
	}
	if (Id.StartsWith(TEXT("bread_")))
	{
		if (!Has("half_loaves"))
		{
			Set("half_loaves");
			Narrate(this, TEXT("Half a loaf of bread on the threshold, uneaten. There is one on almost every doorstep."));
			Do([this]() { Journal(TEXT("Half loaves"), TEXT("No dogs. No children. A goat tethered by the dovecote watched me pass and made no sound at all. Somebody bakes here, because there is bread on every doorstep, and nobody eats it.")); });
		}
		else Narrate(this, TEXT("Another half loaf. Nobody has touched it."));
		return;
	}
	if (Id == TEXT("goat")) { Narrate(this, TEXT("A goat tethered by the dovecote watches him pass and makes no sound at all.")); return; }
	if (Id == TEXT("dovecote")) { Narrate(this, TEXT("A dovecote with no doves.")); return; }
	if (Id == TEXT("laundry")) { Narrate(this, TEXT("Laundry still hangs on a line. Whoever hung it has not come back for it.")); return; }

	// ------------------------------------------------ I-2 the well
	if (Id == TEXT("well"))
	{
		if (!Has("met_hooded_man")) { Narrate(this, TEXT("The village well, under a palm-trunk frame. A rope and bucket over a stone lip worn by generations of hands.")); return; }
		if (Has("well_cleansed")) { Narrate(this, TEXT("The water is still now. Something scraped on stone down there, and a small light went out. He cannot reach it from here.")); return; }
		Enable(TEXT("well"), false);
		Do([this]() { Play(TEXT("rope_creak"), 0.8f); ShowProp(this, TEXT("bucket"), false); StopLoop(TEXT("amb"), 1.5f); });
		Wait(2.2f);
		Do([this]() { Loop(TEXT("chant"), TEXT("chant_reverse"), 0.75f, 2.5f); Sub(TEXT("VOICES FROM THE WELL"), TEXT("Us on mercy have... Immortal Holy... Mighty Holy..."), 6.f); });
		Wait(3.0f);
		Do([this]() {
			SetLighting(EVLight::Vision);
			for (const TCHAR* N : { TEXT("refl_alexios"), TEXT("refl_bright") }) if (AVigilFigure* F = NPC(N)) { F->SetActorHiddenInGame(false); F->SetFade(1.f); }
			LightOn(this, TEXT("vision"), 0.35f);
			if (Rig) { Rig->Fixed(VIS + FVector(0, 0, -40), FRotator(-89.f, 45.f, 0), 52.f, 4.f); Rig->Snap(); }
			Shot(TEXT("well_reflection"));
		});
		Narrate(this, TEXT("In the dark water, Alexios's reflection looks up at him. Behind the reflection stands a tall figure in white. Its light falls on nothing around it."));
		Choose({ { TEXT("Turn around"), [this]() {
			SetLighting(EVLight::Night);
			if (Rig) { Rig->Follow(); Rig->Snap(); }
			Wait(1.4f);
			Narrate(this, TEXT("The square is empty."));
			Choose({ { TEXT("Look back into the well"), [this]() {
				SetLighting(EVLight::Vision);
				if (Rig) { Rig->Fixed(VIS + FVector(0, 0, -40), FRotator(-89.f, 45.f, 0), 52.f, 4.f); Rig->Snap(); }
				if (AVigilFigure* Bf = NPC(TEXT("refl_bright"))) Bf->SetActorLocation(VIS + FVector(16, 12, -480));
				Play(TEXT("sting"), 0.7f);
				Hurt(30.f);
				Shake(0.6f);
				Wait(1.2f);
				Narrate(this, TEXT("The figure has moved closer to the reflection's shoulder."));
				Do([this]() { Set("await_sprinkle"); });
				Until([this]() { return Has("sprinkled"); }, TEXT("Press G to use the Holy Water."), true);
				Say(TEXT("ALEXIOS"), TEXT("In the name of the Father, and of the Son, and of the Holy Spirit."));
				Do([this]() { StopLoop(TEXT("chant"), 0.08f); Set("vision_dissolve"); Play(TEXT("flash"), 0.5f, 1.4f); });
				Wait(1.6f);
				Do([this]() { LightOn(this, TEXT("vision_wall"), 0.6f); Play(TEXT("scrape_stone"), 0.6f); });
				Wait(0.9f);
				Do([this]() { LightOn(this, TEXT("vision_wall"), 0.f); });
				Narrate(this, TEXT("The singing cuts off. The figure in the water breaks apart like oil. For one moment a second, smaller light glows in the well wall below the waterline, then goes out. Something scrapes on stone."));
				Do([this]() {
					Set("well_cleansed");
					Flags.Remove("sprinkled");
					LightOn(this, TEXT("vision"), 0.f);
					for (const TCHAR* N : { TEXT("refl_alexios"), TEXT("refl_bright") }) if (AVigilFigure* F = NPC(N)) F->SetActorHiddenInGame(true);
					SetLighting(EVLight::Night);
					if (Rig) { Rig->Follow(); Rig->Snap(); }
					Loop(TEXT("amb"), TEXT("amb_night"), 0.7f, 2.f);
					// Thecla runs out
					Tween(Named[TEXT("door_0")], Named[TEXT("door_0")]->GetActorRotation().Yaw + 100.f, 0.35f);
					Play(TEXT("door_bang"), 0.9f);
					if (AVigilFigure* Th = NPC(TEXT("thecla")))
					{
						Th->SetActorHiddenInGame(false);
						Th->WalkTo(Priest->GetActorLocation() + (Th->GetActorLocation() - Priest->GetActorLocation()).GetSafeNormal() * 90.f, 360.f);
					}
				});
				Until([this]() { AVigilFigure* Th = NPC(TEXT("thecla")); return !Th || Th->Arrived(); }, FString(), true);
				Do([this]() { if (AVigilFigure* Th = NPC(TEXT("thecla"))) { Th->FacePoint(Priest->GetActorLocation()); Priest->FacePoint(Th->GetActorLocation()); } });
				Say(TEXT("THECLA"), TEXT("No! No words after the sun."), true);
				Say(TEXT("THECLA"), TEXT("Quiet at night. They hear."));
				Say(TEXT("ALEXIOS"), TEXT("Who hears?"));
				Narrate(this, TEXT("She presses a finger to his lips like a mother with a child, and pulls him under her doorway."));
				Do([this]() {
					if (AVigilFigure* Th = NPC(TEXT("thecla"))) Th->WalkTo(M(9.0f, -5.4f) + FVector(0, 0, Th->GetActorLocation().Z), 200.f);
					Priest->WalkTo(M(8.6f, -4.4f), 200.f);
					if (AVigilFigure* Pk = NPC(TEXT("pakhom"))) { Pk->SetActorHiddenInGame(false); Pk->FacePoint(M(8.6f, -4.4f), true); }
				});
				Wait(2.2f);
				Do([this]() { if (AVigilFigure* Th = NPC(TEXT("thecla"))) { Th->FacePoint(Priest->GetActorLocation()); Priest->FacePoint(Th->GetActorLocation()); } Shot(TEXT("thecla")); });
				Narrate(this, TEXT("Over her shoulder, a boy of about nine watches. His hair is wet. He doesn't blink."));
				Do([this]() { if (AVigilFigure* Th = NPC(TEXT("thecla"))) { Th->Pose.Point = 1.f; Th->FaceYaw(20.f); } });
				Say(TEXT("THECLA"), TEXT("Besa. Go to Besa. And thank you, Father. Thank you."));
				Do([this]() {
					if (AVigilFigure* Th = NPC(TEXT("thecla"))) { Th->Pose.Point = 0.f; Th->SetActorHiddenInGame(true); }
					if (AVigilFigure* Pk = NPC(TEXT("pakhom"))) Pk->SetActorHiddenInGame(true);
					Tween(Named[TEXT("door_0")], Named[TEXT("door_0")]->GetActorRotation().Yaw - 100.f, 0.5f);
					Play(TEXT("door_bang"), 0.4f, 1.2f);
				});
				Wait(1.0f);
				Do([this]() { PlayAt(TEXT("hum_child"), M(10.0f, -5.6f, 1.2f), 0.9f); Sub(TEXT(""), TEXT("From inside, very softly, the boy begins to hum the same reversed melody."), 5.f); Objective(TEXT("Find Besa in his boat shed by the canal.")); });
			} } });
		} } });
		return;
	}

	// ------------------------------------------------ I-3 the locked church
	if (Id == TEXT("church_door"))
	{
		Play(TEXT("chain"), 0.8f);
		Narrate(this, TEXT("The doors are chained shut from the outside with a padlock, and the closed eye is drawn on the door in ash."));
		Set("saw_chain");
		return;
	}
	if (Id == TEXT("church_window"))
	{
		Do([this, B]() {
			LightOn(this, TEXT("templon_glow"), 0.5f);
			if (Rig) { Rig->Fixed(B + FVector(220, 360, 380), (B + FVector(-215, 0, 140) - (B + FVector(220, 360, 380))).Rotation(), 45.f, 4.f); Rig->Snap(); }
			Shot(TEXT("lattice"));
		});
		Narrate(this, TEXT("Through the high lattice he can see the icons on the screen inside, every one turned to face the wall."));
		Do([this]() {
			LightOn(this, TEXT("templon_glow"), 0.f);
			if (Rig) { Rig->Follow(); Rig->Snap(); }
			Journal(TEXT("The locked church"), TEXT("Someone locked the church from outside. Someone turned the saints away from the people. And someone took the semantron, so nobody could call the village to prayer. I want to believe it was not Ioannes. I can't think who else had the keys."));
			Set("church_seen");
		});
		return;
	}
	if (Id == TEXT("semantron_frame"))
	{
		Narrate(this, TEXT("A wooden bell-frame by the door, for the semantron. The frame is empty."));
		return;
	}

	// ------------------------------------------------ I-4 Besa (also triggered by walking into the shed)
	if (Id == TEXT("shed")) { OnZone(TEXT("shed"), true); return; }

	// ------------------------------------------------ I-5 the bone house
	if (Id == TEXT("bonehouse"))
	{
		if (!Has("met_besa")) { Narrate(this, TEXT("A small stone hut over a stair going down into the dark. He doesn't know yet where it leads.")); return; }
		FadeTo(1.f, 1.2f);
		Do([this]() {
			GoTo(OSS + M(-1.4f, 0, 0.05f), 180.f, EVLight::Under, true);
			Chapter = TEXT("ossuary");
			ChapterTime = 0.f;
			PlaceText = TEXT("The bone house. Night 1, late.");
			Loop(TEXT("amb"), TEXT("amb_ossuary"), 0.8f, 2.f);
			if (Rig) { Rig->Follow(1450.f, -56.f, -135.f, 34.f); Rig->Snap(); }
			Objective(TEXT("Find the way up into the church."));
		});
		FadeTo(0.f, 1.5f);
		Do([this]() { Hint(TEXT("The lamp lights only a little way. The low walls are cut away so you can see."), 6.f); Shot(TEXT("ossuary")); });
		return;
	}
	if (Id == TEXT("oss_stair")) { Narrate(this, TEXT("The stair back up to the burial ground. Besa said the way into the church is from below.")); return; }
	if (Id == TEXT("skulls")) { Narrate(this, TEXT("Low vaulted passages lined with skulls, Coptic names painted on their brows. It smells of dust and old oil.")); return; }
	if (Id == TEXT("scratch"))
	{
		Narrate(this, TEXT("The wall is covered with scratched Greek letters: Kyrie eleison, Lord have mercy, hundreds of times, the later ones shaky and deep."));
		Do([this]() { Journal(TEXT("Lord have mercy"), TEXT("In the side passage, the wall is scratched with Kyrie eleison, hundreds of times. The later ones are shaky and deep. Someone wrote it until they could barely hold the nail.")); });
		return;
	}
	if (Id == TEXT("stole"))
	{
		Enable(TEXT("stole"), false);
		ShowProp(this, TEXT("stole"), false);
		Narrate(this, TEXT("A priest's stole hangs torn on an iron nail. Ioannes's. Its pocket holds a small key."));
		Do([this]() { Give("stole", TEXT("Ioannes's Stole")); Give("key", TEXT("A small key")); });
		return;
	}
	if (Id == TEXT("bones"))
	{
		Enable(TEXT("bones"), false);
		ShowProp(this, TEXT("wrapped"), false);
		Narrate(this, TEXT("Beneath a heap of bones: the missing semantron, the long wooden board and its mallet, wrapped in sacking. Someone hid it here rather than burning it."));
		Do([this]() { Set("found_semantron"); Give("semantron", TEXT("The Semantron and mallet")); });
		return;
	}
	if (Id == TEXT("oiljar"))
	{
		Enable(TEXT("oiljar"), false);
		ShowProp(this, TEXT("oiljar"), false);
		Narrate(this, TEXT("Lamp oil, in a sealed jar on a shelf."));
		Do([this]() { Give("oil", TEXT("Lamp oil")); });
		return;
	}
	if (Id == TEXT("trapdoor_up"))
	{
		if (!Holds("stole") || !Holds("semantron") || !Holds("oil"))
		{
			Narrate(this, TEXT("A stair leads up to a trapdoor. Above it: the sanctuary. But this room has not been searched yet, and someone hid things here for a reason."));
			return;
		}
		FadeTo(1.f, 1.f);
		Do([this, B]() {
			GoTo(B + FVector(-300, 140, 14), 0.f, EVLight::Night, false);
			Chapter = TEXT("church_night");
			ChapterTime = 0.f;
			PlaceText = TEXT("The Church of the Archangel Michael.");
			Loop(TEXT("amb"), TEXT("amb_church"), 0.6f, 2.f);
			LightOn(this, TEXT("chapel_fill"), 0.6f);
			if (Rig) { Rig->Follow(1700.f, -52.f, -135.f, 34.f); Rig->Snap(); }
			Objective(TEXT("Look around the church."));
			bUnderground = false;
			if (OssBright) OssBright->SetFade(0.f);
		});
		FadeTo(0.f, 1.5f);
		Do([this]() { Shot(TEXT("church_inside")); });
		Narrate(this, TEXT("Inside the church, the air is still. Every icon on the screen faces the wall. The altar is covered with a cloth embroidered with the closed eye. On it sit a bowl of ash and half a loaf of bread. The lamp before the Theotokos is dry."));
		Do([this]() {
			Journal(TEXT("The church, put away"), TEXT("Everything in the church is whole. It was put away, the way you put away a room when someone has died and you can't bear to look at their things. I will sleep here tonight beside the altar. In the morning I'll find what I need to set it right."));
			Objective(TEXT("Sleep beside the altar."));
		});
		return;
	}

	// ------------------------------------------------ inside the church (night 1 and day 2)
	if (Id.StartsWith(TEXT("icon_")))
	{
		if (Chapter == TEXT("journal") && Id == TEXT("icon_michael") && !Has("read_ioannes_journal"))
		{
			// I-7: the recess behind the Archangel
			ShowProp(this, TEXT("codex"), false);
			Narrate(this, TEXT("With the icon of the Archangel turned back, a gap shows behind its board: a recess in the wall, and in it a small codex."));
			Doc(TEXT("Ioannes's Journal"), {
				TEXT("Second week. They are grieving people and they are kind to me. The great death took so many. I ring the board every evening and some come."),
				TEXT("Second month. A man from Alexandria visits me, a scholar and a reader once, very learned. The village calls him the Shepherd. He sits with the dying when I am too tired to. He asks whether God hears the prayers of a village that buried a third of itself. I tell him yes. I find I say it more slowly each time."),
				TEXT("He says the saints' eyes frighten the grieving. God forgive me, I turned the icons to the wall. It was only for a little while, I told myself."),
				TEXT("Apollo the potter's son was \"taken up\" last night. I followed the lamps west to the tombs. I saw what they took him to. I cannot go back into that church. I hid the board where he won't think to look."),
				TEXT("If anyone comes after me: do not eat the bread. And understand this, if you understand nothing else. The evil here is one thing wearing many masks.") });
			Do([this]() { Set("read_ioannes_journal"); Give("ioannes_journal", TEXT("Ioannes's Journal")); Set("lamp_passing"); Play(TEXT("step_dirt"), 0.3f, 0.7f); });
			Narrate(this, TEXT("Alexios closes the journal. Outside, in the dark, someone is walking slowly around the church. The light of their lamp passes along the lattice windows: twice, and then it is gone."));
			Wait(7.0f);
			Do([this]() {
				Journal(TEXT("The board has spoken"), TEXT("The church is clean and the board has spoken. I should feel stronger than I do. Tomorrow I'll cross the canal and find out who answered."));
			});
			Card(TEXT("Act I ends"), TEXT("The Silent Village"), 4.f);
			Card(TEXT("The Third Day"), TEXT("16 November"), 3.5f);
			FadeTo(1.f, 1.f);
			Do([this]() {
				// II-1, opening: the convent gate
				Chapter = TEXT("convent");
				GoTo(Ground(42.6f, -4.0f), 0.f, EVLight::Dawn, false);
				if (Priest) Priest->GetCharacterMovement()->SetMovementMode(MOVE_None);
				PlaceText = TEXT("The Convent of the Theotokos, across the canal. Morning.");
				ChapterTime = 0.f;
				LightOn(this, TEXT("convent"), 2.0f);
				if (AVigilFigure* An = NPC(TEXT("anastasia"))) { An->SetActorHiddenInGame(false); An->FaceYaw(180.f, true); }
				if (Rig) { Rig->Fixed(Ground(38.6f, 0.4f, 5.2f), (Ground(43.0f, -4.0f, 1.1f) - Ground(38.6f, 0.4f, 5.2f)).Rotation(), 34.f, 3.f); Rig->Snap(); }
				Loop(TEXT("amb"), TEXT("amb_day"), 0.5f, 2.f);
				Loop(TEXT("amb2"), TEXT("amb_water"), 0.3f, 2.f);
			});
			FadeTo(0.f, 2.f);
			Do([this]() { Shot(TEXT("convent")); });
			Narrate(this, TEXT("A nun waits at the gate with a cloth-wrapped loaf. She looks Alexios up and down."));
			Say(TEXT("ANASTASIA"), TEXT("They sent a boy."));
			Say(TEXT("ALEXIOS"), TEXT("They sent a priest."));
			Say(TEXT("ANASTASIA"), TEXT("Those can be the same thing, Father. I'm Anastasia. That was our lamp you saw. I baked this myself, so you can eat it."));
			Say(TEXT("ALEXIOS"), TEXT("Ioannes wrote that I shouldn't eat the bread."));
			Say(TEXT("ANASTASIA"), TEXT("Ioannes had more sense than I gave him credit for. We stopped taking the village bread at Pascha. There are four of us left here, and we are the only people in Tarsenis who still dream like ordinary people."));
			FadeTo(1.f, 1.5f);
			Do([this]() {
				StopLoop(TEXT("amb"), 1.f); StopLoop(TEXT("amb2"), 1.f);
				// its true form, shown for one frame
				if (AVigilFigure* Gs = NPC(TEXT("guest")))
				{
					if (Rig) { Rig->Fixed(Gs->GetActorLocation() + FVector(260, 180, 60), (Gs->GetActorLocation() + FVector(0, 0, 60) - (Gs->GetActorLocation() + FVector(260, 180, 60))).Rotation(), 40.f, 10.f); Rig->Snap(); }
					Gs->FacePoint(Gs->GetActorLocation() + FVector(260, 180, 0), true);
				}
				SetLighting(EVLight::Under);
			});
			Wait(0.4f);
			Do([this]() { LightOn(this, TEXT("guest"), 12.f); LightOn(this, TEXT("guest_fill"), 0.7f); LightOn(this, TEXT("guest_under"), 0.9f); FadeAlpha = 0.f; FadeTarget = 0.f; Play(TEXT("flash"), 1.f); Shot(TEXT("guest")); });
			Wait(bAutoPilot ? 0.25f : 0.07f);
			Do([this]() { FadeAlpha = 1.f; FadeTarget = 1.f; LightOn(this, TEXT("guest"), 0.f); LightOn(this, TEXT("guest_fill"), 0.f); LightOn(this, TEXT("guest_under"), 0.f); });
			Wait(1.5f);
			Do([this]() { Screen = EVScreen::End; MenuSel = 0; Loop(TEXT("music"), TEXT("mus_title"), 0.6f, 3.f); Set("slice_finished"); UE_LOG(LogTemp, Display, TEXT("VIGIL the slice reached its end card")); });
			return;
		}
		if (Chapter == TEXT("journal")) { Narrate(this, TEXT("The icon faces the people again.")); return; }
		Narrate(this, TEXT("Every icon on the screen is turned to face the wall. Only the backs of the boards look out at the nave."));
		return;
	}
	if (Id == TEXT("lampada")) { Narrate(this, Chapter == TEXT("journal") ? TEXT("The lamp before the Theotokos burns.") : TEXT("The lamp before the Theotokos is dry.")); return; }
	if (Id == TEXT("brazier")) { Narrate(this, TEXT("A bronze brazier, cold.")); return; }
	if (Id == TEXT("trapdoor")) { Narrate(this, TEXT("The trapdoor down to the bone house.")); return; }
	if (Id == TEXT("font")) { HolyWater = 3; Play(TEXT("sprinkle"), 0.5f, 0.8f); Hint(TEXT("The flask is full again."), 3.f); return; }
	if (Id == TEXT("altar"))
	{
		if (Chapter == TEXT("church_night"))
		{
			Narrate(this, TEXT("The defiled cloth, the bowl of ash, half a loaf of bread. He lies down beside the altar."));
			Choose({
				{ TEXT("Pray with the prayer rope before sleep"), [this]() {
					Do([this]() { KnotsTotal = 0; });
					Until([this]() { return KnotsTotal >= 4; }, TEXT("Hold Q and pray: Space as the ring meets the knots."));
					Do([this]() { Set("prayed_before_sleep"); bPraying = false; });
					Narrate(this, TEXT("He prays until the words are only breathing. It is a quiet night."));
				} },
				{ TEXT("Sleep"), [this]() {
					Do([this]() { Set("dreamed_of_table"); });
					FadeTo(1.f, 2.f);
					Do([this]() { Loop(TEXT("voices"), TEXT("whisper"), 0.4f, 1.f); });
					Narrate(this, TEXT("He dreams. Steps going down into the dark. At the bottom, a table, laid for a meal."));
					Do([this]() { StopLoop(TEXT("voices"), 1.f); Play(TEXT("sting"), 0.5f); });
				} } });
			FadeTo(1.f, 2.f);
			Do([this]() {
				Card(TEXT("The Second Day"), TEXT("15 November"), 3.5f);
				Do([this]() {
					const FVector B2 = ChurchB(this);
					Chapter = TEXT("day2");
					ChapterTime = 0.f;
					PlaceText = TEXT("Tarsenis. 15 November. Day.");
					GoTo(B2 + FVector(-280, 120, 14), 0.f, EVLight::Day, false);
					LightOn(this, TEXT("chapel_fill"), 45.0f);
					Loop(TEXT("amb"), TEXT("amb_day"), 0.4f, 2.f);
					Objective(TEXT("Gather what the rite needs: incense, lamp oil, candles, the Psalter."));
				});
				FadeTo(0.f, 2.f);
				Do([this]() { Shot(TEXT("day2")); });
				Narrate(this, TEXT("The village in daylight is no friendlier. Shutters stay closed. Faces appear and vanish behind lattices."));
			});
			return;
		}
		if (Chapter == TEXT("day2"))
		{
			if (Holds("incense") && Holds("oil") && Holds("candles") && Holds("psalter"))
			{
				Choose({ { TEXT("Wait for sunset and begin the rite"), [this]() { BeginRite(); } }, { TEXT("Not yet"), []() {} } });
			}
			else
			{
				TArray<FString> Need;
				if (!Holds("incense")) Need.Add(TEXT("incense"));
				if (!Holds("oil")) Need.Add(TEXT("lamp oil"));
				if (!Holds("candles")) Need.Add(TEXT("candles"));
				if (!Holds("psalter")) Need.Add(TEXT("the Psalter"));
				Narrate(this, FString::Printf(TEXT("The rite cannot begin yet. Still needed: %s."), *FString::Join(Need, TEXT(", "))));
			}
			return;
		}
		Narrate(this, TEXT("The altar."));
		return;
	}
	if (Id == TEXT("chest"))
	{
		if (!Holds("key")) { Narrate(this, TEXT("The sacristy chest is locked.")); return; }
		if (Holds("incense")) { Narrate(this, TEXT("The chest is empty now but for vestments folded away.")); return; }
		Play(TEXT("scrape_wood"), 0.5f, 1.5f);
		Narrate(this, TEXT("Ioannes's key turns. Inside, folded vestments, and a box of incense."));
		Do([this]() { Give("incense", TEXT("Incense")); });
		return;
	}
	if (Id == TEXT("psalter"))
	{
		if (Holds("psalter")) return;
		ShowProp(this, TEXT("psalter"), false);
		Enable(TEXT("psalter"), false);
		Narrate(this, TEXT("The Psalter, on the reading stand."));
		Do([this]() { Give("psalter", TEXT("The Psalter")); });
		return;
	}
	if (Id == TEXT("church_exit"))
	{
		if (Chapter == TEXT("church_night")) { Narrate(this, TEXT("Chained from outside. He came in the only way there was.")); return; }
		if (Has("doors_open")) { Narrate(this, TEXT("The doors stand open.")); return; }
		// ADDED: how the doors come open on day 2 (the script leaves this open)
		Play(TEXT("chain"), 0.9f);
		Narrate(this, TEXT("Outside, someone works at the chain. It falls away. When he opens the doors there is no one there."));
		Do([this]() {
			Set("doors_open");
			SetBlocker(TEXT("church_door"), false);
			ShowProp(this, TEXT("chain"), false);
			ShowProp(this, TEXT("church_sign"), false);
			Tween(Named[TEXT("church_door_a")], 95.f, 1.2f);
			Tween(Named[TEXT("church_door_b")], -95.f, 1.2f);
			Play(TEXT("scrape_wood"), 0.6f, 0.7f);
			const FVector B2 = ChurchB(this);
			AVigilInteract* Cd = Usable(TEXT("candles"), TEXT("Take the candles"), B2 + FVector(720, 60, 0), TEXT("SM_Basket"), 0, 0.5f, 120.f);
			Cd->Height = 60.f;
			Cd->AddMesh(Mesh(TEXT("SM_Candle")), FVector(0, 0, 10), FRotator(0, 0, 80), FVector(3.f));
			Cd->AddMesh(Mesh(TEXT("SM_Candle")), FVector(5, 6, 12), FRotator(0, 20, 80), FVector(3.f));
			Enable(TEXT("church_door"), false);
		});
		return;
	}
	if (Id == TEXT("candles"))
	{
		if (AVigilInteract* Cd = FindUsable(TEXT("candles"))) { Cd->bEnabled = false; Cd->SetActorHiddenInGame(true); }
		Narrate(this, TEXT("On the step: candles, wrapped in cloth, left with no note. Thecla, perhaps."));
		Do([this]() { Give("candles", TEXT("Candles")); });
		return;
	}
}

// ================================================================ I-4 Besa

static void BesaMenu(AVigilGameMode* GM)
{
	GM->Choose({
		{ TEXT("Ask about the bread"), [GM]() {
			// ADDED: the script lists this question but does not write the answer
			GM->Say(TEXT("BESA"), TEXT("Sarapion bakes it and the Shepherd pays for it. Every taking leaves half a loaf on the step. I don't eat it. I haven't slept well since, and I'm glad of it."));
			GM->Do([GM]() { GM->Journal(TEXT("Besa's bread"), TEXT("Besa will not eat the village bread. He says every taking leaves half a loaf on the step.")); BesaMenu(GM); });
		}, FName("besa_q_bread") },
		{ TEXT("Ask about the bright ones"), [GM]() {
			// ADDED
			GM->Say(TEXT("BESA"), TEXT("Tall. White, like a burial cloth that's been through a fire. They carry light, but it doesn't touch anything. You see them in the lanes after dark, and then you don't."));
			GM->Do([GM]() { GM->Journal(TEXT("The bright ones"), TEXT("Tall, robed in scorched white. Their light touches nothing. They come for whoever makes noise after dark.")); BesaMenu(GM); });
		}, FName("besa_q_bright") },
		{ TEXT("Ask about Ioannes"), [GM]() {
			// ADDED
			GM->Say(TEXT("BESA"), TEXT("At the end he was frightened. He walked round and round the church at night with his lamp. Then one night he went down behind it and didn't come up."));
			GM->Do([GM]() { BesaMenu(GM); });
		}, FName("besa_q_ioannes") },
		{ TEXT("Ask about the tombs"), [GM]() {
			// ADDED (the script says only that Besa tells of the lamps)
			GM->Say(TEXT("BESA"), TEXT("The old tombs on the ridge to the west, from before the saints. Nobody's lived up there since the fever. But there are lamps up there at night. Small ones. Moving."));
			GM->Do([GM]() { GM->Journal(TEXT("Lamps in the western tombs"), TEXT("Besa has seen lamps at night in the old tombs on the western ridge, where nobody lives.")); BesaMenu(GM); });
		}, FName("besa_q_tombs") },
		{ TEXT("\"I need to get into the church.\""), [GM]() {
			GM->Say(TEXT("ALEXIOS"), TEXT("I need to get into the church."));
			GM->Say(TEXT("BESA"), TEXT("Then go the way Ioannes went. Behind the church there's the old burial ground, and under the sanctuary the bone house. There's a passage up into the church from below. I watched him go down there one night. I never saw him come out."));
			GM->Say(TEXT("BESA"), TEXT("Father. Don't speak out there. And if you hear Petbe singing, it isn't him."));
			GM->Do([GM]() {
				GM->Set("met_besa");
				GM->Objective(TEXT("Find the bone house in the old burial ground behind the church."));
				if (AVigilFigure* Bs = GM->NPC(TEXT("besa"))) { Bs->WalkTo(M(23.6f, -14.8f), 120.f); }
			});
		} } });
}

void AVigilGameMode::OnZone(const FString& Id, bool bInside)
{
	if (Id == TEXT("shed") && bInside && !Has("besa_met_started") && (Has("well_cleansed") || Has("met_hooded_man")))
	{
		Set("besa_met_started");
		Do([this]() {
			if (AVigilFigure* Bs = NPC(TEXT("besa")))
			{
				Bs->Pose.Lunge = 1.f;
				Bs->WalkTo(Priest->GetActorLocation() + (Bs->GetActorLocation() - Priest->GetActorLocation()).GetSafeNormal() * 85.f, 480.f);
			}
			Play(TEXT("whoosh"), 0.9f); Play(TEXT("sting"), 0.7f); Shake(0.8f); Hurt(15.f);
			LightOn(this, TEXT("shed"), 0.6f);
		});
		Wait(0.8f);
		Do([this]() { if (AVigilFigure* Bs = NPC(TEXT("besa"))) { Bs->FacePoint(Priest->GetActorLocation()); Priest->FacePoint(Bs->GetActorLocation()); } Shot(TEXT("besa")); });
		Narrate(this, TEXT("A reed-and-timber boat shed. Nets and floats, a lamp turned down to a bead. A man lunges out of the dark with a gutting knife."));
		Say(TEXT("BESA"), TEXT("One of his? Did he send you?"));
		Do([this]() { Set("cross_tutorial"); });
		Until([this]() { return bCrossUp; }, TEXT("Hold F to raise the cross."), true);
		Say(TEXT("ALEXIOS"), TEXT("The Patriarch sent me."));
		Do([this]() {
			Flags.Remove("cross_tutorial");
			bCrossUp = false;
			StopLoop(TEXT("cross"), 0.4f);
			if (AVigilFigure* Bs = NPC(TEXT("besa"))) { Bs->Pose.Lunge = 0.f; Bs->ClearHeld(); }
		});
		Narrate(this, TEXT("Besa sees the cross and lowers the knife slowly. He's in his twenties, broad, exhausted."));
		Say(TEXT("BESA"), TEXT("A priest. Another one. The last one didn't do us much good."));
		Say(TEXT("ALEXIOS"), TEXT("Father Ioannes. Where is he?"));
		Say(TEXT("BESA"), TEXT("Gone at the end of the flood. When he first came he rang the board every evening, loud enough to wake the dead. Then the Shepherd started visiting him. Then the board stopped. Then the church was locked from the outside, and your Ioannes was gone."));
		Say(TEXT("ALEXIOS"), TEXT("Who is the Shepherd?"));
		Say(TEXT("BESA"), TEXT("Ask anyone in Tarsenis and they'll say he's the best man in the delta. He fed us after the great death. He sits with the dying, and he brought Thecla's boy back from the river. Right now he's off in the desert praying for us, they say."));
		Say(TEXT("ALEXIOS"), TEXT("And the silence?"));
		Say(TEXT("BESA"), TEXT("His rule. The bright ones come for whoever makes noise after dark. Speak after sunset and you're chosen. My brother Petbe used to sing to his nets. At the last new moon he sang one night. In the morning his mat was empty and there was half a loaf on our step. That's the Shepherd's gift. \"A blessing for your loss.\""));
		Do([this]() { BesaMenu(this); });
		return;
	}
	if (Id == TEXT("oss_encounter") && bInside && OssState == 0 && Chapter == TEXT("ossuary"))
	{
		OssState = 1;
		Do([this]() {
			if (OssBright) { OssBright->SetActorLocation(OSS + M(-25.5f, 0, 1.25f)); OssBright->SetFade(1.f); OssBright->FaceYaw(0.f, true); }
			LightOn(this, TEXT("oss_far"), 0.f);
			PlayAt(TEXT("bright_voice"), OSS + M(-24.f, 0, 1.6f), 1.0f);
			Play(TEXT("sting"), 0.5f);
		});
		Say(TEXT("BRIGHT ONE"), TEXT("Little priest. You came the long way round."));
		Do([this]() { Shot(TEXT("bright_one")); Hint(TEXT("Hold F to raise the cross. Back away to the side passage before the flame closes."), 8.f); });
		return;
	}
	if (Id == TEXT("church") && bInside && Chapter == TEXT("village"))
	{
		// cannot happen in Act I: the doors are chained. Kept for completeness.
	}
}

// ================================================================ the ossuary encounter (I-5)

void AVigilGameMode::OssuaryTick(float Dt)
{
	if (!OssBright || !Priest || OssState != 1) return;
	const FVector P = Priest->GetActorLocation();
	FVector Bp = OssBright->GetActorLocation();
	const FVector ToPriest = P - Bp;
	const float D = ToPriest.Size2D();
	OssBright->FacePoint(P);
	// does the raised cross face it?
	const bool bFacing = FVector::DotProduct(Priest->GetActorForwardVector(), (Bp - P).GetSafeNormal2D()) > 0.25f;
	bCrossPressed = bCrossUp && bFacing && D < 1100.f;
	if (!bCrossPressed)
	{
		const float Step = FMath::Min(D - 60.f, 135.f * Dt);
		if (Step > 0.f) Bp += ToPriest.GetSafeNormal2D() * Step;
		OssBright->SetActorLocation(FVector(Bp.X, Bp.Y, OSS.Z + 125.f));
	}
	if (D < 700.f) Hurt(4.f * Dt);
	const bool bSide = Zones.ContainsByPredicate([](AVigilZone* Z) { return Z && Z->Id == TEXT("oss_side") && Z->bInside; });
	if (bSide || Has("oss_banished"))
	{
		// it withdraws into the dark without being banished
		OssState = 2;
		bCrossPressed = false;
		Flags.Remove("oss_banished");
		Set("oss_withdrew");
		Hint(FString(), 0.f);
		Calm(10.f);
		TweenLoc(OssBright, OSS + M(-26.f, 0, 1.25f), 3.f);
		FTimerHandle Th;
		GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (OssBright) OssBright->SetFade(0.f); }), 2.2f, false);
		Sub(TEXT(""), TEXT("The bright one withdraws into the dark. It is not banished."), 4.f);
		UE_LOG(LogTemp, Display, TEXT("VIGIL ossuary: the bright one withdrew"));
		return;
	}
	if (D < 110.f || (bCrossPressed && CrossRing <= 0.f))
	{
		// it passes through him. Failure is not death.
		Flash(FLinearColor(1, 1, 1, 0.9f), 1.2f);
		Play(TEXT("flash"), 0.9f);
		Hurt(30.f);
		Shake(1.f);
		CrossRing = 1.f;
		bCrossPressed = false;
		OssBright->SetActorLocation(OSS + M(-25.5f, 0, 1.25f));
		Hint(TEXT("It passed through him. Raise the cross [F] and back away to the side passage."), 6.f);
		UE_LOG(LogTemp, Display, TEXT("VIGIL ossuary: the bright one passed through"));
	}
}

// ================================================================ I-6 the cleansing of the church

void AVigilGameMode::BeginRite()
{
	FadeTo(1.f, 1.5f);
	Do([this]() {
		const FVector B = ChurchB(this);
		Chapter = TEXT("rite");
		ChapterTime = 0.f;
		PlaceText = TEXT("Sunset.");
		SetLighting(EVLight::Sunset);
		LightOn(this, TEXT("chapel_fill"), 0.5f);
		GoTo(B + FVector(-300, 0, 14), 180.f, EVLight::Sunset, false);
		Rite = FVRite();
		Rite.bActive = true;
		Rite.Step = 1;
		for (int32 s = 0; s < 2; ++s) { ShowProp(this, s ? TEXT("candles_1") : TEXT("candles_0"), true); LightOn(this, s ? TEXT("candles_1") : TEXT("candles_0"), 1.1f); }
		Enable(TEXT("candlestand_0"), false); Enable(TEXT("candlestand_1"), false);
		if (Rig) { Rig->Follow(1250.f, -44.f, -135.f, 36.f); Rig->Snap(); }
		Loop(TEXT("amb"), TEXT("amb_church"), 0.5f, 2.f);
		Objective(TEXT("The cleansing of the church."));
		for (const FString& Id : { FString(TEXT("altar")), FString(TEXT("brazier")) }) if (AVigilInteract* I = FindUsable(Id)) I->bGlint = true;
		UE_LOG(LogTemp, Display, TEXT("VIGIL rite begins"));
	});
	FadeTo(0.f, 2.f);
	Narrate(this, TEXT("Sunset. Alexios stands at the altar."));
	Do([this]() { Shot(TEXT("rite_start")); });
}

static const TCHAR* Psalm[] = {
	TEXT("Have mercy upon me, O God, according to thy lovingkindness: according unto the multitude of thy tender mercies blot out my transgressions."),
	TEXT("Wash me throughly from mine iniquity, and cleanse me from my sin."),
	TEXT("For I acknowledge my transgressions: and my sin is ever before me."),
	TEXT("Purge me with hyssop, and I shall be clean: wash me, and I shall be whiter than snow."),
};

static void SetGlints(AVigilGameMode* GM, TArray<FString> Ids)
{
	for (AVigilInteract* I : GM->Usables) if (I) I->bGlint = Ids.Contains(I->Id);
}

// Step 5 ends when the altar and four walls are censed AND the Theotokos faces the nave, in either order.
static bool TryFinishCense(AVigilGameMode* GM)
{
	FVRite& R = GM->Rite;
	if (R.Step != 5 || R.Count < 5 || R.bIconTurning) return false;
	R.Step = 6;
	GM->Priest->Pose.Cense = 0.f;
	GM->Priest->ClearHeld();
	GM->Priest->Hold(TEXT("SM_HandLamp"), TEXT("hand_l"), FVector(4, 6, -2), FRotator(0, 90, 0));
	SetGlints(GM, { TEXT("altar") });
	GM->Hint(TEXT("Read the Letter of Blessing at the altar."), 5.f);
	UE_LOG(LogTemp, Display, TEXT("VIGIL rite: step 5 complete"));
	return true;
}

void AVigilGameMode::RiteInteract(const FString& Id)
{
	FVRite& R = Rite;
	if (R.bCandlesOut && !Id.StartsWith(TEXT("candlestand_")))
	{
		Sub(TEXT(""), TEXT("The candles are out. Relight them before going on."), 3.f);
		return;
	}
	if (Id.StartsWith(TEXT("candlestand_")))
	{
		const int32 S = Id.EndsWith(TEXT("1")) ? 1 : 0;
		LightOn(this, S ? TEXT("candles_1") : TEXT("candles_0"), 1.1f);
		ShowProp(this, S ? TEXT("candles_1") : TEXT("candles_0"), true);
		Enable(Id, false);
		Play(TEXT("pickup"), 0.4f, 0.8f);
		if (!FindUsable(TEXT("candlestand_0"))->bEnabled && !FindUsable(TEXT("candlestand_1"))->bEnabled) { R.bCandlesOut = false; Sub(TEXT(""), TEXT("The candles burn again."), 2.5f); }
		return;
	}
	if (Id == TEXT("trapdoor") && R.bVoice)
	{
		// going to the false voice breaks the step
		R.bVoice = false;
		Set("heeded_false_voice");
		Hurt(22.f);
		Play(TEXT("sting"), 0.6f);
		Sub(TEXT(""), TEXT("Nothing is below the trapdoor but the dark. The voice stops. The psalm is broken off: begin it again."), 5.f);
		R.Count = 0;
		for (bool& W : R.bWallDone) W = false;
		for (int32 w = 0; w < 4; ++w) Enable(FString::Printf(TEXT("wall_%d"), w), true);
		R.VoiceTimer = 0.f;
		return;
	}
	switch (R.Step)
	{
	case 1:
		if (Id == TEXT("altar") && !R.bClothCarried)
		{
			R.bClothCarried = true;
			ShowProp(this, TEXT("altar_cloth"), false); ShowProp(this, TEXT("ash_bowl"), false); ShowProp(this, TEXT("altar_bread"), false);
			Priest->Pose.Reach = 1.f;
			Priest->Hold(TEXT("SM_Nets"), TEXT("hand_r"), FVector(0, 8, 0), FRotator::ZeroRotator, 0.25f);
			Sub(TEXT(""), TEXT("He lifts the defiled cloth from the altar, and the ash and bread with it."), 3.f);
			SetGlints(this, { TEXT("brazier") });
		}
		else if (Id == TEXT("brazier") && R.bClothCarried)
		{
			Priest->Pose.Reach = 0.f;
			Priest->ClearHeld();
			Priest->Hold(TEXT("SM_HandLamp"), TEXT("hand_l"), FVector(4, 6, -2), FRotator(0, 90, 0));
			ShowProp(this, TEXT("brazier_fire"), true);
			LightOn(this, TEXT("brazier"), 2.5f);
			PlayAt(TEXT("fire_loop"), ChurchB(this) + FVector(200, -220, 100), 0.6f);
			Play(TEXT("flash"), 0.3f, 1.8f);
			Sub(TEXT(""), TEXT("The cloth burns in the brazier."), 3.f);
			R.Step = 2;
			SetGlints(this, { TEXT("icon_christ"), TEXT("icon_theotokos"), TEXT("icon_michael"), TEXT("icon_forerunner") });
			R.WindCooldown = 10.f;
		}
		else if (R.bClothCarried) Sub(TEXT(""), TEXT("The brazier. Burn the cloth."), 2.5f);
		else Sub(TEXT(""), TEXT("First the altar: remove the defiled cloth."), 2.5f);
		return;
	case 2:
	{
		const TCHAR* Order[] = { TEXT("icon_christ"), TEXT("icon_theotokos"), TEXT("icon_michael"), TEXT("icon_forerunner") };
		const TCHAR* Names[] = { TEXT("Christ"), TEXT("the Theotokos"), TEXT("the Archangel Michael"), TEXT("the Forerunner") };
		if (R.IconsTurned < 4 && Id == Order[R.IconsTurned])
		{
			const int32 I = R.IconsTurned;
			Priest->Pose.Reach = 1.f;
			FTimerHandle Th;
			GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Priest) Priest->Pose.Reach = 0.f; }), 1.2f, false);
			Tween(Icons[I], 0.f, 1.4f);
			Play(TEXT("scrape_wood"), 0.6f);
			Sub(TEXT(""), FString::Printf(TEXT("He turns %s back to face the nave."), Names[I]), 3.f);
			R.IconsTurned++;
			if (R.IconsTurned == 4)
			{
				R.Step = 3;
				SetGlints(this, { TEXT("lampada") });
			}
			else SetGlints(this, { Order[R.IconsTurned] });
		}
		else if (Id.StartsWith(TEXT("icon_")))
		{
			Sub(TEXT("ALEXIOS"), FString::Printf(TEXT("(quietly) No. %s first."), Names[R.IconsTurned]), 3.f);
		}
		return;
	}
	case 3:
		if (Id == TEXT("lampada"))
		{
			Priest->Pose.Reach = 1.f;
			FTimerHandle Th;
			GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Priest) Priest->Pose.Reach = 0.f; }), 1.2f, false);
			LightOn(this, TEXT("lampada"), 1.6f);
			Play(TEXT("pickup"), 0.6f, 0.7f);
			Sub(TEXT(""), TEXT("He fills the lamp before the Theotokos and lights it."), 3.f);
			Take("oil");
			R.Step = 4;
			R.Count = 0;
			for (int32 w = 0; w < 4; ++w) Enable(FString::Printf(TEXT("wall_%d"), w), true);
			SetGlints(this, { TEXT("wall_0"), TEXT("wall_1"), TEXT("wall_2"), TEXT("wall_3") });
			R.bWatcher = true;
			if (AVigilFigure* Wt = NPC(TEXT("watcher"))) Wt->SetFade(1.f);
			Hint(TEXT("Press E at each wall to read and sprinkle."), 5.f);
		}
		return;
	case 4:
		if (Id.StartsWith(TEXT("wall_")))
		{
			const int32 W = FCString::Atoi(*Id.RightChop(5));
			if (R.bWallDone[W]) return;
			R.bWallDone[W] = true;
			Enable(Id, false);
			Priest->Pose.Sprinkle = 1.f;
			FTimerHandle Th;
			GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this]() { if (Priest) Priest->Pose.Sprinkle = 0.f; }), 1.4f, false);
			Play(TEXT("sprinkle"), 0.8f);
			Sub(TEXT("ALEXIOS"), Psalm[FMath::Clamp(R.Count, 0, 3)], 6.f);
			R.Count++;
			if (R.Count == 1 && !Has("voice_done")) { R.bVoice = true; R.VoiceTimer = 3.f; }
			if (R.Count >= 4)
			{
				if (R.bVoice) { StopLoop(TEXT("voice"), 0.05f); Sub(TEXT(""), TEXT("Under the floor, the voice falls silent mid-word."), 4.f); }
				R.bVoice = false;
				Set("voice_done");
				R.Step = 5;
				R.Count = 0;
				for (bool& Wd : R.bWallDone) Wd = false;
				for (int32 w = 0; w < 4; ++w) Enable(FString::Printf(TEXT("wall_%d"), w), true);
				Priest->Hold(TEXT("SM_Censer"), TEXT("hand_r"), FVector(0, 4, -60), FRotator::ZeroRotator);
				Priest->Pose.Cense = 1.f;
				R.bIconTurning = true;
				R.IconTurn = 0.f;
				SetGlints(this, { TEXT("altar"), TEXT("wall_0"), TEXT("wall_1"), TEXT("wall_2"), TEXT("wall_3") });
			}
		}
		return;
	case 5:
		if (Id == TEXT("altar") || Id.StartsWith(TEXT("wall_")))
		{
			if (Id.StartsWith(TEXT("wall_")))
			{
				const int32 W = FCString::Atoi(*Id.RightChop(5));
				if (R.bWallDone[W]) return;
				R.bWallDone[W] = true;
				Enable(Id, false);
			}
			else if (Has("censed_altar")) { TryFinishCense(this); return; }
			else Set("censed_altar");
			Play(TEXT("censer"), 0.8f);
			R.Count++;
			Sub(TEXT(""), Id == TEXT("altar") ? TEXT("He censes the altar.") : TEXT("He censes the wall."), 2.5f);
			TArray<FString> G;
			if (!Has("censed_altar")) G.Add(TEXT("altar"));
			for (int32 w = 0; w < 4; ++w) if (!R.bWallDone[w]) G.Add(FString::Printf(TEXT("wall_%d"), w));
			if (R.bIconTurning) G.Add(TEXT("icon_theotokos"));
			SetGlints(this, G);
		}
		if (R.Count >= 5 && !TryFinishCense(this) && R.bIconTurning)
			Hint(TEXT("The icon of the Theotokos is turning back to the wall. Sprinkle it with holy water [G]."), 5.f);
		return;
	case 6:
		if (Id == TEXT("altar"))
		{
			Doc(TEXT("The Letter of Blessing"), { TEXT("By the grace of God, I give my unworthy servant Alexios, priest, my blessing to cleanse and open the house of the Archangel at Tarsenis, as though my own hands did it.") });
			Say(TEXT("ALEXIOS"), TEXT("\"By the grace of God, I give my unworthy servant Alexios, priest, my blessing to cleanse and open the house of the Archangel at Tarsenis, as though my own hands did it.\""));
			Do([this]() {
				Rite.Step = 7;
				Rite.bWatcher = false;
				if (AVigilFigure* Wt = NPC(TEXT("watcher"))) Wt->SetFade(0.f);
				SetGlints(this, { TEXT("semantron_frame") });
				Hint(TEXT("Carry the semantron out to its frame by the door."), 5.f);
			});
		}
		return;
	case 7:
		if (Id == TEXT("semantron_frame") && !Has("striking"))
		{
			ShowProp(this, TEXT("semantron_hung"), true);
			Take("semantron");
			Set("striking");
			Rite.Strikes = 0;
			Do([this]() {
				const FVector B = ChurchB(this);
				SetLighting(EVLight::Night);
				LightOn(this, TEXT("chapel_fill"), 0.f);
				if (Rig) Rig->Fixed(B + FVector(2600, 2900, 2700), (M(4, 0) - (B + FVector(2600, 2900, 2700))).Rotation(), 42.f, 0.5f);
				Narrate(this, TEXT("Step seven. Alexios sets the semantron in its frame and lifts the mallet."));
			});
			Until([this]() { return Rite.Strikes >= 12; }, TEXT("Press E to strike the board: slow at first, then faster."), true);
			Do([this]() {
				Flags.Remove("striking");
				Shot(TEXT("semantron"));
				LightOn(this, TEXT("convent"), 0.f);
			});
			Narrate(this, TEXT("The board's knock rolls down the empty streets: slow at first, then faster, the old rhythm that calls a village to prayer."));
			Do([this]() { Play(TEXT("scrape_wood"), 0.4f, 1.3f); if (AVigilProp* H = Named.FindRef(TEXT("house_thecla"))) { Lights.Add(TEXT("window"), Lamp(H, H->GetActorLocation() + FVector(0, 0, 220), 1.2f, 500.f, FLinearColor(1, 0.6f, 0.3f), false)); } });
			Wait(1.3f);
			Do([this]() { LightOn(this, TEXT("window"), 0.f); });
			Narrate(this, TEXT("Shutters move. A lamp comes on in one window, then goes out in fear."));
			Do([this]() { LightOn(this, TEXT("convent"), 2.2f); Play(TEXT("choir"), 0.7f); });
			Wait(2.0f);
			Narrate(this, TEXT("Across the canal, on the far bank, a single lamp is lit in a small walled building, and stays lit. It's answering him."));
			Do([this]() {
				Rite.bActive = false;
				Rite.Step = 8;
				Set("church_cleansed");
				SetGlints(this, {});
				for (int32 w = 0; w < 4; ++w) Enable(FString::Printf(TEXT("wall_%d"), w), false);
				ShowProp(this, TEXT("font"), true);
				Enable(TEXT("font"), true);
				ShowProp(this, TEXT("codex"), true);
				if (Rig) { Rig->Follow(); }
				Chapter = TEXT("journal");
				ChapterTime = 0.f;
				PlaceText = TEXT("The church is the first safe room. Its font refills the Holy Water.");
				LightOn(this, TEXT("chapel_fill"), 0.4f);
				Objective(TEXT("Go back inside. Something shows behind the icon of the Archangel."));
				if (AVigilInteract* I = FindUsable(TEXT("icon_michael"))) { I->bGlint = true; I->Prompt = TEXT("Look behind the icon"); }
				UE_LOG(LogTemp, Display, TEXT("VIGIL rite complete"));
			});
		}
		return;
	}
}

void AVigilGameMode::RiteSprinkle()
{
	FVRite& R = Rite;
	if (R.bIconTurning && Priest && Icons.IsValidIndex(1))
	{
		if (FVector::Dist2D(Icons[1]->GetActorLocation(), Priest->GetActorLocation()) < 300.f)
		{
			R.bIconTurning = false;
			R.bIconDone = true;
			R.IconTurn = 0.f;
			Tween(Icons[1], 0.f, 0.8f);
			Sub(TEXT(""), TEXT("Holy water on the icon. The Theotokos faces the nave again."), 3.f);
			UE_LOG(LogTemp, Display, TEXT("VIGIL rite: the icon set right"));
			TryFinishCense(this);
			return;
		}
		Sub(TEXT(""), TEXT("Closer to the icon."), 2.f);
	}
}

void AVigilGameMode::RiteTick(float Dt)
{
	FVRite& R = Rite;
	// the voice from the trapdoor
	if (R.bVoice)
	{
		R.VoiceTimer -= Dt;
		if (R.VoiceTimer <= 0.f)
		{
			R.VoiceTimer = 7.f;
			PlayAt(TEXT("voice_call"), ChurchB(this) + FVector(-360, 210, -20), 1.f);
			Sub(TEXT("IOANNES'S VOICE"), TEXT("Alexios. Alexios, I'm down here, help me."), 4.f);
			if (AVigilInteract* T = FindUsable(TEXT("trapdoor"))) T->Prompt = TEXT("Go to the voice");
		}
	}
	else if (AVigilInteract* T = FindUsable(TEXT("trapdoor"))) T->Prompt = TEXT("The trapdoor");
	// the wind, once, during the early steps
	if (!R.bWind && !R.bCandlesOut && R.Step >= 2 && R.Step <= 4 && !Has("wind_done"))
	{
		R.WindCooldown -= Dt;
		if (R.WindCooldown <= 0.f)
		{
			R.bWind = true;
			R.WindTimer = 9.f;
			Play(TEXT("gust"), 0.9f);
			Sub(TEXT(""), TEXT("A draft with no source. The candles gutter."), 4.f);
			Hint(TEXT("Hold Q to shield the candles with prayer."), 6.f);
		}
	}
	if (R.bWind)
	{
		const float Fl = 0.4f + 0.4f * FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 9.f));
		LightOn(this, TEXT("candles_0"), 1.1f * Fl);
		LightOn(this, TEXT("candles_1"), 1.1f * Fl);
		R.WindTimer -= Dt;
		static float Shield = 0.f;
		Shield = bPraying ? Shield + Dt : FMath::Max(0.f, Shield - Dt * 0.5f);
		if (Shield > 2.5f)
		{
			R.bWind = false; Set("wind_done"); Shield = 0.f;
			LightOn(this, TEXT("candles_0"), 1.1f); LightOn(this, TEXT("candles_1"), 1.1f);
			Sub(TEXT(""), TEXT("The draft dies. The flames stand straight again."), 3.f);
		}
		else if (R.WindTimer <= 0.f)
		{
			R.bWind = false; Set("wind_done"); Shield = 0.f;
			R.bCandlesOut = true;
			Hurt(15.f);
			for (int32 s = 0; s < 2; ++s) { LightOn(this, s ? TEXT("candles_1") : TEXT("candles_0"), 0.f); ShowProp(this, s ? TEXT("candles_1") : TEXT("candles_0"), false); Enable(FString::Printf(TEXT("candlestand_%d"), s), true); }
			Sub(TEXT(""), TEXT("The candles go out."), 3.f);
			Hint(TEXT("Relight both candle stands [E]."), 6.f);
		}
	}
	// the icon of the Theotokos slowly turns back to the wall while he censes
	if (R.bIconTurning)
	{
		R.IconTurn = FMath::Min(1.f, R.IconTurn + Dt / 28.f);
		SetIconFacing(1, R.IconTurn);
		if (R.IconTurn > 0.25f && !Has("icon_hint")) { Set("icon_hint"); Play(TEXT("scrape_wood"), 0.4f, 0.6f); Hint(TEXT("The icon of the Theotokos is turning back to the wall. Sprinkle it [G]."), 6.f); }
		if (R.IconTurn >= 1.f) Hurt(4.f * Dt);
	}
	// the bright one at the lattice window: looking at it drains composure
	if (R.bWatcher && Priest)
	{
		if (AVigilFigure* Wt = NPC(TEXT("watcher")))
		{
			const FVector To = (Wt->GetActorLocation() - Priest->GetActorLocation());
			const bool bLooking = FVector::DotProduct(Priest->GetActorForwardVector(), To.GetSafeNormal2D()) > 0.8f && To.Size2D() < 1200.f;
			if (bLooking)
			{
				Hurt(7.f * Dt);
				if (!Has("watcher_hint")) { Set("watcher_hint"); Sub(TEXT(""), TEXT("A bright one presses its palm flat against the lattice window and watches. It doesn't enter."), 5.f); Hint(TEXT("Don't look at the window."), 5.f); }
			}
		}
	}
}

// ================================================================ per-frame story beats

void AVigilGameMode::StoryTick(float Dt)
{
	// tweens
	for (int32 i = GTweens.Num() - 1; i >= 0; --i)
	{
		FTw& T = GTweens[i];
		if (!T.A.IsValid()) { GTweens.RemoveAt(i); continue; }
		T.T += Dt;
		const float A = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T.T / T.Dur, 0.f, 1.f));
		FRotator R = T.A->GetActorRotation();
		R.Yaw = FMath::Lerp(T.Y0, T.Y1, A);
		T.A->SetActorRotation(R);
		if (T.bLoc) T.A->SetActorLocation(FMath::Lerp(T.L0, T.L1, A));
		if (T.T >= T.Dur) GTweens.RemoveAt(i);
	}
	// Hor's boat leaves north up the canal
	if (Has("boat_leaving"))
	{
		if (AVigilProp* B = Named.FindRef(TEXT("felucca")))
		{
			B->AddActorWorldOffset(FVector(0, -110.f * Dt, 0));
			if (B->GetActorLocation().Y < -6000.f) { Flags.Remove("boat_leaving"); B->SetActorHiddenInGame(true); if (AVigilFigure* H = NPC(TEXT("hor"))) H->SetActorHiddenInGame(true); }
		}
	}
	// the well vision breaks apart like oil
	if (Has("vision_dissolve"))
	{
		if (AVigilFigure* Bf = NPC(TEXT("refl_bright")))
		{
			const float F = FMath::Max(0.f, Bf->Fade - Dt * 1.2f);
			Bf->SetFade(F);
			Bf->SetActorScale3D(FVector(1.25f + (1.f - F) * 0.6f, 1.25f - (1.f - F) * 0.4f, 1.25f));
			if (F <= 0.f) Flags.Remove("vision_dissolve");
		}
	}
	// the lamp that passes along the lattice windows, twice
	if (Has("lamp_passing"))
	{
		static float T = 0.f;
		T += Dt;
		const FVector B = ChurchB(this);
		if (!Lights.Contains(TEXT("passing")))
		{
			AVigilProp* P = Prop(FString(), B, 0, 1, false, false);
			Lights.Add(TEXT("passing"), Lamp(P, B, 2.5f, 700.f, FLinearColor(1.f, 0.6f, 0.3f), true));
		}
		const float U = FMath::Fmod(T / 3.f, 1.f);
		Lights[TEXT("passing")]->SetWorldLocation(B + FVector(FMath::Lerp(-500.f, 500.f, U), 420.f, 320.f));
		if (T > 6.f) { Lights[TEXT("passing")]->SetIntensity(0.f); Flags.Remove("lamp_passing"); T = 0.f; }
	}

	if (Chapter != TEXT("village") || !Priest || Locked()) return;

	// I-1: the hooded man comes to the square once Alexios has looked around, or reaches the well
	if (!Has("met_hooded_man") && !Has("hooded_coming"))
	{
		const bool bClues = Has("closed_eye") && Has("half_loaves");
		const bool bSquare = FVector::Dist2D(Priest->GetActorLocation(), M(6, 0)) < 900.f && ChapterTime > 25.f;
		if (bClues || bSquare)
		{
			Set("hooded_coming");
			AVigilFigure* Hd = NPC(TEXT("hooded"));
			if (!Hd) { Set("met_hooded_man"); return; }
			Hd->SetActorHiddenInGame(false);
			Hd->SetActorLocation(Ground(14.0f, 16.0f, 0.95f));
			const FVector Meet = Priest->GetActorLocation() + (Hd->GetActorLocation() - Priest->GetActorLocation()).GetSafeNormal2D() * 130.f;
			Hd->WalkTo(Meet, 120.f);
			Sub(TEXT(""), TEXT("A lamp approaches from a side street."), 4.f);
			Until([Hd]() { return Hd->Arrived(); }, FString(), true);
			Do([this, Hd]() { Hd->FacePoint(Priest->GetActorLocation()); Priest->FacePoint(Hd->GetActorLocation()); Shot(TEXT("hooded_man")); });
			Narrate(this, TEXT("Its bearer is an old man in a hooded traveller's cloak. The camera never shows his face."));
			Say(TEXT("HOODED MAN"), TEXT("A priest! God is kind. You've come up from the river."));
			Choose({
				{ TEXT("Look at his lamp"), [this]() {
					Set("noticed_crook_lamp");
					Narrate(this, TEXT("The lamp is bronze, and its handle curls like a shepherd's crook."));
					Do([this]() { Journal(TEXT("A lamp like a crook"), TEXT("The old man at dusk carried a bronze lamp whose handle curls like a shepherd's crook.")); });
					Say(TEXT("ALEXIOS"), TEXT("From Alexandria. Where is everyone?"));
				} },
				{ TEXT("\"From Alexandria. Where is everyone?\""), [this]() { Say(TEXT("ALEXIOS"), TEXT("From Alexandria. Where is everyone?")); } } });
			Say(TEXT("HOODED MAN"), TEXT("Indoors. People here keep a silence after sunset. A local custom since the great death. You'll want water after three days on that boat. The well is just there. Draw it quietly, Father."));
			Say(TEXT("ALEXIOS"), TEXT("And a bed?"));
			Say(TEXT("HOODED MAN"), TEXT("Besa the fisherman sleeps in his boat shed by the canal. He hates everyone equally, so you won't be singled out."));
			Do([this, Hd]() { Hd->WalkTo(Ground(8.0f, 17.5f), 110.f); });
			Narrate(this, TEXT("He bows and turns the corner. The lamplight fades around it. There are no footsteps."));
			Do([this, Hd]() {
				Set("met_hooded_man");
				Objective(TEXT("Draw water at the well."));
				FTimerHandle Th;
				GetWorldTimerManager().SetTimer(Th, FTimerDelegate::CreateWeakLambda(this, [this, Hd]() { if (Hd) { Hd->SetActorHiddenInGame(true); Hd->Stop(); LightOn(this, TEXT("hooded"), 0.f); } }), 4.5f, false);
			});
		}
	}
}

// ================================================================ dev: jump straight to a chapter

void AVigilGameMode::JumpTo(const FString& Where)
{
	Screen = EVScreen::Playing;
	StopLoop(TEXT("music"), 0.5f);
	FadeAlpha = FadeTarget = 0.f;
	bHasCross = bHasFlask = bHasRope = true;
	for (const TCHAR* F : { TEXT("met_hooded_man"), TEXT("well_cleansed"), TEXT("met_besa"), TEXT("closed_eye"), TEXT("half_loaves") }) Set(F);
	const FVector B = ChurchB(this);
	ChapterTime = 0.f;
	if (Rig) Rig->Follow();
	if (Where == TEXT("ossuary"))
	{
		Chapter = TEXT("ossuary");
		GoTo(OSS + M(-6.0f, 0, 0.05f), 180.f, EVLight::Under, true);
		if (Rig) Rig->Follow(1450.f, -56.f, -135.f, 34.f);
	}
	else if (Where == TEXT("chamber"))
	{
		Chapter = TEXT("ossuary");
		OssState = 2;
		GoTo(OSS + M(-9.0f, -9.0f, 0.05f), 0.f, EVLight::Under, true);
		if (Rig) Rig->Follow(1450.f, -56.f, -135.f, 34.f);
	}
	else if (Where == TEXT("day2"))
	{
		for (const TCHAR* I : { TEXT("stole"), TEXT("key"), TEXT("semantron"), TEXT("oil") }) Give(I, I);
		Chapter = TEXT("day2");
		GoTo(B + FVector(-280, 120, 14), 0.f, EVLight::Day, false);
		LightOn(this, TEXT("chapel_fill"), 45.f);
	}
	else if (Where == TEXT("rite"))
	{
		for (const TCHAR* I : { TEXT("stole"), TEXT("key"), TEXT("semantron"), TEXT("oil"), TEXT("incense"), TEXT("candles"), TEXT("psalter") }) Give(I, I);
		Chapter = TEXT("day2");
		BeginRite();
	}
	else if (Where == TEXT("convent"))
	{
		Chapter = TEXT("convent");
		GoTo(Ground(42.6f, -4.0f), 0.f, EVLight::Dawn, false);
		LightOn(this, TEXT("convent"), 2.0f);
		if (AVigilFigure* An = NPC(TEXT("anastasia"))) { An->SetActorHiddenInGame(false); An->FaceYaw(180.f, true); }
		if (Rig) { Rig->Fixed(Ground(38.6f, 0.4f, 5.2f), (Ground(43.0f, -4.0f, 1.1f) - Ground(38.6f, 0.4f, 5.2f)).Rotation(), 34.f, 3.f); Rig->Snap(); }
	}
	else if (Where == TEXT("well"))
	{
		Chapter = TEXT("village");
		GoTo(Ground(7.2f, 1.0f), 180.f, EVLight::Vision, false);
		for (const TCHAR* N : { TEXT("refl_alexios"), TEXT("refl_bright") }) if (AVigilFigure* F = NPC(N)) { F->SetActorHiddenInGame(false); F->SetFade(1.f); }
		LightOn(this, TEXT("vision"), 0.35f);
		if (Rig) { Rig->Fixed(VIS + FVector(0, 0, -40), FRotator(-89.f, 45.f, 0), 52.f, 4.f); Rig->Snap(); }
	}
	else if (Where == TEXT("bright"))
	{
		Chapter = TEXT("ossuary");
		OssState = 3;   // posed, not hunting
		GoTo(OSS + M(-14.0f, 0, 0.05f), 180.f, EVLight::Under, true);
		if (OssBright) { OssBright->SetActorLocation(OSS + M(-19.0f, 0, 1.25f)); OssBright->SetFade(1.f); OssBright->FaceYaw(0.f, true); }
		if (Rig) Rig->Follow(1450.f, -56.f, -135.f, 34.f);
	}
	else if (Where == TEXT("guest"))
	{
		Chapter = TEXT("end");
		SetLighting(EVLight::Under);
		if (AVigilFigure* Gs = NPC(TEXT("guest")))
		{
			if (Rig) { Rig->Fixed(Gs->GetActorLocation() + FVector(260, 180, 60), (Gs->GetActorLocation() + FVector(0, 0, 60) - (Gs->GetActorLocation() + FVector(260, 180, 60))).Rotation(), 40.f, 10.f); Rig->Snap(); }
			Gs->FacePoint(Gs->GetActorLocation() + FVector(260, 180, 0), true);
		}
		LightOn(this, TEXT("guest"), 12.f); LightOn(this, TEXT("guest_fill"), 0.7f); LightOn(this, TEXT("guest_under"), 0.9f);
	}
	else
	{
		Chapter = TEXT("village");
		GoTo(Ground(8.f, 3.f), 180.f, EVLight::Night, false);
	}
	if (Rig) Rig->Snap();
	UE_LOG(LogTemp, Display, TEXT("VIGIL jumped to %s"), *Where);
}
