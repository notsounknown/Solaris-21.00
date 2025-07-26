// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "Utils.h"
#include "Replication.h"
#include "Options.h"

DWORD WINAPI HotKeyThread(LPVOID)
{
    while (true)
    {
        if (GetAsyncKeyState(VK_F7))
        {
            UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), L"STARTAIRCRAFT", nullptr);
        }

        Sleep(1000 / 30);
    }
}

void Main()
{
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("Sarah 21.00: Setting up");
    Sarah::Offsets::Init();
    ReplicationOffsets::Init();
    auto FrontEndGameMode = (AFortGameMode*) UWorld::GetWorld()->AuthorityGameMode;
    while (FrontEndGameMode->MatchState != L"InProgress");
    Sleep(5000);

    LogCategory = FName(L"LogGameserver");
    MH_Initialize();
    for (auto& HookFunc : _HookFuncs)
        HookFunc();
    MH_EnableHook(MH_ALL_HOOKS);
    *(bool*)Sarah::Offsets::GIsClient = false;
    *(bool*)(Sarah::Offsets::GIsClient + 1) = true;
    srand((uint32_t)time(0));


    Log(L"ImageBase: %p", (LPVOID)Sarah::Offsets::ImageBase);

    CreateThread(0, 0, HotKeyThread, 0, 0, 0);
    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Remove(0);
    UKismetSystemLibrary::ExecuteConsoleCommand(UWorld::GetWorld(), bCreative ? L"open Creative_NoApollo_Terrain" : L"open Artemis_Terrain", nullptr);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        std::thread(Main).detach();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

