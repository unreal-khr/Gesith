// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"
#include "CombatCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UCombatLifeBar;
class UWidgetComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatCharacter, Log, All);

/**
 *  An enhanced Third Person Character with networked melee combat capabilities:
 *  - Combo attack string, hit detection runs on the server only
 *  - Guard input: hold to block with a shield, or tap to parry without one
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

	/** Max amount of HP the character will have on respawn */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
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

	/** Distance ahead of the character that melee attack sphere collision traces will extend */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of the sphere trace for melee attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float MeleeTraceRadius = 75.0f;

	/** Amount of damage a melee attack will deal */
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

	/** If true, the guard input blocks while held. If false, it attempts a parry when pressed */
	UPROPERTY(EditAnywhere, Category="Guard")
	bool bHasShield = true;

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

	/** Console command for solo testing. Toggles guarding every attack from the front without holding the input */
	UFUNCTION(Exec)
	void AutoGuard();

	/** Interrupts the current attack and stops the character from acting for a while. Server only */
	void Stagger(float Duration);

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
