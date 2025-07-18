// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AuraProjectileSpell.h"
#include "Actor/AuraProjectile.h"
#include "Interaction/CombatInterface.h"

void UAuraProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo,
                                           const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
}

void UAuraProjectileSpell::SpawnProjectile(const FVector& ProjectileTargetLocation)
{
	const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
	if (!bIsServer) return;

	// GetAvatarActorFromActorInfo()で、この能力を使うキャラクターを取得
	const ICombatInterface* CombatInterface = Cast<ICombatInterface>(GetAvatarActorFromActorInfo());
	if (CombatInterface)
	{
		const FVector SocketLocation = CombatInterface->GetCombatSocketLocation();

		// Projectileの方向計算(Target - Socketで方向ベクトル算出)
		FRotator Rotation = (ProjectileTargetLocation - SocketLocation).Rotation();

		// Pitchを0にし、地面と平行で発射(水平発射）
		Rotation.Pitch = 0.0f;
		
		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SocketLocation);

		// 回転設定
		SpawnTransform.SetRotation(Rotation.Quaternion());
		
		// Projectileに設定や情報を持たせる
		AAuraProjectile* Projectile = GetWorld()->SpawnActorDeferred<AAuraProjectile>(
			ProjectileClass,
			SpawnTransform,
			GetAvatarActorFromActorInfo(),
			Cast<APawn>(GetAvatarActorFromActorInfo()),	// 能力を発動したキャラクター
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn		// 衝突無視で強制生成
		);

		// TODO: Give the projectile GameplayEffectSpec for causing Damage

		// Projectileの生成完了
		Projectile->FinishSpawning(SpawnTransform);
	}
}
