// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/AuraEffectActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

AAuraEffectActor::AAuraEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void AAuraEffectActor::BeginPlay()
{
	Super::BeginPlay();
}

void AAuraEffectActor::ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) 
	{
		UE_LOG(LogTemp, Warning, TEXT("TargetASC is null for actor: %s"), *TargetActor->GetName());
		return;
	}

	if (!IsValid(GameplayEffectClass)) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid GameplayEffect class"));
		return;
	}
        
	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(GameplayEffectClass, ActorLevel, EffectContextHandle);
	
	if (EffectSpecHandle.IsValid())
	{
		const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
		UE_LOG(LogTemp, Warning, TEXT("Applied effect: %s to %s"), *GameplayEffectClass->GetName(), *TargetActor->GetName());
		
		const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
		if (bIsInfinite)
		{
			ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create effect spec for: %s"), *GameplayEffectClass->GetName());
	}
}

void AAuraEffectActor::OnOverlap(AActor* TargetActor)
{
	UE_LOG(LogTemp, Warning, TEXT("=== OnOverlap Called ==="));
	UE_LOG(LogTemp, Warning, TEXT("Target Actor: %s"), *TargetActor->GetName());

	// 新しいポリシー配列構造を使用
	for (const FGameplayEffectPolicy& Policy : GameplayEffectPolicies)
	{
		if (Policy.ApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap && IsValid(Policy.GameplayEffectClass))
		{
			ApplyEffectToTarget(TargetActor, Policy.GameplayEffectClass);
		}
	}

	// 後方互換性のため、レガシー配列もサポート
	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		for (const TSubclassOf<UGameplayEffect>& EffectClass : InstantGameplayEffectClasses)
		{
			if (IsValid(EffectClass))
			{
				ApplyEffectToTarget(TargetActor, EffectClass);
			}
		}
	}

	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		for (const TSubclassOf<UGameplayEffect>& EffectClass : DurationGameplayEffectClasses)
		{
			if (IsValid(EffectClass))
			{
				ApplyEffectToTarget(TargetActor, EffectClass);
			}
		}
	}

	if (InfinityEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		for (const TSubclassOf<UGameplayEffect>& EffectClass : InfinityGameplayEffectClasses)
		{
			if (IsValid(EffectClass))
			{
				ApplyEffectToTarget(TargetActor, EffectClass);
			}
		}
	}
}

void AAuraEffectActor::OnEndOverlap(AActor* TargetActor)
{
	UE_LOG(LogTemp, Warning, TEXT("=== OnEndOverlap Called ==="));
	UE_LOG(LogTemp, Warning, TEXT("Target Actor: %s"), *TargetActor->GetName());

	// 新しいポリシー配列構造を使用
	for (const FGameplayEffectPolicy& Policy : GameplayEffectPolicies)
	{
		if (Policy.ApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap && IsValid(Policy.GameplayEffectClass))
		{
			ApplyEffectToTarget(TargetActor, Policy.GameplayEffectClass);
		}
	}

	// エフェクト削除処理（重複削除を防ぐため統合）
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!IsValid(TargetASC)) return;

	TArray<FActiveGameplayEffectHandle> HandlesToRemove;
	
	// 削除が必要かチェック（新しいポリシーまたはレガシーポリシー）
	bool bShouldRemoveEffects = false;
	
	// 新しいポリシー配列をチェック
	for (const FGameplayEffectPolicy& Policy : GameplayEffectPolicies)
	{
		if (Policy.RemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
		{
			bShouldRemoveEffects = true;
			break;
		}
	}
	
	// レガシーポリシーもチェック
	if (InfinityEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		bShouldRemoveEffects = true;
	}

	// 削除処理を一度だけ実行
	if (bShouldRemoveEffects)
	{
		for (TPair<FActiveGameplayEffectHandle, UAbilitySystemComponent*> HandlePair : ActiveEffectHandles)
		{
			if (TargetASC == HandlePair.Value)
			{
				bool bRemoved = TargetASC->RemoveActiveGameplayEffect(HandlePair.Key);
				if (bRemoved)
				{
					HandlesToRemove.Add(HandlePair.Key);
					UE_LOG(LogTemp, Warning, TEXT("Removed active effect"));
				}
			}
		}

		// 削除したハンドルをマップから除去
		for (FActiveGameplayEffectHandle& Handle : HandlesToRemove)
		{
			ActiveEffectHandles.FindAndRemoveChecked(Handle);
		}
	}
}