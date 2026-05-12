#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>


struct WeaponEntry
{
    int WeaponId;
    int SlotStart;
    int SlotSize;
    int Damage;
    int InUse;
};

struct InventorySlot
{
    int WeaponId;
    int OccupiedBy;
};

struct LongTermStorage
{
    WeaponEntry Weapons[20];
    int Count;
};

struct EntityState
{
    int Hp;
    int MaxHp;
    int Stamina;
    int MaxStamina;
    int Speed;
    int Damage;
    int Alive;
    int Stunned;
    long StunEndTime;
    pid_t Pid;
    int HasWeapon;
    int WeaponId;
    int HoldsArtifact;
    int ArtifactHeld[3];
    int WaitingForArtifact;
    long DeathTime;
    int ReadyOrder;
};

struct ArtifactEntry
{
    int ArtifactId;
    int Free;
    pid_t HeldBy;
    int EntityId;
    int IsPlayer;
    int InWorld;
};

struct ActionRequest
{
    int Ready;
    int EntityId;
    int IsPlayer;
    int ActionType;
    int TargetId;
    int WeaponId;
    int LtsIndex;
    int TurnId;
};

struct TurnControl
{
    int ActiveEntityId;
    int IsPlayerTurn;
    int Phase;
    long TurnStartTime;
    int TurnId;
};

struct InputBuffer
{
    int Ready;
    int ActionType;
    int TargetId;
    int WeaponId;
    int LtsIndex;
    sem_t InputLock;
};

struct GuiInput
{
    int Phase;
    int ActivePlayerId;
    int ActionChosen;
    int TargetChosen;
    int WeaponChosen;
    int LtsChosen;
    int ArtifactChosen;
    int DropChoice;
    int Confirmed;
    int DropConfirmed;
    int SelectedEnemy;
    int ShowInventory;
    int KeyPressed;
};

struct ActionLogEntry
{
    char Text[128];
};

struct WeaponDropEvent
{
    int Pending;
    int WeaponId;
    int SlotSize;
    int WeaponDamage;
    int ForPlayerId;
    int PlayerDecided;
    int PlayerPickedUp;
};

struct GameStatus
{
    int EnemiesKilled;
    int GameOver;
    int PlayerWon;
    int NumPlayers;
    int NumEnemies;
    int EclipseRelicIntroduced;
    int TurnAlternator;
    int SchedulerCounter;
};

struct SharedState
{
    EntityState Players[4];
    EntityState Enemies[9];
    InventorySlot Inventory[4][20];
    LongTermStorage Lts[4];
    ArtifactEntry Artifacts[3];
    ActionRequest PendingAction;
    TurnControl Turn;
    InputBuffer Input;
    GuiInput Gui;
    GameStatus Status;
    WeaponDropEvent DropEvent;
    ActionLogEntry Log[50];
    int LogHead;
    int LogCount;
    sem_t GlobalLock;
    sem_t PlayerTurnReady;
    sem_t EnemyTurnReady;
    sem_t ActionReady;
    pid_t ArbiterPid;
    pid_t HipPid;
    pid_t AspPid;
};
