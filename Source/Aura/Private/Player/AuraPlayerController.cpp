// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/AuraPlayerController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "Components/SplineComponent.h"
#include "Input/AuraEnhancedInputComponent.h"
#include "Interaction/EnemyInterface.h"  

AAuraPlayerController::AAuraPlayerController()
{
	bReplicates = true;

	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
}

void AAuraPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	CursorTrace();
	AutoRun();
}

void AAuraPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(AuraContext);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	// check(Subsystem);
	if (Subsystem)
	{
		Subsystem->AddMappingContext(AuraContext, 0);
	}


	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;

	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AAuraPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UAuraEnhancedInputComponent* AuraInputComponent = CastChecked<UAuraEnhancedInputComponent>(InputComponent);
	AuraInputComponent->BindAction(
		MoveAction, 
		ETriggerEvent::Triggered, 
		this, 
		&AAuraPlayerController::Move
	);

	AuraInputComponent->BindAbilityActions(
		InputConfig,
		this,
		&ThisClass::AbilityInputTagPressed,
		&ThisClass::AbilityInputTagReleased,
		&ThisClass::AbilityInputTagHeld
	);
}

void AAuraPlayerController::Move(const FInputActionValue& InputActionValue)
{
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw,0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void AAuraPlayerController::CursorTrace()
{
	FHitResult CursorHit;
	GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);

	if (!CursorHit.bBlockingHit) return;

	LastActor = ThisActor;
	ThisActor = CursorHit.GetActor();

	/**
	*A. LastActor is null & ThisActor is null
	*	- Do nothing
	*B. LastActor is null & ThisActor is valid
	*	- Highlight ThisActor
	*C. LastActor is valid & ThisActor is null
	*	- UnHighlight LastActor
	*D. Both actors are valid, but LastActor != ThisActor
	*	- UnHighlight LastActor, and Highlight ThisActor
	*E.Both actors are valid, and are the same actor
	*	- Do nothing
	*/

	if (LastActor == nullptr) 
	{
		if (ThisActor != nullptr)
		{
			// case B
			ThisActor->HighlightActor();
		}
		else
		{
			// Case A - both are null
		}
	}
	else // LastActor is valid
	{
		if (ThisActor == nullptr)
		{
			LastActor->UnHighlightActor();
		}
		else // both actors are valid
		{
			if (LastActor != ThisActor)
			{
				LastActor->UnHighlightActor();
				ThisActor->HighlightActor();
			}
			else
			{
				// Case E - do nothing
			}
		}
	}
}

void AAuraPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		// 左マウスクリック時、クリック対象が敵か否か
		bTargeting = ThisActor ? true : false;
		bAutoRunning = false;
	}
}

void AAuraPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	// 左マウス以外の時
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
		return;
	}

	/* 左マウスの場合 */
	// 敵をクリックした時
	if (bTargeting)
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagReleased(InputTag);
		}
	}
	else
	{
		// 何もターゲットしていない：ワンクリック移動
		APawn* ControllerPawn = GetPawn<APawn>();
		if (FollowTime <= ShortPressThreshold && ControllerPawn)
		{
			// 短押し判定、パスファインディング処理の実装
			if (UNavigationPath* NavPath = UNavigationSystemV1::FindPathToLocationSynchronously(
				this,
				ControllerPawn->GetActorLocation(),
				CachedDestination
			))
			{
				/* 移動処理 */
				// 既存のSplineポイントをクリア
				Spline->ClearSplinePoints();

				for (const FVector& PointLocation : NavPath->PathPoints)
				{
					// Navpathの経由地をSplineに追加
					Spline->AddSplinePoint(PointLocation, ESplineCoordinateSpace::World);

					DrawDebugSphere(
						GetWorld(),
						PointLocation,
						8.f,
						8,
						FColor::Green,
						false,
						5.f
					);
				}

				bAutoRunning = true;
			}
		}

		// 状態リセット
		FollowTime = 0.f;
		bTargeting = false;
	}
}

void AAuraPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	// 左マウスボタン以外の処理
	if (!InputTag.MatchesTagExact(FAuraGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
		return;
	}

	/* 左マウスの場合 */
	// 敵をクリックした時
	if (bTargeting)
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
	}
	else
	{
		// 移動処理を続行
		// マウス押下時間の累積
		FollowTime += GetWorld()->GetDeltaSeconds();

		// マウスカーソル下の3D座標取得
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			// 目標地点の更新
			CachedDestination = Hit.Location;
		}

		// 移動実行
		if (APawn* ControllerPawn = GetPawn())
		{
			// 移動方向の計算（ベクトルの正規化込み）
			const FVector WorldDirection = (CachedDestination - ControllerPawn->GetActorLocation()).GetSafeNormal();

			ControllerPawn->AddMovementInput(WorldDirection);
		}
	}
	
}

UAuraAbilitySystemComponent* AAuraPlayerController::GetASC()
{
	if ( AuraAbilitySystemComponent == nullptr)
	{
		AuraAbilitySystemComponent = Cast<UAuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return AuraAbilitySystemComponent;
}

void AAuraPlayerController::AutoRun()
{
	if (!bAutoRunning) return;
	if (APawn* ControllerPawn = GetPawn())
	{
		const FVector LocationOnSpline = Spline->FindLocationClosestToWorldLocation(
			ControllerPawn->GetActorLocation(),		// キャラクターの現在位置
			ESplineCoordinateSpace::World			// ワールド座標系を指定
		);

		const FVector Direction = Spline->FindDirectionClosestToWorldLocation(
			LocationOnSpline,				// 基準位置
			ESplineCoordinateSpace::World
		);
		ControllerPawn->AddMovementInput(Direction);

		const float DistanceToDistination = (LocationOnSpline - CachedDestination).Length(); // ベクトルの長さ
		if (DistanceToDistination <= AutoRunAcceptanceRadius)
		{
			bAutoRunning = false;
		}
	}
}
