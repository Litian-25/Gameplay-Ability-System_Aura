// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/AuraProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"

AAuraProjectile::AAuraProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	SetRootComponent(Sphere);

	// コリジョン設定
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);	// 重なり検知のみ
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);	// 全ての衝突チャンネルを無視
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);	// 動く物体との検知
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);	// 静止体との検知
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);	// キャラクターとの検知

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void AAuraProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(LifeSpan);

	// ループサウンド開始
	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(
		LoopingSound,
		GetRootComponent()
	);

	// Sphereがオーバーラップした時に起こすイベントを登録
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AAuraProjectile::OnSphereOverlap);
}

void AAuraProjectile::Destroyed()
{
	// OnOverlap前にDestroyされたときのフォールバック
	if (!bHit && !HasAuthority())
	{
		// 衝突サウンド再生
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ImpactSound,
			GetActorLocation(),
			FRotator::ZeroRotator
		);

		// 衝突エフェクト生成
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ImpactEffect,
			GetActorLocation()
		);

		LoopingSoundComponent->Stop();
	}

	// 親クラスのDestroyed実行
	Super::Destroyed();
}

void AAuraProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 衝突サウンド再生
	UGameplayStatics::PlaySoundAtLocation(
		this,
		ImpactSound,
		GetActorLocation(),
		FRotator::ZeroRotator
	);

	// 衝突エフェクト生成
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		ImpactEffect,
		GetActorLocation()
	);

	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
	}

	// サーバー側のみ実行
	if (HasAuthority())
	{
		Destroy();
	}
	else
	{
		bHit = true;
	}
}



