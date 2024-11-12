#include "Actor/AuraEffectActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

AAuraEffectActor::AAuraEffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));
}

void AAuraEffectActor::BeginPlay()
{
	Super::BeginPlay();

	// You can add any setup code here if needed.
}

void AAuraEffectActor::ApplyEffectToTarget(AActor* TargetActor, const FAppliedEffectStruct& EffectStruct)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	// Get the gameplay effect class from EffectStruct
	TSubclassOf<UGameplayEffect> GameplayEffectClass = EffectStruct.GameplayEffectClass;
	if (!GameplayEffectClass) return;

	// Make and apply the effect spec
	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);

	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(GameplayEffectClass, ActorLevel, EffectContextHandle);
	FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	// Check if the effect has an infinite duration
	const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;

	// Only store the handle if the effect is infinite and its policy is set to RemoveOnEndOverlap
	if (bIsInfinite && EffectStruct.EffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		// Store the effect handle and target ASC for later removal
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}
}

void AAuraEffectActor::OnOverlap(AActor* TargetActor)
{
	for (const FAppliedEffectStruct& Effect : InstantEffects)
	{
		if (Effect.EffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
		{
			ApplyEffectToTarget(TargetActor, Effect);
		}
	}

	for (const FAppliedEffectStruct& Effect : DurationEffects)
	{
		if (Effect.EffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
		{
			ApplyEffectToTarget(TargetActor, Effect);
		}
	}

	for (const FAppliedEffectStruct& Effect : InfiniteEffects)
	{
		if (Effect.EffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
		{
			ApplyEffectToTarget(TargetActor, Effect);
		}
	}
}

void AAuraEffectActor::OnEndOverlap(AActor* TargetActor)
{
	// Apply Instant Effects on EndOverlap
	for (const FAppliedEffectStruct& Effect : InstantEffects)
	{
		if (Effect.EffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
		{
			ApplyEffectToTarget(TargetActor, Effect);
		}
	}

	// Apply Duration Effects on EndOverlap
	for (const FAppliedEffectStruct& Effect : DurationEffects)
	{
		if (Effect.EffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
		{
			ApplyEffectToTarget(TargetActor, Effect);
		}
	}

	// Apply Infinite Effects on EndOverlap and handle removal
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	TArray<FActiveGameplayEffectHandle> HandlesToRemove;

	for (const FAppliedEffectStruct& Effect : InfiniteEffects)
	{
		// Apply effect on EndOverlap if the policy is set to ApplyOnEndOverlap
		if (Effect.EffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
		{
			ApplyEffectToTarget(TargetActor, Effect);
		}

		// Remove the infinite effect if the removal policy is set to RemoveOnEndOverlap
		if (Effect.EffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
		{
			// Find and remove the effect associated with this component and effect handle
			for (TTuple<FActiveGameplayEffectHandle, UAbilitySystemComponent*> HandlePair : ActiveEffectHandles)
			{
				if (TargetASC == HandlePair.Value)
				{
					TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
					HandlesToRemove.Add(HandlePair.Key);
				}
			}
		}
	}

	// Remove handles from the ActiveEffectHandles map
	for (const FActiveGameplayEffectHandle& Handle : HandlesToRemove)
	{
		ActiveEffectHandles.Remove(Handle);
	}
}

