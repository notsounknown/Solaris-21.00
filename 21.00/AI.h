#pragma once
#include "pch.h"
#include "Utils.h"

class AI
{
private:
	static void SpawnAI(const wchar_t *, const wchar_t *);
	static FFortItemEntry& GiveItem(AFortAthenaAIBotController*, FItemAndCount);
	static void CosmeticLoadoutOnSpawned(UFortAthenaAISpawnerDataComponent_CosmeticLoadout*, AFortAIPawn*);
	DefHookOg(void, AIBotBehaviorOnSpawned, UFortAthenaAISpawnerDataComponent_AIBotBehavior*, AFortAIPawn*);
	DefHookOg(void, InventoryOnSpawned, UFortAthenaAISpawnerDataComponent_InventoryBase*, AFortAIPawn*);
	DefHookOg(void, AIBotConversationOnSpawned, UFortAthenaAISpawnerDataComponent_AIBotConversation*, AFortAIPawn*);
	DefHookOg(void, CreateAndConfigureNavigationSystem, UNavigationSystemModuleConfig*, UWorld*);
	DefHookOg(void, OnPawnAISpawned, __int64, AFortAIPawn*);
	static void InitializeMMRInfos();
	static void SpawnAIHook(UAthenaAIServicePlayerBots* AIServicePlayerBots, FFrame& Stack, APawn** Ret);

	template <typename T>
	static T PickWeighted(TArray<T>& Map, float (*RandFunc)(float), bool bCheckZero = true) // pls put this into utils...
	{
		float TotalWeight = std::accumulate(Map.begin(), Map.end(), 0.0f, [&](float acc, T& p) { return acc + Utils::EvaluateScalableFloat(p.Weight); });
		float RandomNumber = RandFunc(TotalWeight);

		for (auto& Element : Map)
		{
			float Weight = Utils::EvaluateScalableFloat(Element.Weight);
			if (bCheckZero && Weight == 0)
				continue;

			if (RandomNumber <= Weight) return Element;

			RandomNumber -= Weight;
		}

		return T();
	}
public:
	static FDigestedWeaponAccuracyCategory* GetWeaponAccuracy(UFortAthenaAIBotAimingDigestedSkillSet*, UFortWeaponRangedItemDefinition*);
	static void SpawnAIs();

	InitHooks;
};