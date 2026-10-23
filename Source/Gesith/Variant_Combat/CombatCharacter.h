// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"
#include "GesithWeaponTypes.h"
#include "CombatCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UCombatLifeBar;
class UWidgetComponent;
class AGesithCastle;
class AGesithArrow;
class UStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatCharacter, Log, All);

/**
 *  An enhanced Third Person Character with networked melee combat capabilities:
 *  - Combo attack string, hit detection runs on the server only
 *  - Guard input: hold to block with a shield, or tap to parry without one
 *  - Four weapon slots: melee weapons swing, the bow is drawn and shot
 *  - Replicated HP, damage reaction and death
 *  - Respawning
 */
UCLASS(abstract)
class ACombatCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Life bar widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;

	/** Placeholder mesh of the weapon in hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* WeaponMesh;

	/** Placeholder shield mesh, shown while a one handed weapon is in hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* ShieldMesh;

public:

	/** Number of weapon slots, on the 1-4 keys */
	static constexpr int32 WeaponSlotCount = 4;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Combo Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ComboAttackAction;

	/** Guard Input Action. Hold to block with a shield, tap to parry without one */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* GuardAction;

	/** Max amount of HP the character will have on respawn. Replicated because soldier stats change it */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CurrentHP, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MaxHP = 5.0f;

	/** Current amount of HP the character has. Only the server changes it */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_CurrentHP, Category="Damage")
	float CurrentHP = 0.0f;

	/** Life bar widget fill color */
	UPROPERTY(EditAnywhere, Category="Damage")
	FLinearColor LifeBarColor;

	/** Name of the pelvis bone, for damage ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName PelvisBoneName;

	/** Pointer to the life bar widget */
	UPROPERTY(EditAnywhere, Category="Damage")
	TObjectPtr<UCombatLifeBar> LifeBarWidget;

	/** Max amount of time that may elapse for a non-combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float AttackInputCacheTimeTolerance = 1.0f;

	/** Time at which an attack button was last pressed */
	float CachedAttackInputTime = 0.0f;

	/** If true, the character is currently playing an attack animation */
	bool bIsAttacking = false;

	/** Base damage of an attack. The weapon in hand multiplies it */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** Amount of knockback impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 250.0f;

	/** Amount of upwards impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 300.0f;

	/** AnimMontage that will play for combo attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Names of the AnimMontage sections that correspond to each stage of the combo attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Max amount of time that may elapse for a combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboInputCacheTimeTolerance = 0.45f;

	/** Index of the current stage of the melee attack combo */
	int32 ComboCount = 0;

	/** Attack animation speed. Final value will later be the attack speed stat multiplied by the weapon speed */
	UPROPERTY(EditAnywhere, Category="Melee Attack", meta = (ClampMin = 0.1, ClampMax = 3))
	float AttackSpeed = 1.0f;

	/** If true, the character carries a shield and the guard input blocks while a one handed weapon is in hand. Otherwise it parries */
	UPROPERTY(EditAnywhere, Category="Guard")
	bool bHasShield = true;

	/** Bone or socket the shield is attached to */
	UPROPERTY(EditAnywhere, Category="Guard")
	FName ShieldSocket = FName("lowerarm_l");

	/** Shield placement relative to its socket */
	UPROPERTY(EditAnywhere, Category="Guard")
	FTransform ShieldTransform;

	/** Weapon in each of the 1-4 slots */
	UPROPERTY(EditAnywhere, Category="Weapons")
	TArray<EGesithWeaponType> WeaponSlots;

	/** How each kind of weapon fights and looks */
	UPROPERTY(EditAnywhere, Category="Weapons")
	TMap<EGesithWeaponType, FGesithWeaponStats> WeaponStats;

	/** Slot of the weapon in hand. Only the server has the final say */
	UPROPERTY(ReplicatedUsing=OnRep_CurrentWeaponSlot, BlueprintReadOnly, Category="Weapons")
	int32 CurrentWeaponSlot = 0;

	/** Time it takes to put one weapon away and take out another. No attacking or guarding meanwhile */
	UPROPERTY(EditAnywhere, Category="Weapons", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float WeaponSwapTime = 0.5f;

	/** Local time at which the current weapon swap ends */
	float SwapEndTime = 0.0f;

	/** Arrows carried when full */
	UPROPERTY(EditAnywhere, Category="Weapons|Ranged", meta = (ClampMin = 0, ClampMax = 200))
	int32 MaxArrows = 30;

	/** Arrows left. Refilled inside a castle that isn't an enemy's */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Weapons|Ranged")
	int32 Arrows = 0;

	/** Arrows released before this much of the draw are not shot */
	UPROPERTY(EditAnywhere, Category="Weapons|Ranged", meta = (ClampMin = 0, ClampMax = 1))
	float MinDrawFraction = 0.3f;

	/** Arrow actor to shoot */
	UPROPERTY(EditAnywhere, Category="Weapons|Ranged")
	TSubclassOf<AGesithArrow> ArrowClass;

	/** How often a character inside a castle checks whether to refill arrows */
	UPROPERTY(EditAnywhere, Category="Weapons|Ranged", meta = (ClampMin = 0.1, ClampMax = 10, Units = "s"))
	float ArrowRefillInterval = 1.0f;

	/** Camera offset over the shoulder while a bow is in hand, so the crosshair isn't hidden behind the character */
	UPROPERTY(EditAnywhere, Category="Weapons|Ranged")
	FVector BowCameraOffset = FVector(0.0f, 60.0f, 50.0f);

	/** Camera field of view while aiming with the guard input */
	UPROPERTY(EditAnywhere, Category="Weapons|Ranged", meta = (ClampMin = 10, ClampMax = 120, Units = "deg"))
	float AimFieldOfView = 55.0f;

	/** True while the bow is being drawn */
	bool bIsDrawing = false;

	/** Local time at which the draw started */
	float DrawStartTime = 0.0f;

	/** Camera settings saved on BeginPlay, so aiming can restore them */
	FVector DefaultCameraSocketOffset = FVector::ZeroVector;
	float DefaultFieldOfView = 90.0f;

	/** Rotation setting saved on BeginPlay. Drawing a bow turns the character with the camera instead */
	bool bDefaultOrientRotationToMovement = true;

	/** True while this character's squad fights with bows. Set by the leader's F4 key */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Squad")
	bool bSquadRanged = false;

	/** Checks for arrow refills. Server only */
	FTimerHandle ArrowRefillTimer;

	/** If true, the character is holding up a shield */
	UPROPERTY(ReplicatedUsing=OnRep_IsBlocking, BlueprintReadOnly, Category="Guard")
	bool bIsBlocking = false;

	/** Fraction of damage that still goes through a block */
	UPROPERTY(EditAnywhere, Category="Guard", meta = (ClampMin = 0, ClampMax = 1))
	float BlockDamageMultiplier = 0.0f;

	/** Movement speed multiplier while blocking */
	UPROPERTY(EditAnywhere, Category="Guard", meta = (ClampMin = 0, ClampMax = 1))
	float BlockWalkSpeedMultiplier = 0.5f;

	/** Attacks coming from within this angle of the character's facing can be blocked or parried */
	UPROPERTY(EditAnywhere, Category="Guard", meta = (ClampMin = 0, ClampMax = 180, Units = "deg"))
	float GuardAngle = 60.0f;

	/** How long a parry stays active after the guard input is pressed. Generous enough to absorb network latency */
	UPROPERTY(EditAnywhere, Category="Guard|Parry", meta = (ClampMin = 0, ClampMax = 1, Units = "s"))
	float ParryWindow = 0.3f;

	/** Minimum time between parry attempts, so the input can't be spammed */
	UPROPERTY(EditAnywhere, Category="Guard|Parry", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ParryCooldown = 0.8f;

	/** How long an attacker is unable to act after being parried */
	UPROPERTY(EditAnywhere, Category="Guard|Parry", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ParryStaggerTime = 1.0f;

	/** Team this character fights for. 0 means no team: soldiers ignore it and it can hit anyone */
	UPROPERTY(ReplicatedUsing=OnRep_TeamId, BlueprintReadOnly, Category="Team")
	int32 TeamId = 0;

	/** Life bar colors for teams 1 and 2 */
	UPROPERTY(EditAnywhere, Category="Team")
	TArray<FLinearColor> TeamColors;

	/** Distance between soldiers in a spawned battle line */
	UPROPERTY(EditAnywhere, Category="Team|Testing", meta = (ClampMin = 50, ClampMax = 1000, Units = "cm"))
	float BattleLineSpacing = 150.0f;

	/** Distance between the two spawned battle lines */
	UPROPERTY(EditAnywhere, Category="Team|Testing", meta = (ClampMin = 100, ClampMax = 5000, Units = "cm"))
	float BattleLineGap = 800.0f;

	/** Testing aid: when true, every attack from the front is blocked, or parried without a shield. Server only */
	bool bDebugAutoGuard = false;

	/** Server time at which the current parry window closes */
	float ParryWindowEndTime = 0.0f;

	/** Time at which the next parry attempt is allowed */
	float NextParryTime = 0.0f;

	/** Local time at which the current stagger ends */
	float StaggerEndTime = 0.0f;

	/** Walk speed saved on BeginPlay, so it can be restored after blocking */
	float DefaultWalkSpeed = 0.0f;

	/** Camera boom length while the character is dead */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DeathCameraDistance = 400.0f;

	/** Camera boom length when the character respawns */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DefaultCameraDistance = 100.0f;

	/** Time to wait before respawning the character */
	UPROPERTY(EditAnywhere, Category="Respawn", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 3.0f;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Character respawn timer */
	FTimerHandle RespawnTimer;

	/** Copy of the mesh's transform so we can reset it after ragdoll animations */
	FTransform MeshStartingTransform;

public:
	
	/** Constructor */
	ACombatCharacter();

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for combo attack input */
	void ComboAttackPressed();

	/** Called for combo attack input released */
	void ComboAttackReleased();

	/** Called for guard input pressed */
	void GuardPressed();

	/** Called for guard input released */
	void GuardReleased();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles combo attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackStart();

	/** Handles combo attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackEnd();

	/** Handles guard pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoGuardStart();

	/** Handles guard released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoGuardEnd();

	/** Returns true if the character has run out of HP */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsDead() const { return CurrentHP <= 0.0f; }

	/** Returns true if the character was recently parried and can't act */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsStaggered() const;

	/** Returns true if the character is playing an attack animation */
	bool IsAttacking() const { return bIsAttacking; }

	/** Returns true if the character is holding up a shield */
	bool IsBlocking() const { return bIsBlocking; }

	/** Returns true if both characters are on different teams. Characters without a team are nobody's enemy */
	bool IsEnemyOf(const ACombatCharacter* Other) const;

	/** Returns true if both characters are on the same team, so they can't hurt each other */
	bool IsAllyOf(const ACombatCharacter* Other) const;

	/** Sets the team and updates the life bar color. Server only */
	void SetTeam(int32 NewTeamId);

	/** Console command: joins team 1 or 2 so soldiers fight you, or 0 to leave */
	UFUNCTION(Exec)
	void JoinTeam(int32 NewTeamId);

	/** Console command: spawns two AI battle lines in front of you, with this many soldiers on each team */
	UFUNCTION(Exec)
	void SpawnBattle(int32 SoldiersPerTeam = 5);

	/** Console command: removes every AI soldier */
	UFUNCTION(Exec)
	void ClearBattle();

	/** Console command: spawns soldiers on your team that follow your orders */
	UFUNCTION(Exec)
	void SpawnSquad(int32 SquadSize = 5);

	/** Summons the castle's soldiers to follow this character. Server only */
	void MusterFromCastle(AGesithCastle* Castle);

	/** Sends this character's soldiers from the castle back to their posts. Server only */
	void DismissToCastle(AGesithCastle* Castle);

	/** Scales HP and damage from a soldier's health and strength. 10 is a fresh recruit. Server only */
	void ApplySoldierStats(float Health, float Strength);

	/** Console command: spawns an enemy line far in front of you that charges */
	UFUNCTION(Exec)
	void SpawnEnemies(int32 EnemyCount = 5);

	/** Console command for solo testing. Toggles guarding every attack from the front without holding the input */
	UFUNCTION(Exec)
	void AutoGuard();

	/** Interrupts the current attack and stops the character from acting for a while. Server only */
	void Stagger(float Duration);

	/** Takes out the weapon in a slot, 0 to 3. Works on the owning client and the server */
	UFUNCTION(BlueprintCallable, Category="Weapons")
	void SelectWeaponSlot(int32 Slot);

	/** Returns the slot of the weapon in hand */
	int32 GetCurrentWeaponSlot() const { return CurrentWeaponSlot; }

	/** Returns the weapon in a slot, or None */
	EGesithWeaponType GetWeaponInSlot(int32 Slot) const;

	/** Returns how a kind of weapon fights. Unknown kinds fight bare handed */
	const FGesithWeaponStats& GetWeaponStats(EGesithWeaponType Type) const;

	/** Returns how the weapon in hand fights */
	const FGesithWeaponStats& GetCurrentWeaponStats() const { return GetWeaponStats(GetWeaponInSlot(CurrentWeaponSlot)); }

	/** Returns the first slot holding a ranged or a melee weapon, or -1 */
	int32 FindWeaponSlot(bool bRanged) const;

	/** Returns true while putting one weapon away and taking out another */
	bool IsSwappingWeapon() const;

	/** Returns true if the shield is on the arm and can block */
	bool HasShieldInHand() const;

	/** Returns arrows left and the most that can be carried */
	int32 GetArrows() const { return Arrows; }
	int32 GetMaxArrows() const { return MaxArrows; }

	/** Returns true while the bow is being drawn */
	bool IsDrawingBow() const { return bIsDrawing; }

	/** Returns how far the bow is drawn, from 0 to 1. 0 while not drawing */
	float GetDrawFraction() const;

	/** Returns true if this character's squad should fight with bows */
	bool IsSquadRanged() const { return bSquadRanged; }

	/** Switches this character's squad between melee and bows. Server only */
	void SetSquadRanged(bool bRanged);

	/** Lowers the bow without shooting */
	void CancelDraw();

	/** Shoots the drawn bow at a point. Server only, used by AI soldiers */
	void ReleaseBowAt(const FVector& AimPoint);

	/** Console command: turns and moves the placeholder weapon in hand on this screen only, to find a good fit */
	UFUNCTION(Exec)
	void WeaponPose(float Pitch, float Yaw, float Roll, float X = 0.0f, float Y = 0.0f, float Z = 0.0f);

	/** Console command: turns and moves the placeholder shield on this screen only */
	UFUNCTION(Exec)
	void ShieldPose(float Pitch, float Yaw, float Roll, float X = 0.0f, float Y = 0.0f, float Z = 0.0f);

protected:

	/** Resets the character's current HP to maximum */
	void ResetHP();

	/** Updates the life bar to match the current HP */
	void UpdateLifeBar();

	/** Handles combo attack input on both the owning client and the server */
	void HandleComboAttackInput();

	/** Performs a combo attack */
	void ComboAttack();

	/** Plays the combo attack montage from the start */
	void PlayComboAttackMontage();

	/** Called from a delegate when the attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Raises or lowers the shield */
	void SetBlocking(bool bNewBlocking);

	/** Slows the character down while the shield is up */
	void UpdateWalkSpeed();

	/** Opens the parry window if the cooldown allows it. Server only */
	void TryParry();

	/** Prints a combat event on screen for testing. Does nothing in shipping builds */
	void ShowCombatDebugMessage(const FString& Message, const FColor& Color) const;

	/** Prints a message on the owning player's own screen only. Does nothing in shipping builds */
	void ShowLocalMessage(const FString& Message, const FColor& Color) const;

	/** Switches to a slot after the checks passed, on whichever machine runs it */
	void ApplyWeaponSlot(int32 Slot);

	/** Returns true if the character may take out the weapon in a slot right now */
	bool CanSelectWeaponSlot(int32 Slot) const;

	/** Shows the weapon in hand and the shield, and moves the camera for the bow */
	void UpdateWeaponLook();

	/** Starts drawing the bow */
	void StartDraw();

	/** Shoots the drawn bow at a point, or cancels if it wasn't drawn far enough. Server only */
	void ReleaseDraw(const FVector& AimPoint);

	/** Makes the character turn with the camera while drawing, and back afterwards */
	void SetFaceCamera(bool bFaceCamera);

	/** Zooms the camera in for aiming. Owning player only */
	void SetAimZoom(bool bZoom);

	/** Returns the point under the crosshair */
	FVector GetCameraAimPoint() const;

	/** Refills arrows inside a castle that isn't an enemy's. Server only */
	void CheckArrowRefill();

	/** Called on clients when the weapon in hand replicates */
	UFUNCTION()
	void OnRep_CurrentWeaponSlot();

	/** Sends the weapon slot change to the server */
	UFUNCTION(Server, Reliable)
	void ServerSelectWeaponSlot(int32 Slot);

	/** Puts the owner back to the server's weapon after a refused swap */
	UFUNCTION(Client, Reliable)
	void ClientRejectWeaponSlot(int32 Slot);

	/** Tells the server the bow is being drawn */
	UFUNCTION(Server, Reliable)
	void ServerStartDraw();

	/** Tells the server where the bow was released at */
	UFUNCTION(Server, Reliable)
	void ServerReleaseDraw(const FVector_NetQuantize& AimPoint);

	/** Returns true if an attack coming from the provided actor is within the guard angle */
	bool IsFacingAttacker(const AActor* Attacker) const;

	/** Called on clients when CurrentHP replicates */
	UFUNCTION()
	void OnRep_CurrentHP();

	/** Called on clients when the blocking state replicates */
	UFUNCTION()
	void OnRep_IsBlocking();

	/** Sends the combo attack input to the server */
	UFUNCTION(Server, Reliable)
	void ServerComboAttackInput();

	/** Sends the blocking state to the server */
	UFUNCTION(Server, Reliable)
	void ServerSetBlocking(bool bNewBlocking);

	/** Sends the team change to the server */
	UFUNCTION(Server, Reliable)
	void ServerJoinTeam(int32 NewTeamId);

	/** Asks the server to spawn a battle */
	UFUNCTION(Server, Reliable)
	void ServerSpawnBattle(int32 SoldiersPerTeam);

	/** Asks the server to remove every AI soldier */
	UFUNCTION(Server, Reliable)
	void ServerClearBattle();

	/** Asks the server to spawn a squad for this character */
	UFUNCTION(Server, Reliable)
	void ServerSpawnSquad(int32 SquadSize);

	/** Asks the server to spawn enemies in front of this character */
	UFUNCTION(Server, Reliable)
	void ServerSpawnEnemies(int32 EnemyCount);

	/** Spawns soldiers behind this character that join its squad. Server only */
	TArray<ACombatCharacter*> SpawnSquadMembers(int32 SquadSize);

	/** Spawns one AI soldier on a team. Server only */
	ACombatCharacter* SpawnSoldier(const FVector& Location, const FRotator& Facing, int32 Team);

	/** Prints how many soldiers are still alive on each team */
	void ShowBattleStatus() const;

	/** Called on clients when the team replicates */
	UFUNCTION()
	void OnRep_TeamId();

	/** Sends the auto guard toggle to the server */
	UFUNCTION(Server, Reliable)
	void ServerAutoGuard();

	/** Sends a parry attempt to the server */
	UFUNCTION(Server, Reliable)
	void ServerParry();

	/** Plays the combo attack montage on other machines */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayComboAttack();

	/** Advances the combo on other machines */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpToComboSection(int32 NewComboCount);

	/** Plays the damage dealt effects everywhere */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDealtDamage(float Damage, const FVector_NetQuantize& ImpactPoint);

	/** Plays the damage reaction everywhere */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastReceivedDamage(float Damage, const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantizeNormal& DamageDirection);

	/** Plays the blocked or parried reaction everywhere */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastGuardedAttack(bool bWasParried, const FVector_NetQuantize& ImpactPoint);

	/** Interrupts the attack and starts the stagger everywhere */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStagger(float Duration);

	/** Ragdolls the character everywhere */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeath();

	/** Applies knockback on the owning client so its movement matches the server */
	UFUNCTION(Client, Reliable)
	void ClientApplyKnockback(const FVector& DamageImpulse);

	
public:

	// ~begin CombatAttacker interface

	/** Performs the collision check for an attack */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs the combo string check */
	virtual void CheckCombo() override;

	/** Unused by players, kept for the attacker interface */
	virtual void CheckChargedAttack() override;

	// ~end CombatAttacker interface

	// ~begin CombatDamageable interface

	/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	// ~end CombatDamageable interface

	/** Called from the respawn timer to destroy and re-create the character */
	void RespawnCharacter();

public:

	/** Overrides the default TakeDamage functionality */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Overrides landing to reset damage ragdoll physics */
	virtual void Landed(const FHitResult& Hit) override;

protected:

	/** Blueprint handler to play damage dealt effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void DealtDamage(float Damage, const FVector& ImpactPoint);

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);

	/** Blueprint handler to play block and parry effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void GuardedAttack(bool bWasParried, const FVector& ImpactPoint);

	/** Blueprint handler to play stagger effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void Staggered(float Duration);

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Handles input bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Handles possessed initialization */
	virtual void NotifyControllerChanged() override;

	/** Registers replicated properties */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
