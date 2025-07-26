#include "pch.h"
#include "AI.h"
#include "Inventory.h"
#include "Options.h"

void AI::SpawnAI(const wchar_t* NPCName, const wchar_t *Season)
{
	auto SpawnerNameThing = wcscmp(NPCName, L"JonesyTheFirst") ? L"BP_AIBotSpawnerData_NPC_S" : L"BP_AIBotSpawnerData_S";
	auto Path = UEAllocatedWString(L"/NPCLibraryS") + Season + L"/NPCs/" + NPCName + L"/" + SpawnerNameThing + Season + L"_" + NPCName + L"." + SpawnerNameThing + Season + L"_" + NPCName + L"_C";
	auto Class = Utils::FindObject<UClass>(Path);
	auto SpawnerData = (UFortAthenaAIBotSpawnerData*)Class->DefaultObject;
	//auto SpawnerTag = SpawnerData->GetTyped()->DescriptorTag.GameplayTags[0].TagName;
	//auto SpawnerTagName = SpawnerTag.ToWString();
	//auto NPCName = SpawnerTagName.substr(SpawnerTagName.rfind(L'.') + 1);

	TArray<AFortAthenaPatrolPath*> PossibleSpawnPaths;
	for (auto& Path : Utils::GetAll<AFortAthenaPatrolPathPointProvider>())
	{
		if (Path->FiltersTags.GameplayTags.Num() == 0)
			continue;
		auto PathName = Path->FiltersTags.GameplayTags[0].TagName.ToWString();
		if (PathName.substr(PathName.rfind(L'.') + 1) == NPCName)
			PossibleSpawnPaths.Add(Path->AssociatedPatrolPath);
	}
	for (auto& PatrolPath : Utils::GetAll<AFortAthenaPatrolPath>())
	{
		if (PatrolPath->GameplayTags.GameplayTags.Num() == 0)
			continue;
		auto PathName = PatrolPath->GameplayTags.GameplayTags[0].TagName.ToWString();
		if (PathName.substr(PathName.rfind(L'.') + 1) == NPCName)
			PossibleSpawnPaths.Add(PatrolPath);
	}

	if (PossibleSpawnPaths.Num() > 0)
	{
		auto PatrolPath = PossibleSpawnPaths[(int)ceil((float)rand() / RAND_MAX) * PossibleSpawnPaths.Num() - 1];
		auto ComponentList = UFortAthenaAISpawnerData::CreateComponentListFromClass(Class, UWorld::GetWorld());

		auto Transform = PatrolPath->PatrolPoints[0]->GetTransform();
		((UAthenaAISystem*)UWorld::GetWorld()->AISystem)->AISpawner->RequestSpawn(ComponentList, Transform);
		Log(L"Spawning %s at %lf %lf %lf", NPCName, Transform.Translation.X, Transform.Translation.Y, Transform.Translation.Z);
	}
	else
	{
		Log(L"todo: %s has no paths", NPCName);
	}
}


FFortItemEntry& AI::GiveItem(AFortAthenaAIBotController *Controller, FItemAndCount ItemAndCount)
{
	UFortWorldItem* Item = (UFortWorldItem*)ItemAndCount.Item->CreateTemporaryItemInstanceBP(ItemAndCount.Count, -1);
	Item->OwnerInventory = Controller->Inventory;
	if (auto Weapon = ItemAndCount.Item->Cast<UFortWeaponItemDefinition>())
	{
		Item->ItemEntry.LoadedAmmo = Inventory::GetStats(Weapon)->ClipSize;
	}

	auto& ItemEntry = Controller->Inventory->Inventory.ReplicatedEntries.Add(Item->ItemEntry);
	auto ItemInstance = Controller->Inventory->Inventory.ItemInstances.Add(Item);

	Controller->Inventory->Inventory.MarkItemDirty(Item->ItemEntry);
	Controller->Inventory->HandleInventoryLocalUpdate();
	if (ItemAndCount.Item->Cast<UFortWeaponRangedItemDefinition>())
	{
		//Controller->EquipWeapon(Item);
	}
	return ItemEntry;
}

void AI::InventoryOnSpawned(UFortAthenaAISpawnerDataComponent_InventoryBase* InventoryBase, AFortAIPawn* Pawn)
{
	auto Controller = (AFortAthenaAIBotController*)Pawn->Controller;

	if (!Controller->Inventory)
	{
		if (Controller->Inventory = Utils::SpawnActor<AFortInventory>(FVector{}, {}))
		{
			Controller->Inventory->Owner = Controller;
			Controller->Inventory->OnRep_Owner();
		}
	}

	for (auto& Item : ((UFortAthenaAISpawnerDataComponent_AIBotInventory*)InventoryBase)->Items)
	{
		GiveItem(Controller, Item);
	}
}

void AI::CosmeticLoadoutOnSpawned(UFortAthenaAISpawnerDataComponent_CosmeticLoadout* CosmeticLoadout, AFortAIPawn* Pawn) 
{
	((void (*)(APlayerState*, APawn*)) Sarah::Offsets::ApplyCharacterCustomization)(Pawn->PlayerState, Pawn);
}

void AI::AIBotConversationOnSpawned(UFortAthenaAISpawnerDataComponent_AIBotConversation* AIBotConversation, AFortAIPawn* Pawn)
{
	auto Controller = (AFortAthenaAIBotController*)Pawn->Controller;

	UFortNPCConversationParticipantComponent* NPCConversationParticipantComponent = (UFortNPCConversationParticipantComponent*)Controller->GetComponentByClass(AIBotConversation->ConversationComponentClass);

	if (!NPCConversationParticipantComponent)
	{
		NPCConversationParticipantComponent = (UFortNPCConversationParticipantComponent*)Controller->AddComponentByClass(AIBotConversation->ConversationComponentClass, false, FTransform(), false);
	}

	NPCConversationParticipantComponent->PlayerPawnOwner = (AFortPlayerPawn*)Pawn;
	NPCConversationParticipantComponent->BotControllerOwner = Controller;

	NPCConversationParticipantComponent->ConversationEntryTag = AIBotConversation->ConversationEntryTag;
	NPCConversationParticipantComponent->CharacterData = AIBotConversation->CharacterData;

	if (NPCConversationParticipantComponent->ServiceProviderIDTag.TagName == FName(0))
	{
		NPCConversationParticipantComponent->ServiceProviderIDTag.TagName = NPCConversationParticipantComponent->ConversationEntryTag.TagName;
	}

	if (NPCConversationParticipantComponent->SalesInventory)
	{
		for (const auto& [RowName, RowPtr] : NPCConversationParticipantComponent->SalesInventory->RowMap)
		{
			FNPCSaleInventoryRow* Row = (FNPCSaleInventoryRow*)RowPtr;

			if (!RowName.IsValid() || !Row)
				continue;

			if (Row->NPC.TagName == NPCConversationParticipantComponent->ServiceProviderIDTag.TagName)
			{
				NPCConversationParticipantComponent->SupportedSales.Add(*Row);
			}
		}
	}

	if (NPCConversationParticipantComponent->Services)
	{
		for (const auto& [RowName, RowPtr] : NPCConversationParticipantComponent->Services->RowMap)
		{
			FNPCDynamicServiceRow* Row = (FNPCDynamicServiceRow*)RowPtr;

			if (!RowName.IsValid() || !Row)
				continue;

			if (Row->NPC == NPCConversationParticipantComponent->ServiceProviderIDTag)
			{
				NPCConversationParticipantComponent->SupportedServices.GameplayTags.Add(Row->ServiceTag);
			}
		}
	}

	NPCConversationParticipantComponent->bCanStartConversation = true;
	NPCConversationParticipantComponent->OnRep_CanStartConversation();
}


void AI::AIBotBehaviorOnSpawned(UFortAthenaAISpawnerDataComponent_AIBotBehavior* AIBotBehavior, AFortAIPawn* Pawn)
{
	auto Controller = (AFortAthenaAIBotController*) Pawn->Controller;

	*(bool*)(__int64(Controller->CachedAIServicePlayerBots) + 0x7b8) = true;
	if (!Controller->BrainComponent || !Controller->BrainComponent->IsA<UBehaviorTreeComponent>())
	{
		Controller->BrainComponent = (UBrainComponent*)UGameplayStatics::SpawnObject(UBehaviorTreeComponent::StaticClass(), Controller);
		auto RegisterComponent = (void (*)(UActorComponent*, UWorld*, void*)) (Sarah::Offsets::ImageBase + 0xd287ac);
		RegisterComponent(Controller->BrainComponent, UWorld::GetWorld(), nullptr);
	}

	//Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhaseStep"), (uint8)EAthenaGamePhaseStep::Warmup);
	//Controller->Blackboard->SetValueAsEnum(FName(L"AIEvaluator_Global_GamePhase"), (uint8)EAthenaGamePhase::Warmup);

	return AIBotBehaviorOnSpawnedOG(AIBotBehavior, Pawn);
}

void AI::OnPawnAISpawned(__int64 ControllerIFaceThing, AFortAIPawn *Pawn)
{
	OnPawnAISpawnedOG(ControllerIFaceThing, Pawn);
}

void AI::CreateAndConfigureNavigationSystem(UNavigationSystemModuleConfig* ModuleConfig, UWorld *World)
{
	ModuleConfig->bAutoSpawnMissingNavData = true;
	return CreateAndConfigureNavigationSystemOG(ModuleConfig, World);
}

FDigestedWeaponAccuracyCategory* AI::GetWeaponAccuracy(UFortAthenaAIBotAimingDigestedSkillSet* SkillSet, UFortWeaponRangedItemDefinition* ItemDefinition)
{
	/*auto TagName = ItemDefinition->AnalyticTags.GameplayTags[0].TagName;
	auto PhoebeTag = FName()
	return SkillSet->WeaponAccuracies.Search([&](FDigestedWeaponAccuracyCategory& Category)
	{

	});*/
	return nullptr;
}

void AI::SpawnAIs()
{
	SpawnAI(L"AgentJones", L"19");
	SpawnAI(L"BaoBros", L"19");
	SpawnAI(L"Brainiac", L"19");
	SpawnAI(L"BuffLlama", L"19");
	SpawnAI(L"BunkerJonesy", L"19");
	SpawnAI(L"Cuddlepool", L"19");
	SpawnAI(L"CuddleTeamLeader", L"19");
	SpawnAI(L"ExoSuit", L"19");
	SpawnAI(L"Galactico", L"19");
	SpawnAI(L"Guaco", L"19");
	SpawnAI(L"IslandNomad", L"19");
	SpawnAI(L"JonesyTheFirst", L"19");
	SpawnAI(L"LilWhip", L"19");
	SpawnAI(L"LoneWolf", L"19");
	SpawnAI(L"Ludwig", L"19");
	SpawnAI(L"Mancake", L"19");
	SpawnAI(L"MetalTeamLeader", L"19");
	SpawnAI(L"MulletMarauder", L"19");
	SpawnAI(L"Quackling", L"19");
	SpawnAI(L"Ragsy", L"19");
	SpawnAI(L"TheScientist", L"19");
	SpawnAI(L"TheVisitor", L"19");
	SpawnAI(L"TomatoHead", L"19");

	SpawnAI(L"Binary", L"20");
	SpawnAI(L"Columbus", L"20");
	SpawnAI(L"Mystic", L"20");
	SpawnAI(L"Peely", L"20");
	SpawnAI(L"TheOrigin", L"20");

	SpawnAI(L"Cryptic", L"21");
	SpawnAI(L"Fishstick", L"21");
	SpawnAI(L"Kyle", L"21");
	SpawnAI(L"MoonHawk", L"21");
	SpawnAI(L"Rustler", L"21");
	SpawnAI(L"Stashd", L"21");
	SpawnAI(L"Sunbird", L"21");
	SpawnAI(L"TheParadigm", L"21");
}

void AI::InitializeMMRInfos()
{
	UAthenaAIServicePlayerBots* AIServicePlayerBots = UAthenaAIBlueprintLibrary::GetAIServicePlayerBots(UWorld::GetWorld());

	AIServicePlayerBots->DefaultBotAISpawnerData = Utils::FindObject<UClass>(L"/Game/Athena/AI/Phoebe/BP_AISpawnerData_Phoebe.BP_AISpawnerData_Phoebe_C");

	Log(L"%s", AIServicePlayerBots->DefaultBotAISpawnerData->GetFullWName().c_str());
	
	FMMRSpawningInfo NewSpawningInfo{};
	NewSpawningInfo.SpawnerData = (UFortAthenaAISpawnerData*)AIServicePlayerBots->DefaultBotAISpawnerData->DefaultObject;
	NewSpawningInfo.AmountToSpawn = 70;

	AIServicePlayerBots->CachedMMRSpawningInfo.SpawningInfos.Add(NewSpawningInfo);
}

void AI::SpawnAIHook(UAthenaAIServicePlayerBots* AIServicePlayerBots, FFrame& Stack, APawn** Ret)
{
	FVector InSpawnLocation;
	FRotator InSpawnRotation;
	UFortAthenaAISpawnerDataComponentList* AISpawnerComponentList;

	Stack.StepCompiledIn(&InSpawnLocation);
	Stack.StepCompiledIn(&InSpawnRotation);
	Stack.StepCompiledIn(&AISpawnerComponentList);

	Stack.IncrementCode();

	FTransform SpawnTransform;
	SpawnTransform.Translation = InSpawnLocation;
	SpawnTransform.Rotation = InSpawnRotation;
	SpawnTransform.Scale3D = FVector(1, 1, 1);

	int32 RequestId = ((UFortAISystem*)AIServicePlayerBots->AIServiceManager->AISystem)->AISpawner->RequestSpawn(AISpawnerComponentList, SpawnTransform);

	// how get pawn??

	*Ret = nullptr;
}

void AI::Hook()
{
	Utils::Hook(Sarah::Offsets::ImageBase + 0x8cf0718, InventoryOnSpawned, InventoryOnSpawnedOG);
	Utils::Hook<UFortAthenaAISpawnerDataComponent_CosmeticLoadout>(uint32(0x5a), CosmeticLoadoutOnSpawned);
	Utils::Hook<UFortAthenaAISpawnerDataComponent_AIBotBehavior>(uint32(0x5a), AIBotBehaviorOnSpawned, AIBotBehaviorOnSpawnedOG);
	Utils::Hook<UFortAthenaAISpawnerDataComponent_AIBotConversation>(uint32(0x5a), AIBotConversationOnSpawned);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x1205608, CreateAndConfigureNavigationSystem, CreateAndConfigureNavigationSystemOG);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x8c00d78, OnPawnAISpawned, OnPawnAISpawnedOG);
	if (!bGameSessions)
		Utils::Patch<uint16>(Sarah::Offsets::ImageBase + 0x8D10F88, 0xe990);
	Utils::Patch<uint32>(Sarah::Offsets::ImageBase + 0x8d061a6, 0x4e5152);
	Utils::Hook(Sarah::Offsets::ImageBase + 0x91eb2fc, InitializeMMRInfos);
	Utils::ExecHook(L"/Script/FortniteAI.AthenaAIServicePlayerBots.SpawnAI", SpawnAIHook);

	UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogAthenaAIServiceBots VeryVerbose", nullptr);
	UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogAthenaBots VeryVerbose", nullptr);
	//UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogNavigation VeryVerbose", nullptr);
	//UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"log LogNavigationDataBuild VeryVerbose", nullptr);
}
