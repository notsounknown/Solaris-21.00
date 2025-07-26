#include "pch.h"
#include "Replication.h"
#include "Misc.h"
#include "Options.h"

float& Replication::GetTimeSeconds(UWorld* World)
{
	return *(float*)(__int64(World) + ReplicationOffsets::TimeSeconds);
}

int& Replication::GetReplicationFrame(UNetDriver* Driver)
{
	return *(int*)(__int64(Driver) + ReplicationOffsets::ReplicationFrame);
}

FNetworkObjectList& Replication::GetNetworkObjectList(UNetDriver* Driver)
{
	return *(*(class SDK::TSharedPtr<FNetworkObjectList>*)(__int64(Driver) + ReplicationOffsets::NetworkObjectList));
}

double& Replication::GetElapsedTime(UNetDriver* Driver)
{
	return *(double*)(__int64(Driver) + ReplicationOffsets::ElapsedTime);
}

AActor* Replication::GetViewTarget(APlayerController* Controller)
{
	return (decltype(&GetViewTarget)(ReplicationOffsets::GetViewTarget))(Controller);
}

double& Replication::GetRelevantTime(UActorChannel* Channel)
{
	return *(double*)(__int64(Channel) + ReplicationOffsets::RelevantTime);
}

int32& Replication::GetNetTag(UNetDriver* Driver)
{
	return *(int32*)(__int64(Driver) + ReplicationOffsets::NetTag);
}

TSet<FNetworkGUID>& Replication::GetDestroyedStartupOrDormantActorGUIDs(UNetConnection* Conn)
{
	return *(TSet<FNetworkGUID>*)(__int64(Conn) + ReplicationOffsets::DestroyedStartupOrDormantActorGUIDs);
}

TMap<FNetworkGUID, TUniquePtr<FActorDestructionInfo>>& Replication::GetDestroyedStartupOrDormantActors(UNetDriver* Driver)
{
	return *(TMap<FNetworkGUID, TUniquePtr<FActorDestructionInfo>>*)(__int64(Driver) + ReplicationOffsets::DestroyedStartupOrDormantActors);
}

uint32& Replication::GetLastProcessedFrame(UNetConnection* Conn)
{
	return *(uint32*)(__int64(Conn) + ReplicationOffsets::LastProcessedFrame);
}

TSet<FName>& Replication::GetClientVisibleLevelNames(UNetConnection* Conn)
{
	return *(TSet<FName>*)(__int64(Conn) + ReplicationOffsets::ClientVisibleLevelNames);
}

FName& Replication::GetClientWorldPackageName(UNetConnection* Conn)
{
	return *(FName*)(__int64(Conn) + ReplicationOffsets::ClientWorldPackageName);
}

AActor*& Replication::GetRepContextActor(UNetConnection* Conn)
{
	return *(AActor**)(__int64(Conn) + ReplicationOffsets::RepContextActor);
}

ULevel*& Replication::GetRepContextLevel(UNetConnection* Conn)
{
	return *(ULevel**)(__int64(Conn) + ReplicationOffsets::RepContextLevel);
}

class SDK::TSharedPtr<void*>& Replication::GetGuidCache(UNetDriver* Driver)
{
	return *(class SDK::TSharedPtr<void*>*)(__int64(Driver) + ReplicationOffsets::GuidCache);
}

bool& Replication::GetPendingCloseDueToReplicationFailure(UNetConnection* Conn)
{
	return *(bool*)(__int64(Conn) + ReplicationOffsets::PendingCloseDueToReplicationFailure);
}

EConnectionState& Replication::GetState(UNetConnection* Conn)
{
	return *(EConnectionState*)(__int64(Conn) + ReplicationOffsets::State);
}

ULevel* GetLevel(AActor* Actor) {
	ULevel* Level = nullptr;
	auto Outer = Actor->Outer;
	while (Outer && !Level)
	{
		if (Outer->Class == ULevel::StaticClass())
		{
			Level = (ULevel*)Outer;
			break;
		}
		else
		{
			Outer = Outer->Outer;
		}
	}
	return Level;
}

UWorld* GetWorld(AActor* Actor) {
	if (!Actor->IsDefaultObject() && Actor->Outer && !(Actor->Outer->Flags & EObjectFlags::BeginDestroyed) && !(Actor->Outer->Flags & EObjectFlags(0x10000000))) {
		if (auto Level = GetLevel(Actor))
			return Level->OwningWorld;
	}
	return nullptr;
}

int Replication::PrepConns(UNetDriver* Driver)
{
	bool bFoundReadyConn = false;
	for (auto& Conn : Driver->ClientConnections)
	{
		auto Owner = Conn->OwningActor;
		if (Owner && GetState(Conn) == USOCK_Open && GetElapsedTime(Conn->Driver) - Conn->LastReceiveTime < 1.5)
		{
			bFoundReadyConn = true;
			auto OutViewTarget = Owner;
			if (auto Controller = Conn->PlayerController)
				if (auto ViewTarget = GetViewTarget(Controller))
					if (GetWorld(Controller))
						OutViewTarget = ViewTarget;
			Conn->ViewTarget = OutViewTarget;
			for (auto& Child : Conn->Children)
			{
				if (auto Controller = Child->PlayerController)
					Child->ViewTarget = GetViewTarget(Controller);
				else
					Child->ViewTarget = nullptr;
			}
		}
		else
		{
			Conn->ViewTarget = nullptr;
			for (auto& Child : Conn->Children)
				Child->ViewTarget = nullptr;
		}
	}

	return bFoundReadyConn ? Driver->ClientConnections.Num() : 0;
}

float FRand()
{
	/*random_device rd;
	mt19937 gen(rd());
	uniform_real_distribution<> dis(0, 1);
	float random_number = (float) dis(gen);

	return random_number;*/
	constexpr int32 RandMax = 0x00ffffff < RAND_MAX ? 0x00ffffff : RAND_MAX;
	return (rand() & RandMax) / (float)RandMax;
}

float fastLerp(float a, float b, float t)
{
	//return (a * (1.f - t)) + (b * t);
	return a - (a + b) * t;
}

TArray<TPair<FNetworkObjectInfo*, TArray<TPair<UNetConnection *, FRelevantConnectionInfo>>>> ConsiderList;
TArray<TPair<UNetConnection*, TArray<FNetViewer>>> ViewerMap;
void Replication::BuildConsiderList(UNetDriver* Driver, float DeltaTime, TArray<FNetViewer>& AllConns)
{
	void (*CallPreReplication)(AActor*, UNetDriver*) = decltype(CallPreReplication)(ReplicationOffsets::CallPreReplication);
	void (*RemoveNetworkActor)(UNetDriver*, AActor*) = decltype(RemoveNetworkActor)(ReplicationOffsets::RemoveNetworkActor);
	UActorChannel* (*FindActorChannelRef)(UNetConnection*, const TWeakObjectPtr<AActor>&) = decltype(FindActorChannelRef)(ReplicationOffsets::FindActorChannelRef);
	void (*CloseActorChannel)(UActorChannel*, uint8) = decltype(CloseActorChannel)(ReplicationOffsets::CloseActorChannel);
	void (*StartBecomingDormant)(UActorChannel*) = decltype(StartBecomingDormant)(ReplicationOffsets::StartBecomingDormant);


	auto& ActiveNetworkObjects = GetNetworkObjectList(Driver).ActiveNetworkObjects;
	auto Time = GetTimeSeconds(Driver->World);
	TArray<TPair<UNetConnection*, bool>> RelevantConnections;

	ConsiderList.ResetNum();
	ConsiderList.Reserve(ActiveNetworkObjects.Num());

	for (auto& ActorInfo : ActiveNetworkObjects)
	{
		if (!ActorInfo->bPendingNetUpdate && Time <= ActorInfo->NextUpdateTime)
			continue;
		auto Actor = ActorInfo->Actor;
		if (!Actor) continue;
		auto Outer = Actor->Outer;
		if (Actor->bActorIsBeingDestroyed || Actor->Flags & (EObjectFlags::PendingKill | EObjectFlags::Garbage) || Actor->RemoteRole == ENetRole::ROLE_None || (Actor->bNetStartup && Actor->NetDormancy == ENetDormancy::DORM_Initial))
		{
			RemoveNetworkActor(Driver, Actor);
			continue;
		}
		if (!Actor->bActorInitialized || Actor->NetDriverName != Driver->NetDriverName)
			continue;

		ULevel* Level = GetLevel(Actor);
		if (!Level || Level == Level->OwningWorld->CurrentLevelPendingVisibility || Level == Level->OwningWorld->CurrentLevelPendingInvisibility || Level->bIsAssociatingLevel)
		{
			continue;
		}

		TArray<TPair<UNetConnection*, FRelevantConnectionInfo>> RelevantConnections;
		RelevantConnections.Reserve(AllConns.Num());
		for (auto& ConnViewer : AllConns)
		{
			auto& Conn = ConnViewer.Connection;
			auto Channel = FindActorChannelRef(Conn, ActorInfo->WeakActor);

			auto ViewerPair = ViewerMap.Search([&](TPair<UNetConnection*, TArray<FNetViewer>>& Pair) 
			{
				return Pair.First == Conn;
			});
			auto& Viewers = ViewerPair->Second;

			for (auto& Temp : Conn->SentTemporaries)
				if (Temp == Actor)
					continue;

			bool bLevelInitializedForActor = IsLevelInitializedForActor(Driver, Actor, Conn);
			bool bRelevantToConn = IsActorRelevantToConnection(Actor, Viewers);
			if (!Channel && (!bRelevantToConn || !bLevelInitializedForActor)) 
				continue;
			
			if (Actor->bOnlyRelevantToOwner)
			{
				bool bHasNullViewTarget = false;

				if (!IsActorOwnedByAndRelevantToConnection(Actor, Viewers, bHasNullViewTarget))
				{
					if (!bHasNullViewTarget && Channel != NULL && GetElapsedTime(Driver) - GetRelevantTime(Channel) >= Driver->RelevantTimeout)
						CloseActorChannel(Channel, 3);

					continue;
				}
			}
			
			if (ActorInfo->DormantConnections.Contains(Conn)) continue;

			if (ShouldActorGoDormant(Actor, Viewers, Channel, (float)GetElapsedTime(Driver), false))
				StartBecomingDormant(Channel);


			bool bRelevant = bLevelInitializedForActor && !Actor->bTearOff && (!Channel || GetElapsedTime(Driver) - GetRelevantTime(Channel) > 1.0) && bRelevantToConn;
			bool bRecentlyRelevant = bRelevant || (Channel && GetElapsedTime(Driver) - GetRelevantTime(Channel) < Driver->RelevantTimeout) || (ActorInfo->ForceRelevantFrame >= GetLastProcessedFrame(Conn));

			if (bRecentlyRelevant)
			{
				RelevantConnections.Add({ Conn, { Channel, bRelevant } });
				continue;
			}
			
			if (Channel && (!bRecentlyRelevant || Actor->bTearOff) && (bLevelInitializedForActor || !Actor->bNetStartup))
				CloseActorChannel(Channel, Actor->bTearOff ? 4 : 3);
		}
		
		/*if (ActorInfo->LastNetReplicateTime == 0)
		{
			ActorInfo->LastNetReplicateTime = Time;
			ActorInfo->OptimalNetUpdateDelta = 1.f / Actor->NetUpdateFrequency;
		}

		const float LastReplicateDelta = float(Time - ActorInfo->LastNetReplicateTime);

		if (LastReplicateDelta > 2.f)
		{
			if (Actor->MinNetUpdateFrequency == 0.f)
			{
				Actor->MinNetUpdateFrequency = 2.f;
			}

			const float MinOptimalDelta = 1.f / Actor->NetUpdateFrequency;
			const float MaxOptimalDelta = max(1.f / Actor->MinNetUpdateFrequency, MinOptimalDelta);

			const float Alpha = clamp((LastReplicateDelta - 2.f) / 5.f, 0.f, 1.f);
			ActorInfo->OptimalNetUpdateDelta = fastLerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}*/                                                             

		if (!ActorInfo->bPendingNetUpdate)
		{
			//constexpr bool bUseAdapativeNetFrequency = false;
			const float NextUpdateDelta = /*bUseAdapativeNetFrequency ? ActorInfo->OptimalNetUpdateDelta : */1.f / Actor->NetUpdateFrequency;

			float ServerTickTime = 1.f / std::clamp(1.f / DeltaTime, 1.f, MaxTickRate);
			ActorInfo->NextUpdateTime = Time + FRand() * ServerTickTime + NextUpdateDelta;
			ActorInfo->LastNetUpdateTimestamp = (float)GetElapsedTime(Driver);
		}
		else
		{
			ActorInfo->bPendingNetUpdate = false;
		}


		if (RelevantConnections.Num() == 0) {
			RelevantConnections.Free();
		}
		else {
			ConsiderList.Add({ ActorInfo.Get(), RelevantConnections });
		}

		CallPreReplication(Actor, Driver);
	}
}

bool Replication::IsActorRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers)
{
	bool (*IsNetRelevantFor)(const AActor *, const AActor *, const AActor *, const FVector &) = decltype(IsNetRelevantFor)(Actor->VTable[ReplicationOffsets::IsNetRelevantForVft]);

	for (auto& Viewer : ConnectionViewers)
	{
		if (IsNetRelevantFor(Actor, Viewer.InViewer, Viewer.ViewTarget, Viewer.ViewLocation))
		{
			return true;
		}
	}

	return false;
}

FNetViewer Replication::ConstructNetViewer(UNetConnection* NetConnection)
{
	FNetViewer NewNetViewer{};
	NewNetViewer.Connection = NetConnection;
	NewNetViewer.InViewer = NetConnection->PlayerController ? NetConnection->PlayerController : NetConnection->OwningActor;
	NewNetViewer.ViewTarget = NetConnection->ViewTarget;

	APlayerController* ViewingController = NetConnection->PlayerController;

	if (ViewingController)
	{
		//FRotator ViewRotation = ViewingController->ControlRotation;
		FRotator ViewRotation;
		ViewingController->GetPlayerViewPoint(&NewNetViewer.ViewLocation, &ViewRotation);
		constexpr auto radian = 0.017453292519943295;
		double cosPitch = cos(ViewRotation.Pitch * radian), sinPitch = sin(ViewRotation.Pitch * radian), cosYaw = cos(ViewRotation.Yaw * radian), sinYaw = sin(ViewRotation.Yaw * radian);
		NewNetViewer.ViewDir = FVector(cosPitch * cosYaw, cosPitch * sinYaw, sinPitch);
	}
	else {
		NewNetViewer.ViewLocation = NewNetViewer.ViewTarget->K2_GetActorLocation();
	}

	return NewNetViewer;
}

bool Replication::IsActorOwnedByAndRelevantToConnection(const AActor* Actor, TArray<FNetViewer>& ConnViewers, bool& bOutHasNullViewTarget)
{
	bool (*IsRelevancyOwnerFor)(const AActor*, const AActor*, const AActor*, const AActor*) = decltype(IsRelevancyOwnerFor)(Actor->VTable[ReplicationOffsets::IsRelevancyOwnerForVft]);
	AActor *(*GetNetOwner)(const AActor*) = decltype(GetNetOwner)(Actor->VTable[ReplicationOffsets::GetNetOwnerVft]);

	const AActor* ActorOwner = GetNetOwner(Actor);

	bOutHasNullViewTarget = false;

	for (auto& Viewer : ConnViewers)
	{
		auto Conn = Viewer.Connection;

		if (Conn->ViewTarget == nullptr)
		{
			bOutHasNullViewTarget = true;
		}

		if (ActorOwner == Conn->PlayerController ||
			(Conn->PlayerController && ActorOwner == Conn->PlayerController->Pawn) ||
			(Conn->ViewTarget && IsRelevancyOwnerFor(Conn->ViewTarget, Actor, ActorOwner, Conn->OwningActor)))
		{
			return true;
		}
	}

	return false;
}

bool Replication::ShouldActorGoDormant(AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, UActorChannel* Channel, const float Time, const bool bLowNetBandwidth)
{
	if (Actor->NetDormancy <= ENetDormancy::DORM_Awake || !Channel || Channel->bPendingDormancy || Channel->Dormant)
	{
		// Either shouldn't go dormant, or is already dormant
		return false;
	}

	if (Actor->NetDormancy == ENetDormancy::DORM_DormantPartial)
	{
		Log(L"dorm partial??? %s", Actor->GetWName().c_str());
		for (int32 viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++)
		{
			//if (!GetNetDormancy(Actor, ConnectionViewers[viewerIdx].ViewLocation, ConnectionViewers[viewerIdx].ViewDir, ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, Channel, Time, bLowNetBandwidth))
			{
				return false;
			}
		}
	}

	return true;
}

bool Replication::IsLevelInitializedForActor(const UNetDriver* NetDriver, const AActor* InActor, UNetConnection* InConnection)
{
	bool (*ClientHasInitializedLevelFor)(const UNetConnection*, const AActor*) = decltype(ClientHasInitializedLevelFor)(ReplicationOffsets::ClientHasInitializedLevelFor);

	const bool bCorrectWorld = NetDriver->WorldPackage != nullptr && (GetClientWorldPackageName(InConnection) == NetDriver->WorldPackage->Name) && ClientHasInitializedLevelFor(InConnection, InActor);		
	const bool bIsConnectionPC = (InActor == InConnection->PlayerController);
	return bCorrectWorld || bIsConnectionPC;
}

void Replication::SendClientAdjustment(APlayerController *PlayerController)
{
	auto& ServerFrameInfo = *(FServerFrameInfo*)(__int64(PlayerController) + ReplicationOffsets::ServerFrameInfo);

	if (ServerFrameInfo.LastProcessedInputFrame != -1 && ServerFrameInfo.LastProcessedInputFrame != ServerFrameInfo.LastSentLocalFrame)
	{
		ServerFrameInfo.LastSentLocalFrame = ServerFrameInfo.LastProcessedInputFrame;
		PlayerController->ClientRecvServerAckFrame(ServerFrameInfo.LastProcessedInputFrame, ServerFrameInfo.LastLocalFrame, ServerFrameInfo.QuantizedTimeDilation);
	}


	if (PlayerController->AcknowledgedPawn != PlayerController->Pawn && !PlayerController->SpectatorPawn)
	{
		return;
	}

	auto Pawn = (ACharacter*)(PlayerController->Pawn ? PlayerController->Pawn : PlayerController->SpectatorPawn);
	if (Pawn && Pawn->RemoteRole == ENetRole::ROLE_AutonomousProxy) {
		auto Interface = Utils::GetInterface<INetworkPredictionInterface>(Pawn->CharacterMovement);

		if (Interface)
		{
			auto SendClientAdjustment = (void (*)(INetworkPredictionInterface*)) ReplicationOffsets::SendClientAdjustment;
			SendClientAdjustment(Interface);
		}
	}
}

UEAllocatedVector<FActorPriority> PriorityActors;

void Replication::PrioritizeActors(UNetDriver *Driver, UNetConnection *Conn, TArray<FNetViewer>& ConnViewers) {
	void (*CloseActorChannel)(UActorChannel*, uint8) = decltype(CloseActorChannel)(ReplicationOffsets::CloseActorChannel);
	UActorChannel* (*FindActorChannelRef)(UNetConnection*, const TWeakObjectPtr<AActor>&) = decltype(FindActorChannelRef)(ReplicationOffsets::FindActorChannelRef);

	int32 MaxSortedActors = ConsiderList.Num() + GetDestroyedStartupOrDormantActors(Driver).Num();
	if (MaxSortedActors > 0) {
		PriorityActors.clear();
		PriorityActors.reserve(MaxSortedActors);

		for (auto& ActorInfoPair : ConsiderList) {
			auto RelevancyInfoPair = ActorInfoPair.Second.Search([&](auto& Pair)
			{
				return Pair.First == Conn;
			});

			if (RelevancyInfoPair)
			{
				auto& Channel = RelevancyInfoPair->Second.Channel;
				auto& ActorInfo = ActorInfoPair.First;

				PriorityActors.push_back(FActorPriority(Conn, Channel, ActorInfo, RelevancyInfoPair->Second.bRelevant, ConnViewers));
			}
		}

		for (auto& NetworkGUID : GetDestroyedStartupOrDormantActorGUIDs(Conn))
		{
			auto Equals = [](const FNetworkGUID& LeftKey, const FNetworkGUID& RightKey) -> bool
				{
					return LeftKey == RightKey;
				};

			auto DestructionInfoPtr = GetDestroyedStartupOrDormantActors(Driver).Search([&](FNetworkGUID& GUID, TUniquePtr<FActorDestructionInfo>& InfoUPtr)
			{
				return GUID == NetworkGUID && (InfoUPtr->StreamingLevelName == FName(0) || GetClientVisibleLevelNames(Conn).Contains(InfoUPtr->StreamingLevelName));
			});

			if (DestructionInfoPtr)
			{
				PriorityActors.push_back(FActorPriority(Conn, (*DestructionInfoPtr).Get(), ConnViewers));
			}
		}

		std::sort(std::execution::par_unseq, PriorityActors.begin(), PriorityActors.end(), std::greater());
	}
}

bool Replication::IsNetReady(UNetConnection* Connection, bool bSaturate) {
	bool (*IsNetReady)(UNetConnection*, bool) = decltype(IsNetReady)(ReplicationOffsets::IsNetReady);
	return IsNetReady(Connection, bSaturate);
}

bool Replication::IsNetReady(UChannel* Channel, bool bSaturate) {
	if (Channel->NumOutRec >= 255)
		return 0;
	return IsNetReady(Channel->Connection, bSaturate);
}

size_t Replication::ProcessPrioritizedActors(UNetDriver *Driver, UNetConnection *Conn, TArray<FNetViewer>& ConnViewers) {
	UActorChannel* (*CreateChannelByName)(UNetConnection*, FName*, EChannelCreateFlags, int32_t) = decltype(CreateChannelByName)(ReplicationOffsets::CreateChannelByName);
	__int64 (*SetChannelActor)(UActorChannel*, AActor*) = decltype(SetChannelActor)(ReplicationOffsets::SetChannelActor);
	__int64 (*ReplicateActor)(UActorChannel*) = decltype(ReplicateActor)(ReplicationOffsets::ReplicateActor);
	void (*CloseActorChannel)(UActorChannel*, uint8) = decltype(CloseActorChannel)(ReplicationOffsets::CloseActorChannel);
	int64(*SetChannelActorForDestroy)(UActorChannel*, FActorDestructionInfo*) = decltype(SetChannelActorForDestroy)(ReplicationOffsets::SetChannelActorForDestroy);
	bool (*SupportsObject)(void*, const UObject*, const TWeakObjectPtr<UObject>*) = decltype(SupportsObject)(ReplicationOffsets::SupportsObject);
	UObject* (*GetArchetype)(UObject*) = decltype(GetArchetype)(ReplicationOffsets::GetArchetype);

	if (!IsNetReady(Conn, false))
		return 0;

	int i = 0;
	for (auto& PriorityActor : PriorityActors) {
		i++;
		auto ActorInfo = PriorityActor.ActorInfo;
		if (ActorInfo == nullptr && PriorityActor.DestructionInfo) {

			GetRepContextLevel(Conn) = PriorityActor.DestructionInfo->Level.Get();

			UActorChannel* Channel = (UActorChannel*)CreateChannelByName(Conn, &ActorFName, EChannelCreateFlags::OpenedLocally, -1);
			if (Channel)
			{
				SetChannelActorForDestroy(Channel, PriorityActor.DestructionInfo);
			}

			GetRepContextLevel(Conn) = nullptr;
			GetDestroyedStartupOrDormantActorGUIDs(Conn).Remove(PriorityActor.DestructionInfo->NetGUID);
			continue;
		}

		UActorChannel* Channel = PriorityActor.Channel;

		if (!Channel || Channel->Actor) {
			auto Actor = ActorInfo->Actor;


			if (ActorInfo->bSwapRolesOnReplicate && Actor && Actor->RemoteRole == ENetRole::ROLE_Authority) {
				auto Role = Actor->Role;
				Actor->Role = Actor->RemoteRole;
				Actor->RemoteRole = Role;
				*(uint32*)(__int64(Channel) + ReplicationOffsets::RelevantTime + 0x14) |= 2;
			}

			if (!Channel && SupportsObject(GetGuidCache(Driver).Get(), Actor->Class, nullptr) && SupportsObject(GetGuidCache(Driver).Get(), Actor->bNetStartup ? Actor : GetArchetype(Actor), nullptr)) {
				Channel = CreateChannelByName(Conn, &ActorFName, EChannelCreateFlags::OpenedLocally, -1);

				if (Channel)
					SetChannelActor(Channel, Actor);
			}
			if (Channel) {
				if (PriorityActor.bRelevant)
					GetRelevantTime(Channel) = GetElapsedTime(Driver) + 0.5 * FRand();
				if (IsNetReady(Channel, false)) {
					if (ReplicateActor(Channel)) {
						/*auto TimeSeconds = GetTimeSeconds(Driver->World);
						const float MinOptimalDelta = 1.f / Actor->NetUpdateFrequency;
						const float MaxOptimalDelta = max(1.f / Actor->MinNetUpdateFrequency, MinOptimalDelta);
						const float DeltaBetweenReplications = float(TimeSeconds - ActorInfo->LastNetReplicateTime);

						ActorInfo->OptimalNetUpdateDelta = clamp(DeltaBetweenReplications * 0.7f, MinOptimalDelta, MaxOptimalDelta);
						ActorInfo->LastNetReplicateTime = TimeSeconds;*/
					}
				}
				else {
					Actor->ForceNetUpdate();
				}

				if (!IsNetReady(Conn, false)) {
					return i;
				}
			}

			if (ActorInfo->bSwapRolesOnReplicate && Actor && Actor->RemoteRole == ENetRole::ROLE_Authority) {
				auto Role = Actor->Role;
				Actor->Role = Actor->RemoteRole;
				Actor->RemoteRole = Role;
				*(uint32*)(__int64(Channel) + ReplicationOffsets::RelevantTime + 0x14) |= 2;
			}
		}
	}

	return PriorityActors.size();
}

void Replication::ServerReplicateActors(UNetDriver* Driver, float DeltaTime)
{
	GetReplicationFrame(Driver)++;

	auto NumConns = PrepConns(Driver);
	if (NumConns == 0)
		return;
	TArray<FNetViewer> AllConns;

	for (auto& Conn : Driver->ClientConnections) {
		if (Conn->PlayerController) 
		{
			auto ConnViewer = ConstructNetViewer(Conn);
			AllConns.Add(ConnViewer);

			TArray<FNetViewer> Viewers;
			Viewers.Reserve(1 + Conn->Children.Num());
			Viewers.Add(ConnViewer);
			for (auto& Child : Conn->Children)
				if (Child->PlayerController)
					Viewers.Add(ConstructNetViewer(Child));

			ViewerMap.Add({ Conn, Viewers });
		}
	}
	BuildConsiderList(Driver, DeltaTime, AllConns);

	auto WorldSettings = Driver->World->K2_GetWorldSettings();
	void (*CloseConnection)(UNetConnection *) = decltype(CloseConnection)(ReplicationOffsets::CloseConnection);


	for (auto& ConnViewer : AllConns) {
		if (!ConnViewer.ViewTarget)
			continue;

		auto& Conn = ConnViewer.Connection;
		if (Conn->PlayerController)
			SendClientAdjustment(Conn->PlayerController);

		for (auto& Child : Conn->Children)
			if (Child->PlayerController)
				SendClientAdjustment(Child->PlayerController);


		auto ViewerPair = ViewerMap.Search([&](TPair<UNetConnection*, TArray<FNetViewer>>& Pair)
		{
			return Pair.First == Conn;
		});
		auto& ConnViewers = ViewerPair->Second;

		PrioritizeActors(Driver, Conn, ConnViewers);
		if (PriorityActors.size() > 0) {
			auto LastProcessedActor = ProcessPrioritizedActors(Driver, Conn, ConnViewers);
			for (size_t i = LastProcessedActor; i < PriorityActors.size(); i++) {
				auto& PriorityActor = PriorityActors[i];
				if (!PriorityActor.ActorInfo)
					continue;

				auto ActorInfo = PriorityActor.ActorInfo;
				auto Actor = ActorInfo->Actor;
				auto Channel = PriorityActor.Channel;

				if (Channel && GetElapsedTime(Driver) - GetRelevantTime(Channel) <= 1.0) {
					ActorInfo->bPendingNetUpdate = true;
				}
				else if (IsActorRelevantToConnection(Actor, ConnViewers)) {
					ActorInfo->bPendingNetUpdate = true;
					if (Channel)
						GetRelevantTime(Channel) = GetElapsedTime(Driver) + 0.5 * FRand();
				}

				if (ActorInfo->ForceRelevantFrame >= GetLastProcessedFrame(Conn))
					ActorInfo->ForceRelevantFrame = GetReplicationFrame(Driver) + 1;
			}
		}
		
		GetLastProcessedFrame(Conn) = GetReplicationFrame(Driver);

		if (GetPendingCloseDueToReplicationFailure(Conn))
			CloseConnection(Conn);
	}

	for (auto& ActorInfoPair : ConsiderList)
		ActorInfoPair.Second.Free();
	for (auto& ViewerPair : ViewerMap)
		ViewerPair.Second.Free();

	ViewerMap.ResetNum();
}
