// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemComponent.h"

UTargetDataUnderMouse* UTargetDataUnderMouse::CreateTargetDataUnderMouse(UGameplayAbility* OwningAbility)
{
	UTargetDataUnderMouse* MyObj = NewAbilityTask<UTargetDataUnderMouse>(OwningAbility);
	return MyObj;
}

void UTargetDataUnderMouse::Activate()
{
	// Client, もしくはListen Server側の場合はtrue
	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
	
	if (bIsLocallyControlled)
	{
		// 自分がマウス操作している→マウス位置を取得して送信
		SendMouseCursorData();
	}
	else
	{
		// 他人がマウス操作している→他人の操作データを受信待機

		const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
		const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();

		// 指定されたAbilityのDelegateを取得し、Target Data到着時の処理登録
		AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UTargetDataUnderMouse::OnTargetDataReplicatedCallback);

		// 既存Target Dataの確認・処理
		const bool bCalledDelegate = AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);

		if (!bCalledDelegate)
		{
			// データが到着していない場合は待機状態
			SetWaitingOnRemotePlayerData();
		}
	}

}

void UTargetDataUnderMouse::SendMouseCursorData()
{
	// スコープ内の処理の予測対象化
	FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());

	// TargetData作成
	FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
	
	// HitResultの取得
	APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
	FHitResult CursorHit;
	PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);

	// TargetDataにHitResultを設定
	Data->HitResult = CursorHit;

	// Target Data Handle作成
	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(Data);

	// ServerSetReplicatedTargetData実行
	AbilitySystemComponent->ServerSetReplicatedTargetData(
		GetAbilitySpecHandle(),	// Ability識別子
		GetActivationPredictionKey(),	// 元の予測キー
		DataHandle,						// 送信する実データ
		FGameplayTag(),					// タグ（用途分類）
		AbilitySystemComponent->ScopedPredictionKey		// 現在の予測キー
	);

	// Abilityのアクティブ状態、タスクが実行中かどうか、ネットワークの状態を調べる
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		// 正常ならブロードキャスト
		ValidData.Broadcast(DataHandle);
	}
}

void UTargetDataUnderMouse::OnTargetDataReplicatedCallback(
	const FGameplayAbilityTargetDataHandle& DataHandle,	// 受信したTarget Data
	FGameplayTag ActivationTag						// 関連タグ
)
{
	// 受信済みデータを削除し、処理完了を記録する
	AbilitySystemComponent.Get()->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		// 正常ならブロードキャスト
		ValidData.Broadcast(DataHandle);
	}
}
