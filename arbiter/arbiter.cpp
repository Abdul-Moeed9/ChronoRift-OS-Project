#include <shared.h>
#include "renderer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <pthread.h>
#include <iostream>
using namespace std;

int g_shmid = -1;
SharedState* g_state = NULL;
pid_t g_asp_pid_for_signal = 0;
pthread_t g_deadlock_thread;
pthread_t g_render_thread;
int g_sigterm_received = 0;
int g_asp_frozen = 0;

void add_log(const char* text);
void free_weapon_at(int playerId, int slotStart);

void cleanup_shared_memory()
{
    if (g_state != NULL)
    {
        sem_destroy(&g_state->PlayerTurnReady);
        sem_destroy(&g_state->EnemyTurnReady);
        sem_destroy(&g_state->ActionReady);
        sem_destroy(&g_state->GlobalLock);
        sem_destroy(&g_state->Input.InputLock);
        shmdt(g_state);
        g_state = NULL;
    }
    if (g_shmid != -1)
    {
        shmctl(g_shmid, IPC_RMID, NULL);
        g_shmid = -1;
    }
}

void sigterm_handler(int sig)
{
    (void)sig;
    g_sigterm_received = 1;
}

void sigalrm_handler(int sig)
{
    (void)sig;
    if (g_asp_pid_for_signal > 0)
    {
        kill(g_asp_pid_for_signal, SIGCONT);
    }
    g_asp_frozen = 0;
}

int create_shared_memory()
{
    int key = 12345;
    g_shmid = shmget(key, sizeof(SharedState), IPC_CREAT | 0666);
    if (g_shmid == -1)
    {
        cout << "shmget failed" << endl;
        return -1;
    }
    g_state = (SharedState*)shmat(g_shmid, NULL, 0);
    if (g_state == (SharedState*)-1)
    {
        cout << "shmat failed" << endl;
        g_state = NULL;
        return -1;
    }
    return 0;
}

void init_sync_primitives()
{
    sem_init(&g_state->GlobalLock, 1, 1);
    sem_init(&g_state->Input.InputLock, 1, 1);
    sem_init(&g_state->PlayerTurnReady, 1, 0);
    sem_init(&g_state->EnemyTurnReady, 1, 0);
    sem_init(&g_state->ActionReady, 1, 0);
}

void init_shared_state(int numPlayers, int numEnemies)
{
    char* raw = (char*)g_state;
    for (unsigned long i = 0; i < sizeof(SharedState); i++)
    {
        raw[i] = 0;
    }
    init_sync_primitives();


    g_state->Status.NumPlayers = numPlayers;
    g_state->Status.NumEnemies = numEnemies;
    g_state->Status.GameOver = 0;
    g_state->Status.PlayerWon = 0;
    g_state->Status.EnemiesKilled = 0;
    g_state->Status.EclipseRelicIntroduced = 0;
    g_state->Status.TurnAlternator = 0;
    g_state->Status.SchedulerCounter = 0;

    g_state->ArbiterPid = getpid();

    g_state->Turn.ActiveEntityId = -1;
    g_state->Turn.IsPlayerTurn = 0;
    g_state->Turn.Phase = 0;
    g_state->Turn.TurnId = 0;

    g_state->LogHead = 0;
    g_state->LogCount = 0;

    g_state->DropEvent.Pending = 0;
    g_state->DropEvent.PlayerDecided = 0;

    g_state->Gui.Phase = 0;
    g_state->Gui.ActivePlayerId = -1;
    g_state->Gui.Confirmed = 0;
    g_state->Gui.DropConfirmed = 0;
    g_state->Gui.ActionChosen = -1;
    g_state->Gui.TargetChosen = -1;
    g_state->Gui.WeaponChosen = -1;
    g_state->Gui.LtsChosen = -1;
    g_state->Gui.ArtifactChosen = -1;
    g_state->Gui.DropChoice = 0;
    g_state->Gui.SelectedEnemy = -1;
    g_state->Gui.ShowInventory = 0;
    g_state->Gui.KeyPressed = 0;
}

void init_players(int numPlayers)
{
    int playerSpeed = 100 / numPlayers;
    for (int i = 0; i < numPlayers; i++)
    {
        g_state->Players[i].Hp = 720 + (rand() % 901) + 100;
        g_state->Players[i].MaxHp = g_state->Players[i].Hp;
        g_state->Players[i].Stamina = 0;
        g_state->Players[i].MaxStamina = 100;
        g_state->Players[i].Speed = playerSpeed;
        g_state->Players[i].Damage = 10;
        g_state->Players[i].Alive = 1;
        g_state->Players[i].Stunned = 0;
        g_state->Players[i].StunEndTime = 0;
        g_state->Players[i].Pid = 0;
        g_state->Players[i].HasWeapon = 0;
        g_state->Players[i].WeaponId = -1;
        g_state->Players[i].HoldsArtifact = -1;
        for (int a = 0; a < 3; a++)
        {
            g_state->Players[i].ArtifactHeld[a] = 0;
        }
        g_state->Players[i].WaitingForArtifact = -1;
        g_state->Players[i].ReadyOrder = 0;
    }
    for (int i = numPlayers; i < 4; i++)
    {
        g_state->Players[i].Alive = 0;
    }
}

void init_enemies(int numEnemies)
{
    for (int i = 0; i < numEnemies; i++)
    {
        g_state->Enemies[i].Hp = 20 + (rand() % 151) + 50;
        g_state->Enemies[i].MaxHp = g_state->Enemies[i].Hp;
        g_state->Enemies[i].Stamina = 0;
        g_state->Enemies[i].MaxStamina = 150;
        g_state->Enemies[i].Speed = (rand() % 21) + 10;
        g_state->Enemies[i].Damage = 2 + 10;
        g_state->Enemies[i].Alive = 1;
        g_state->Enemies[i].Stunned = 0;
        g_state->Enemies[i].StunEndTime = 0;
        g_state->Enemies[i].Pid = 0;
        g_state->Enemies[i].HasWeapon = 0;
        g_state->Enemies[i].WeaponId = -1;
        g_state->Enemies[i].HoldsArtifact = -1;
        for (int a = 0; a < 3; a++)
        {
            g_state->Enemies[i].ArtifactHeld[a] = 0;
        }
        g_state->Enemies[i].WaitingForArtifact = -1;
        g_state->Enemies[i].DeathTime = 0;
        g_state->Enemies[i].ReadyOrder = 0;
    }
    for (int i = numEnemies; i < 9; i++)
    {
        g_state->Enemies[i].Alive = 0;
    }
}

void init_inventories(int numPlayers)
{
    for (int p = 0; p < numPlayers; p++)
    {
        for (int s = 0; s < 20; s++)
        {
            g_state->Inventory[p][s].WeaponId = -1;
            g_state->Inventory[p][s].OccupiedBy = -1;
        }
        g_state->Lts[p].Count = 0;
        for (int w = 0; w < 20; w++)
        {
            g_state->Lts[p].Weapons[w].InUse = 0;
        }
    }
}

void init_artifacts()
{
    g_state->Artifacts[0].ArtifactId = 0;
    g_state->Artifacts[0].Free = 1;
    g_state->Artifacts[0].HeldBy = 0;
    g_state->Artifacts[0].EntityId = -1;
    g_state->Artifacts[0].IsPlayer = -1;
    g_state->Artifacts[0].InWorld = 1;

    g_state->Artifacts[1].ArtifactId = 1;
    g_state->Artifacts[1].Free = 1;
    g_state->Artifacts[1].HeldBy = 0;
    g_state->Artifacts[1].EntityId = -1;
    g_state->Artifacts[1].IsPlayer = -1;
    g_state->Artifacts[1].InWorld = 1;

    g_state->Artifacts[2].ArtifactId = 2;
    g_state->Artifacts[2].Free = 1;
    g_state->Artifacts[2].HeldBy = 0;
    g_state->Artifacts[2].EntityId = -1;
    g_state->Artifacts[2].IsPlayer = -1;
    g_state->Artifacts[2].InWorld = 0;
}

void refresh_entity_artifact_status(int entityId, int isPlayer)
{
    int firstHeld = -1;
    if (isPlayer)
    {
        for (int a = 0; a < 3; a++)
        {
            if (g_state->Players[entityId].ArtifactHeld[a])
            {
                firstHeld = a;
                break;
            }
        }
        g_state->Players[entityId].HoldsArtifact = firstHeld;
    }
    else
    {
        for (int a = 0; a < 3; a++)
        {
            if (g_state->Enemies[entityId].ArtifactHeld[a])
            {
                firstHeld = a;
                break;
            }
        }
        g_state->Enemies[entityId].HoldsArtifact = firstHeld;
    }
}

int entity_has_artifact(int entityId, int isPlayer, int artifactId)
{
    if (artifactId < 0 || artifactId > 2)
    {
        return 0;
    }
    if (isPlayer)
    {
        return g_state->Players[entityId].ArtifactHeld[artifactId];
    }
    return g_state->Enemies[entityId].ArtifactHeld[artifactId];
}

int artifact_belongs_to_entity(int artifactId, int entityId, int isPlayer)
{
    if (artifactId < 0 || artifactId > 2)
    {
        return 0;
    }
    if (g_state->Artifacts[artifactId].Free)
    {
        return 0;
    }
    if (g_state->Artifacts[artifactId].EntityId != entityId)
    {
        return 0;
    }
    if (g_state->Artifacts[artifactId].IsPlayer != isPlayer)
    {
        return 0;
    }
    return 1;
}

void clear_artifact_from_lts(int playerId, int artifactId)
{
    for (int i = 0; i < 20; i++)
    {
        if (g_state->Lts[playerId].Weapons[i].InUse)
        {
            if (g_state->Lts[playerId].Weapons[i].WeaponId == artifactId)
            {
                g_state->Lts[playerId].Weapons[i].InUse = 0;
                g_state->Lts[playerId].Weapons[i].WeaponId = -1;
                g_state->Lts[playerId].Weapons[i].SlotStart = 0;
                g_state->Lts[playerId].Weapons[i].SlotSize = 0;
                g_state->Lts[playerId].Weapons[i].Damage = 0;
                g_state->Lts[playerId].Count = g_state->Lts[playerId].Count - 1;
                if (g_state->Lts[playerId].Count < 0)
                {
                    g_state->Lts[playerId].Count = 0;
                }
            }
        }
    }
}

int try_acquire_artifact(int artifactId, int entityId, int isPlayer)
{
    if (artifactId < 0 || artifactId > 2)
    {
        return 0;
    }
    if (!g_state->Artifacts[artifactId].InWorld)
    {
        return 0;
    }
    if (artifact_belongs_to_entity(artifactId, entityId, isPlayer))
    {
        if (isPlayer)
        {
            g_state->Players[entityId].WaitingForArtifact = -1;
        }
        else
        {
            g_state->Enemies[entityId].WaitingForArtifact = -1;
        }
        return 1;
    }
    if (g_state->Artifacts[artifactId].Free)
    {
        g_state->Artifacts[artifactId].Free = 0;
        g_state->Artifacts[artifactId].EntityId = entityId;
        g_state->Artifacts[artifactId].IsPlayer = isPlayer;
        if (isPlayer)
        {
            g_state->Artifacts[artifactId].HeldBy = g_state->HipPid;
            g_state->Players[entityId].ArtifactHeld[artifactId] = 1;
            g_state->Players[entityId].WaitingForArtifact = -1;
        }
        else
        {
            g_state->Artifacts[artifactId].HeldBy = g_state->AspPid;
            g_state->Enemies[entityId].ArtifactHeld[artifactId] = 1;
            g_state->Enemies[entityId].WaitingForArtifact = -1;
        }
        refresh_entity_artifact_status(entityId, isPlayer);
        char logBuf[128];
        if (isPlayer)
        {
            sprintf(logBuf, "Player %d acquired artifact %d", entityId, artifactId);
        }
        else
        {
            sprintf(logBuf, "Enemy %d acquired artifact %d", entityId, artifactId);
        }
        add_log(logBuf);
        return 1;
    }
    if (isPlayer)
    {
        g_state->Players[entityId].WaitingForArtifact = artifactId;
    }
    else
    {
        g_state->Enemies[entityId].WaitingForArtifact = artifactId;
    }
    return 0;
}

void release_artifact(int artifactId, int entityId, int isPlayer)
{
    if (artifactId < 0 || artifactId > 2)
    {
        return;
    }
    if (!artifact_belongs_to_entity(artifactId, entityId, isPlayer))
    {
        return;
    }
    g_state->Artifacts[artifactId].Free = 1;
    g_state->Artifacts[artifactId].HeldBy = 0;
    g_state->Artifacts[artifactId].EntityId = -1;
    g_state->Artifacts[artifactId].IsPlayer = -1;
    if (isPlayer)
    {
        g_state->Players[entityId].ArtifactHeld[artifactId] = 0;
        if (g_state->Players[entityId].WaitingForArtifact == artifactId)
        {
            g_state->Players[entityId].WaitingForArtifact = -1;
        }
        if (artifactId == 0 || artifactId == 1)
        {
            for (int s = 0; s < 20; s++)
            {
                if (g_state->Inventory[entityId][s].WeaponId == artifactId)
                {
                    free_weapon_at(entityId, g_state->Inventory[entityId][s].OccupiedBy);
                    break;
                }
            }
            clear_artifact_from_lts(entityId, artifactId);
        }
    }
    else
    {
        g_state->Enemies[entityId].ArtifactHeld[artifactId] = 0;
        if (g_state->Enemies[entityId].WaitingForArtifact == artifactId)
        {
            g_state->Enemies[entityId].WaitingForArtifact = -1;
        }
    }
    refresh_entity_artifact_status(entityId, isPlayer);
    char logBuf[128];
    if (isPlayer)
    {
        sprintf(logBuf, "Player %d released artifact %d", entityId, artifactId);
    }
    else
    {
        sprintf(logBuf, "Enemy %d released artifact %d", entityId, artifactId);
    }
    add_log(logBuf);
}

void check_and_resolve_deadlock()
{
    int numPlayers = g_state->Status.NumPlayers;
    int numEnemies = g_state->Status.NumEnemies;
    int totalEntities = numPlayers + numEnemies;

    int waits[13];
    int alive[13];
    int isPlayer[13];
    int entityIdx[13];

    for (int i = 0; i < numPlayers; i++)
    {
        waits[i] = g_state->Players[i].WaitingForArtifact;
        alive[i] = g_state->Players[i].Alive;
        isPlayer[i] = 1;
        entityIdx[i] = i;
    }
    for (int i = 0; i < numEnemies; i++)
    {
        int idx = numPlayers + i;
        waits[idx] = g_state->Enemies[i].WaitingForArtifact;
        alive[idx] = g_state->Enemies[i].Alive;
        isPlayer[idx] = 0;
        entityIdx[idx] = i;
    }

    int edge[13];
    for (int i = 0; i < totalEntities; i++)
    {
        edge[i] = -1;
        if (!alive[i] || waits[i] == -1)
        {
            continue;
        }
        if (g_state->Artifacts[waits[i]].Free)
        {
            continue;
        }
        for (int j = 0; j < totalEntities; j++)
        {
            if (i == j || !alive[j])
            {
                continue;
            }
            if (g_state->Artifacts[waits[i]].EntityId == entityIdx[j])
            {
                if (g_state->Artifacts[waits[i]].IsPlayer == isPlayer[j])
                {
                    edge[i] = j;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < totalEntities; i++)
    {
        int hasHeld = 0;
        for (int a = 0; a < 3; a++)
        {
            if (entity_has_artifact(entityIdx[i], isPlayer[i], a))
            {
                hasHeld = 1;
                break;
            }
        }
        if (!hasHeld)
        {
            edge[i] = -1;
        }
    }

    for (int start = 0; start < totalEntities; start++)
    {
        if (edge[start] == -1)
        {
            continue;
        }

        int visited[13];
        for (int v = 0; v < 13; v++)
        {
            visited[v] = 0;
        }

        int current = start;
        int cycleFound = 0;
        while (current != -1 && !visited[current])
        {
            visited[current] = 1;
            current = edge[current];
            if (current == start)
            {
                cycleFound = 1;
                break;
            }
        }

        if (cycleFound)
        {
            char logBuf[128];
            sprintf(logBuf, "DEADLOCK CYCLE detected involving entity %d", start);
            add_log(logBuf);

            int victim = -1;
            int cur = start;
            do
            {
                if (victim == -1)
                {
                    victim = cur;
                }
                else if (!isPlayer[cur] && isPlayer[victim])
                {
                    victim = cur;
                }
                else if (isPlayer[cur] == isPlayer[victim] && entityIdx[cur] > entityIdx[victim])
                {
                    victim = cur;
                }
                cur = edge[cur];
            } while (cur != start);

            int victimArtifact = -1;
            for (int a = 0; a < 3; a++)
            {
                if (entity_has_artifact(entityIdx[victim], isPlayer[victim], a))
                {
                    victimArtifact = a;
                    break;
                }
            }
            if (victimArtifact == -1)
            {
                return;
            }
            release_artifact(victimArtifact, entityIdx[victim], isPlayer[victim]);

            if (isPlayer[victim])
            {
                sprintf(logBuf, "DEADLOCK RESOLVED: Forced Player %d to release artifact %d",
                        entityIdx[victim], victimArtifact);
            }
            else
            {
                sprintf(logBuf, "DEADLOCK RESOLVED: Forced Enemy %d to release artifact %d",
                        entityIdx[victim], victimArtifact);
            }
            add_log(logBuf);
            return;
        }
    }
}

void try_introduce_eclipse_relic()
{
    if (g_state->Status.EclipseRelicIntroduced)
    {
        return;
    }
    if (g_state->Status.EnemiesKilled < 3)
    {
        return;
    }
    int roll = rand() % 100;
    if (roll < 30)
    {
        g_state->Artifacts[2].InWorld = 1;
        g_state->Artifacts[2].Free = 1;
        g_state->Artifacts[2].EntityId = -1;
        g_state->Artifacts[2].HeldBy = 0;
        g_state->Artifacts[2].IsPlayer = -1;
        g_state->Status.EclipseRelicIntroduced = 1;
        add_log("The Eclipse Relic has appeared in the world!");
    }
}

void* deadlock_monitor_func(void* arg)
{
    (void)arg;
    while (!g_state->Status.GameOver)
    {
        sleep(2);
        if (g_state->Status.GameOver)
        {
            break;
        }
        sem_wait(&g_state->GlobalLock);
        check_and_resolve_deadlock();
        sem_post(&g_state->GlobalLock);
    }
    return NULL;
}

void add_log(const char* text)
{
    int idx = g_state->LogHead;
    strncpy(g_state->Log[idx].Text, text, 127);
    g_state->Log[idx].Text[127] = '\0';
    g_state->LogHead = (g_state->LogHead + 1) % 50;
    if (g_state->LogCount < 50)
    {
        g_state->LogCount++;
    }
}

void print_init_summary(int numPlayers, int numEnemies)
{
    cout << "--Chrono Rift--" << endl;
    cout << "Arbiter PID: " << getpid() << endl;
    cout << "Players: " << numPlayers << " and Enemies: " << numEnemies << endl;
    cout << endl;
    cout << "--- Player Stats ---" << endl;
    for (int i = 0; i < numPlayers; i++)
    {
        cout << "Player " << i
             << ": HP=" << g_state->Players[i].Hp
             << "  DMG=" << g_state->Players[i].Damage
             << "  SPD=" << g_state->Players[i].Speed
             << "  MaxStam=" << g_state->Players[i].MaxStamina << endl;
    }
    cout << endl;
    cout << "--- Enemy Stats ---" << endl;
    for (int i = 0; i < numEnemies; i++)
    {
        cout << "Enemy " << i
             << ": HP=" << g_state->Enemies[i].Hp
             << "  DMG=" << g_state->Enemies[i].Damage
             << "  SPD=" << g_state->Enemies[i].Speed
             << "  MaxStam=" << g_state->Enemies[i].MaxStamina << endl;
    }
    cout << endl;
}

pid_t fork_hip(int shmid)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        char shmid_str[16];
        sprintf(shmid_str, "%d", shmid);
        execl("./hip.out", "hip.out", shmid_str, NULL);
        cout << "Failed to exec hip.out" << endl;
        exit(1);
    }
    return pid;
}

pid_t fork_asp(int shmid)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        char shmid_str[16];
        sprintf(shmid_str, "%d", shmid);
        execl("./asp.out", "asp.out", shmid_str, NULL);
        cout << "Failed to exec asp.out" << endl;
        exit(1);
    }
    return pid;
}

void setup_signals()  
{
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    signal(SIGALRM, sigalrm_handler);
}

void wait_for_children()
{
    if (g_state->AspPid > 0)
    {
        kill(g_state->AspPid, SIGCONT);
        kill(g_state->AspPid, SIGTERM);
        waitpid(g_state->AspPid, NULL, 0);
    }
    if (g_state->HipPid > 0)
    {
        kill(g_state->HipPid, SIGTERM);
        waitpid(g_state->HipPid, NULL, 0);
    }
}

void refresh_stuns()
{
    long now = time(NULL);
    int numPlayers = g_state->Status.NumPlayers;
    int numEnemies = g_state->Status.NumEnemies;

    for (int i = 0; i < numPlayers; i++)
    {
        if (g_state->Players[i].Stunned && g_state->Players[i].StunEndTime <= now)  
        {
            g_state->Players[i].Stunned = 0;
            g_state->Players[i].StunEndTime = 0;
            if (g_state->Players[i].Alive && g_state->Players[i].Stamina >= g_state->Players[i].MaxStamina)
            {
                g_state->Status.SchedulerCounter = g_state->Status.SchedulerCounter + 1;
                g_state->Players[i].ReadyOrder = g_state->Status.SchedulerCounter;
            }
        }
    }
    for (int i = 0; i < numEnemies; i++)
    {
        if (g_state->Enemies[i].Stunned && g_state->Enemies[i].StunEndTime <= now)
        {
            g_state->Enemies[i].Stunned = 0;
            g_state->Enemies[i].StunEndTime = 0;
            if (g_state->Enemies[i].Alive && g_state->Enemies[i].Stamina >= g_state->Enemies[i].MaxStamina)
            {
                g_state->Status.SchedulerCounter = g_state->Status.SchedulerCounter + 1;
                g_state->Enemies[i].ReadyOrder = g_state->Status.SchedulerCounter;
            }
        }
    }
}

void tick_stamina()
{
    int numPlayers = g_state->Status.NumPlayers;
    int numEnemies = g_state->Status.NumEnemies;

    refresh_stuns();

    for (int i = 0; i < numPlayers; i++)
    {
        if (g_state->Players[i].Alive && !g_state->Players[i].Stunned)
        {
            if (g_state->Players[i].Stamina < g_state->Players[i].MaxStamina)
            {
                g_state->Players[i].Stamina = g_state->Players[i].Stamina + g_state->Players[i].Speed;
                if (g_state->Players[i].Stamina >= g_state->Players[i].MaxStamina)
                {
                    g_state->Players[i].Stamina = g_state->Players[i].MaxStamina;
                    g_state->Status.SchedulerCounter = g_state->Status.SchedulerCounter + 1;
                    g_state->Players[i].ReadyOrder = g_state->Status.SchedulerCounter;
                }
            }
            else if (g_state->Players[i].ReadyOrder == 0)
            {
                g_state->Status.SchedulerCounter = g_state->Status.SchedulerCounter + 1;
                g_state->Players[i].ReadyOrder = g_state->Status.SchedulerCounter;
            }
        }
    }
    for (int i = 0; i < numEnemies; i++)
    {
        if (g_state->Enemies[i].Alive && !g_state->Enemies[i].Stunned)
        {
            if (g_state->Enemies[i].Stamina < g_state->Enemies[i].MaxStamina)
            {
                g_state->Enemies[i].Stamina = g_state->Enemies[i].Stamina + g_state->Enemies[i].Speed;
                if (g_state->Enemies[i].Stamina >= g_state->Enemies[i].MaxStamina)
                {
                    g_state->Enemies[i].Stamina = g_state->Enemies[i].MaxStamina;
                    g_state->Status.SchedulerCounter = g_state->Status.SchedulerCounter + 1;
                    g_state->Enemies[i].ReadyOrder = g_state->Status.SchedulerCounter;
                }
            }
            else if (g_state->Enemies[i].ReadyOrder == 0)
            {
                g_state->Status.SchedulerCounter = g_state->Status.SchedulerCounter + 1;
                g_state->Enemies[i].ReadyOrder = g_state->Status.SchedulerCounter;
            }
        }
    }
}

int find_ready_entity(int* outIsPlayer)
{
    int numPlayers = g_state->Status.NumPlayers;
    int numEnemies = g_state->Status.NumEnemies;
    int bestId = -1;
    int bestIsPlayer = 0;
    int bestOrder = 0;

    for (int i = 0; i < numPlayers; i++)
    {
        if (g_state->Players[i].Alive && !g_state->Players[i].Stunned)
        {
            if (g_state->Players[i].Stamina >= g_state->Players[i].MaxStamina)
            {
                if (g_state->Players[i].ReadyOrder > 0)
                {
                    if (bestId == -1 || g_state->Players[i].ReadyOrder < bestOrder)
                    {
                        bestId = i;
                        bestIsPlayer = 1;
                        bestOrder = g_state->Players[i].ReadyOrder;
                    }
                }
            }
        }
    }
    if (!g_asp_frozen)
    {
        for (int i = 0; i < numEnemies; i++)
        {
            if (g_state->Enemies[i].Alive && !g_state->Enemies[i].Stunned)
            {
                if (g_state->Enemies[i].Stamina >= g_state->Enemies[i].MaxStamina)
                {
                    if (g_state->Enemies[i].ReadyOrder > 0)
                    {
                        if (bestId == -1 || g_state->Enemies[i].ReadyOrder < bestOrder)
                        {
                            bestId = i;
                            bestIsPlayer = 0;
                            bestOrder = g_state->Enemies[i].ReadyOrder;
                        }
                    }
                }
            }
        }
    }
    if (bestId != -1)
    {
        *outIsPlayer = bestIsPlayer;
    }
    return bestId;
}

int get_weapon_damage(int weaponId)
{
    if (weaponId == 0)
    {
        return 95;
    }
    if (weaponId == 1)
    {
        return 90;
    }
    if (weaponId == 2)
    {
        return 55;
    }
    if (weaponId == 3)
    {
        return 30;
    }
    if (weaponId == 4)
    {
        return 50;
    }
    if (weaponId == 5)
    {
        return 45;
    }
    if (weaponId == 6)
    {
        return 48;
    }
    if (weaponId == 7)
    {
        return 12;
    }
    return 0;
}

int get_weapon_slot_size(int weaponId)
{
    if (weaponId == 0)
    {
        return 10;
    }
    if (weaponId == 1)
    {
        return 10;
    }
    if (weaponId == 2)
    {
        return 7;
    }
    if (weaponId == 3)
    {
        return 4;
    }
    if (weaponId == 4)
    {
        return 6;
    }
    if (weaponId == 5)
    {
        return 5;
    }
    if (weaponId == 6)
    {
        return 6;
    }
    if (weaponId == 7)
    {
        return 2;
    }
    return 0;
}

int allocate_weapon(int playerId, int weaponId)
{
    int slotSize = get_weapon_slot_size(weaponId);
    if (slotSize <= 0)
    {
        return -1;
    }

    for (int start = 0; start <= 20 - slotSize; start++)
    {
        int fits = 1;
        for (int s = start; s < start + slotSize; s++)
        {
            if (g_state->Inventory[playerId][s].WeaponId != -1)
            {
                fits = 0;
                break;
            }
        }
        if (fits)
        {
            for (int s = start; s < start + slotSize; s++)
            {
                g_state->Inventory[playerId][s].WeaponId = weaponId;
                g_state->Inventory[playerId][s].OccupiedBy = start;
            }
            return start;
        }
    }
    return -1;
}

void free_weapon_at(int playerId, int slotStart)
{
    if (slotStart < 0 || slotStart >= 20)
    {
        return;
    }
    int weaponId = g_state->Inventory[playerId][slotStart].WeaponId;
    if (weaponId == -1)
    {
        return;
    }
    int slotSize = get_weapon_slot_size(weaponId);
    for (int s = slotStart; s < slotStart + slotSize && s < 20; s++)
    {
        g_state->Inventory[playerId][s].WeaponId = -1;
        g_state->Inventory[playerId][s].OccupiedBy = -1;
    }
}

int find_free_lts_slot(int playerId)
{
    for (int w = 0; w < 20; w++)
    {
        if (!g_state->Lts[playerId].Weapons[w].InUse)
        {
            return w;
        }
    }
    return -1;
}

int swap_out_to_lts(int playerId, int slotStart)
{
    if (slotStart < 0 || slotStart >= 20)
    {
        return -1;
    }
    int weaponId = g_state->Inventory[playerId][slotStart].WeaponId;
    if (weaponId == -1)
    {
        return -1;
    }

    int ltsSlot = find_free_lts_slot(playerId);
    if (ltsSlot == -1)
    {
        return -1;
    }

    int slotSize = get_weapon_slot_size(weaponId);
    int dmg = get_weapon_damage(weaponId);

    g_state->Lts[playerId].Weapons[ltsSlot].WeaponId = weaponId;
    g_state->Lts[playerId].Weapons[ltsSlot].SlotStart = slotStart;
    g_state->Lts[playerId].Weapons[ltsSlot].SlotSize = slotSize;
    g_state->Lts[playerId].Weapons[ltsSlot].Damage = dmg;
    g_state->Lts[playerId].Weapons[ltsSlot].InUse = 1;
    g_state->Lts[playerId].Count = g_state->Lts[playerId].Count + 1;

    free_weapon_at(playerId, slotStart);

    char logBuf[128];
    sprintf(logBuf, "Player %d swapped weapon %d to LTS slot %d", playerId, weaponId, ltsSlot);
    add_log(logBuf);

    return ltsSlot;
}

int find_weapon_start_slots(int playerId, int starts[], int maxCount)
{
    int count = 0;
    int checked[20];
    for (int i = 0; i < 20; i++)
    {
        checked[i] = 0;
    }

    for (int s = 0; s < 20; s++)
    {
        if (g_state->Inventory[playerId][s].WeaponId != -1 && !checked[s])
        {
            int startSlot = g_state->Inventory[playerId][s].OccupiedBy;
            if (startSlot >= 0 && !checked[startSlot])
            {
                if (count < maxCount)
                {
                    starts[count] = startSlot;
                    count = count + 1;
                }
                int wid = g_state->Inventory[playerId][startSlot].WeaponId;
                int wsize = get_weapon_slot_size(wid);
                for (int k = startSlot; k < startSlot + wsize && k < 20; k++)
                {
                    checked[k] = 1;
                }
            }
        }
    }
    return count;
}

void compact_inventory(int playerId)
{
    int writePos = 0;
    int readPos = 0;

    while (readPos < 20)
    {
        if (g_state->Inventory[playerId][readPos].WeaponId == -1)
        {
            readPos = readPos + 1;
            continue;
        }

        int startSlot = g_state->Inventory[playerId][readPos].OccupiedBy;
        if (startSlot != readPos)
        {
            readPos = readPos + 1;
            continue;
        }

        int weaponId = g_state->Inventory[playerId][readPos].WeaponId;
        int slotSize = get_weapon_slot_size(weaponId);

        if (writePos != readPos)
        {
            for (int s = 0; s < slotSize; s++)
            {
                g_state->Inventory[playerId][writePos + s].WeaponId = weaponId;
                g_state->Inventory[playerId][writePos + s].OccupiedBy = writePos;
            }
            for (int s = readPos; s < readPos + slotSize; s++)
            {
                if (s >= writePos + slotSize)
                {
                    g_state->Inventory[playerId][s].WeaponId = -1;
                    g_state->Inventory[playerId][s].OccupiedBy = -1;
                }
            }
        }

        writePos = writePos + slotSize;
        readPos = readPos + slotSize;
    }
}

int auto_swap_out_for_space(int playerId, int neededSize, int incomingWeaponId)
{
    int bestStarts[20];
    int bestCount = 21;
    int protectedWeapon = -1;

    if (incomingWeaponId == 0)
    {
        protectedWeapon = 1;
    }
    if (incomingWeaponId == 1)
    {
        protectedWeapon = 0;
    }

    for (int windowStart = 0; windowStart <= 20 - neededSize; windowStart++)
    {
        int starts[20];
        int count = 0;
        int containsProtected = 0;
        for (int i = 0; i < 20; i++)
        {
            starts[i] = -1;
        }

        for (int s = windowStart; s < windowStart + neededSize; s++)
        {
            if (g_state->Inventory[playerId][s].WeaponId != -1)
            {
                if (g_state->Inventory[playerId][s].WeaponId == protectedWeapon)
                {
                    containsProtected = 1;
                }
                int startSlot = g_state->Inventory[playerId][s].OccupiedBy;
                int seen = 0;
                for (int k = 0; k < count; k++)
                {
                    if (starts[k] == startSlot)
                    {
                        seen = 1;
                        break;
                    }
                }
                if (!seen)
                {
                    starts[count] = startSlot;
                    count = count + 1;
                }
            }
        }

        if (containsProtected)
        {
            continue;
        }

        if (count < bestCount)
        {
            bestCount = count;
            for (int k = 0; k < count; k++)
            {
                bestStarts[k] = starts[k];
            }
        }
    }

    if (bestCount == 21)
    {
        return -1;
    }

    int freeLts = 0;
    for (int i = 0; i < 20; i++)
    {
        if (!g_state->Lts[playerId].Weapons[i].InUse)
        {
            freeLts = freeLts + 1;
        }
    }
    if (freeLts < bestCount)
    {
        return -1;
    }

    for (int i = 0; i < bestCount; i++)
    {
        int result = swap_out_to_lts(playerId, bestStarts[i]);
        if (result == -1)
        {
            return -1;
        }
    }

    return bestCount;
}

int place_weapon_with_swap(int playerId, int weaponId)
{
    int result = allocate_weapon(playerId, weaponId);
    if (result != -1)
    {
        return result;
    }

    int neededSize = get_weapon_slot_size(weaponId);
    int swapped = auto_swap_out_for_space(playerId, neededSize, weaponId);
    if (swapped == -1)
    {
        return -1;
    }

    result = allocate_weapon(playerId, weaponId);
    return result;
}

int swap_in_from_lts(int playerId, int ltsIndex)
{
    if (ltsIndex < 0 || ltsIndex >= 20)
    {
        return -1;
    }
    if (!g_state->Lts[playerId].Weapons[ltsIndex].InUse)
    {
        return -1;
    }

    int weaponId = g_state->Lts[playerId].Weapons[ltsIndex].WeaponId;
    if (weaponId == 0 || weaponId == 1)
    {
        if (!artifact_belongs_to_entity(weaponId, playerId, 1))
        {
            return -1;
        }
    }

    int result = place_weapon_with_swap(playerId, weaponId);
    if (result == -1)
    {
        return -1;
    }

    g_state->Lts[playerId].Weapons[ltsIndex].InUse = 0;
    g_state->Lts[playerId].Weapons[ltsIndex].WeaponId = -1;
    g_state->Lts[playerId].Weapons[ltsIndex].SlotStart = 0;
    g_state->Lts[playerId].Weapons[ltsIndex].SlotSize = 0;
    g_state->Lts[playerId].Weapons[ltsIndex].Damage = 0;
    g_state->Lts[playerId].Count = g_state->Lts[playerId].Count - 1;
    if (g_state->Lts[playerId].Count < 0)
    {
        g_state->Lts[playerId].Count = 0;
    }

    char logBuf[128];
    sprintf(logBuf, "Player %d swapped in weapon %d from LTS slot %d", playerId, weaponId, ltsIndex);
    add_log(logBuf);

    return result;
}

int find_first_weapon_in_inventory(int playerId)
{
    for (int s = 0; s < 20; s++)
    {
        if (g_state->Inventory[playerId][s].WeaponId != -1)
        {
            return g_state->Inventory[playerId][s].WeaponId;
        }
    }
    return -1;
}

void deliver_stun(int isPlayer, int entityId)
{
    char logBuf[128];

    if (isPlayer)
    {
        g_state->Players[entityId].Stunned = 1;
        g_state->Players[entityId].StunEndTime = time(NULL) + 3;
        g_state->Players[entityId].ReadyOrder = 0;
        sprintf(logBuf, "Player %d is STUNNED for 3 seconds!", entityId);
        add_log(logBuf);
        if (g_state->HipPid > 0)
        {
            kill(g_state->HipPid, SIGUSR1);
        }
    }
    else
    {
        g_state->Enemies[entityId].Stunned = 1;
        g_state->Enemies[entityId].StunEndTime = time(NULL) + 3;
        g_state->Enemies[entityId].ReadyOrder = 0;
        sprintf(logBuf, "Enemy %d is STUNNED for 3 seconds!", entityId);
        add_log(logBuf);
        if (g_state->AspPid > 0)
        {
            kill(g_state->AspPid, SIGUSR1);
        }
    }
}

int roll_weapon_drop()
{
    int roll = rand() % 100;
    if (roll < 18)
    {
        return 2;
    }
    if (roll < 36)
    {
        return 3;
    }
    if (roll < 52)
    {
        return 4;
    }
    if (roll < 68)
    {
        return 5;
    }
    if (roll < 84)
    {
        return 6;
    }
    if (roll < 100)
    {
        return 7;
    }
    return -1;
}

void give_weapon_to_enemy(int weaponId)
{
    int numEnemies = g_state->Status.NumEnemies;
    for (int i = 0; i < numEnemies; i++)
    {
        if (g_state->Enemies[i].Alive && !g_state->Enemies[i].HasWeapon)
        {
            g_state->Enemies[i].HasWeapon = 1;
            g_state->Enemies[i].WeaponId = weaponId;
            char logBuf[128];
            sprintf(logBuf, "Enemy %d picked up unclaimed weapon %d", i, weaponId);
            add_log(logBuf);
            return;
        }
    }
}

void handle_enemy_killed(int targetId)
{
    char logBuf[128];
    g_state->Enemies[targetId].Hp = 0;
    g_state->Enemies[targetId].Alive = 0;
    g_state->Enemies[targetId].DeathTime = time(NULL);
    g_state->Status.EnemiesKilled = g_state->Status.EnemiesKilled + 1;
    sprintf(logBuf, "Enemy %d defeated! Total killed: %d", targetId, g_state->Status.EnemiesKilled);
    add_log(logBuf);

    for (int a = 0; a < 3; a++)
    {
        if (g_state->Enemies[targetId].ArtifactHeld[a])
        {
            release_artifact(a, targetId, 0);
        }
    }

    try_introduce_eclipse_relic();

    if (g_state->Enemies[targetId].HasWeapon)
    {
        return;
    }

    int dropRoll = rand() % 100;
    if (dropRoll < 50)
    {
        int weaponId = roll_weapon_drop();
        if (weaponId != -1)
        {
            int slotSize = get_weapon_slot_size(weaponId);
            int dmg = get_weapon_damage(weaponId);

            g_state->DropEvent.Pending = 1;
            g_state->DropEvent.WeaponId = weaponId;
            g_state->DropEvent.SlotSize = slotSize;
            g_state->DropEvent.WeaponDamage = dmg;
            g_state->DropEvent.ForPlayerId = g_state->Turn.ActiveEntityId;
            g_state->DropEvent.PlayerDecided = 0;
            g_state->DropEvent.PlayerPickedUp = 0;

            sprintf(logBuf, "Enemy %d dropped weapon %d (size=%d dmg=%d)!", targetId, weaponId, slotSize, dmg);
            add_log(logBuf);
        }
    }
}

void handle_player_killed(int targetId)
{
    char logBuf[128];
    g_state->Players[targetId].Hp = 0;
    g_state->Players[targetId].Alive = 0;
    sprintf(logBuf, "Player %d has fallen!", targetId);
    add_log(logBuf);
    for (int a = 0; a < 3; a++)
    {
        if (g_state->Players[targetId].ArtifactHeld[a])
        {
            release_artifact(a, targetId, 1);
        }
    }
}

void process_weapon_drop()
{
    int waited = 0;
    while (waited < 5 && !g_state->Status.GameOver)
    {
        sem_wait(&g_state->GlobalLock);
        int decided = g_state->DropEvent.PlayerDecided;
        sem_post(&g_state->GlobalLock);
        if (decided)
        {
            break;
        }
        sleep(1);
        waited = waited + 1;
    }

    sem_wait(&g_state->GlobalLock);

    if (g_state->DropEvent.PlayerDecided && g_state->DropEvent.PlayerPickedUp)
    {
        int playerId = g_state->DropEvent.ForPlayerId;
        int weaponId = g_state->DropEvent.WeaponId;
        int result = place_weapon_with_swap(playerId, weaponId);
        char logBuf[128];
        if (result != -1)
        {
            sprintf(logBuf, "Player %d picked up weapon %d at slot %d", playerId, weaponId, result);
            add_log(logBuf);
        }
        else
        {
            sprintf(logBuf, "Player %d failed to pick up weapon %d (no space)", playerId, weaponId);
            add_log(logBuf);
            give_weapon_to_enemy(weaponId);
        }
    }
    else
    {
        give_weapon_to_enemy(g_state->DropEvent.WeaponId);
    }

    g_state->DropEvent.Pending = 0;
    g_state->DropEvent.PlayerDecided = 0;
    g_state->DropEvent.PlayerPickedUp = 0;

    sem_post(&g_state->GlobalLock);
}

void apply_strike(int entityId, int isPlayer, int targetId)
{
    char logBuf[128];

    if (isPlayer)
    {
        int dmg = g_state->Players[entityId].Damage;
        g_state->Enemies[targetId].Hp = g_state->Enemies[targetId].Hp - dmg;
        sprintf(logBuf, "Player %d strikes Enemy %d for %d dmg", entityId, targetId, dmg);
        add_log(logBuf);
        if (g_state->Enemies[targetId].Hp <= 0)
        {
            handle_enemy_killed(targetId);
        }
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
    }
    else
    {
        int dmg = g_state->Enemies[entityId].Damage;
        g_state->Players[targetId].Hp = g_state->Players[targetId].Hp - dmg;
        sprintf(logBuf, "Enemy %d strikes Player %d for %d dmg", entityId, targetId, dmg);
        add_log(logBuf);
        if (g_state->Players[targetId].Hp <= 0)
        {
            handle_player_killed(targetId);
        } = 
        g_state->Enemies[entityId].Stamina0;
        g_state->Enemies[entityId].ReadyOrder = 0;

        int stunRoll = rand() % 100;
        if (stunRoll < 15)
        {
            if (g_state->Players[targetId].Alive)
            {
                deliver_stun(1, targetId);
            }
        }
    }
}

void apply_exhaust(int entityId, int targetId)
{
    char logBuf[128];
    int dmg = g_state->Players[entityId].Damage;
    g_state->Enemies[targetId].Stamina = g_state->Enemies[targetId].Stamina - dmg;
    if (g_state->Enemies[targetId].Stamina < 0)
    {
        g_state->Enemies[targetId].Stamina = 0;
    }
    g_state->Enemies[targetId].ReadyOrder = 0;
    sprintf(logBuf, "Player %d exhausts Enemy %d stamina by %d", entityId, targetId, dmg);
    add_log(logBuf);
    g_state->Players[entityId].Stamina = 0;
    g_state->Players[entityId].ReadyOrder = 0;
}

void apply_use_weapon(int entityId, int targetId, int requestedWeaponId)
{
    char logBuf[128];

    int weaponId = -1;
    if (requestedWeaponId != -1)
    {
        for (int s = 0; s < 20; s++)
        {
            if (g_state->Inventory[entityId][s].WeaponId == requestedWeaponId)
            {
                weaponId = requestedWeaponId;
                break;
            }
        }
    }

    if (weaponId == -1)
    {
        weaponId = find_first_weapon_in_inventory(entityId);
    }

    if (weaponId == 0 || weaponId == 1)
    {
        if (!artifact_belongs_to_entity(weaponId, entityId, 1))
        {
            sprintf(logBuf, "Player %d cannot use unheld artifact weapon %d", entityId, weaponId);
            add_log(logBuf);
            weaponId = -1;
        }
    }

    if (weaponId == -1)
    {
        int dmg = g_state->Players[entityId].Damage;
        g_state->Enemies[targetId].Hp = g_state->Enemies[targetId].Hp - dmg;
        sprintf(logBuf, "Player %d strikes Enemy %d for %d dmg (no weapon)", entityId, targetId, dmg);
        add_log(logBuf);
    }
    else
    {
        int dmg = get_weapon_damage(weaponId);
        g_state->Enemies[targetId].Hp = g_state->Enemies[targetId].Hp - dmg;
        sprintf(logBuf, "Player %d uses weapon %d on Enemy %d for %d dmg", entityId, weaponId, targetId, dmg);
        add_log(logBuf);

        int stunRoll = rand() % 100;
        if (stunRoll < 30)
        {
            if (g_state->Enemies[targetId].Alive && g_state->Enemies[targetId].Hp > 0)
            {
                deliver_stun(0, targetId);
            }
        }
    }

    if (g_state->Enemies[targetId].Hp <= 0)
    {
        handle_enemy_killed(targetId);
    }
    g_state->Players[entityId].Stamina = 0;
    g_state->Players[entityId].ReadyOrder = 0;
}

void apply_swap_in(int entityId, int ltsIndex)
{
    char logBuf[128];
    if (ltsIndex < 0 || ltsIndex >= 20)
    {
        sprintf(logBuf, "Player %d swap-in failed: bad index", entityId);
        add_log(logBuf);
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
        return;
    }
    if (!g_state->Lts[entityId].Weapons[ltsIndex].InUse)
    {
        sprintf(logBuf, "Player %d swap-in failed: empty slot", entityId);
        add_log(logBuf);
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
        return;
    }

    int result = swap_in_from_lts(entityId, ltsIndex);
    if (result == -1)
    {
        sprintf(logBuf, "Player %d swap-in failed: could not fit weapon", entityId);
        add_log(logBuf);
    }

    g_state->Players[entityId].Stamina = 0;
    g_state->Players[entityId].ReadyOrder = 0;
}

void apply_heal(int entityId)
{
    char logBuf[128];
    if (g_state->Players[entityId].Hp >= g_state->Players[entityId].MaxHp)
    {
        sprintf(logBuf, "Player %d - HP already full!", entityId);
        add_log(logBuf);
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
        return;
    }
    int heal = g_state->Players[entityId].MaxHp / 10;
    int effective = heal;
    if (g_state->Players[entityId].Hp + heal > g_state->Players[entityId].MaxHp)
    {
        effective = g_state->Players[entityId].MaxHp - g_state->Players[entityId].Hp;
    }
    g_state->Players[entityId].Hp = g_state->Players[entityId].Hp + effective;
    sprintf(logBuf, "Player %d heals for %d HP", entityId, effective);
    add_log(logBuf);
    g_state->Players[entityId].Stamina = 0;
    g_state->Players[entityId].ReadyOrder = 0;
}

void apply_skip(int entityId, int isPlayer)
{
    char logBuf[128];
    if (isPlayer)
    {
        g_state->Players[entityId].Stamina = g_state->Players[entityId].MaxStamina / 2;
        g_state->Players[entityId].ReadyOrder = 0;
        sprintf(logBuf, "Player %d skips turn", entityId);
        add_log(logBuf);
    }
    else
    {
        g_state->Enemies[entityId].Stamina = g_state->Enemies[entityId].MaxStamina / 2;
        g_state->Enemies[entityId].ReadyOrder = 0;
        sprintf(logBuf, "Enemy %d skips turn", entityId);
        add_log(logBuf);
    }
}

void apply_ultimate(int entityId)
{
    char logBuf[128];

    int hasSolar = 0;
    int hasLunar = 0;
    for (int s = 0; s < 20; s++)
    {
        if (g_state->Inventory[entityId][s].WeaponId == 0)
        {
            hasSolar = 1;
        }
        if (g_state->Inventory[entityId][s].WeaponId == 1)
        {
            hasLunar = 1;
        }
    }

    if (!hasSolar || !hasLunar)
    {
        sprintf(logBuf, "Player %d ultimate failed - missing artifacts", entityId);
        add_log(logBuf);
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
        return;
    }

    if (!artifact_belongs_to_entity(0, entityId, 1) || !artifact_belongs_to_entity(1, entityId, 1))
    {
        sprintf(logBuf, "Player %d ultimate failed - missing artifacts", entityId);
        add_log(logBuf);
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
        return;
    }

    if (g_state->AspPid > 0)
    {
        g_asp_frozen = 1;
        kill(g_state->AspPid, SIGSTOP);
    }

    sprintf(logBuf, "Player %d ULTIMATE! ASP frozen for 10 seconds!", entityId);
    add_log(logBuf);

    alarm(10);

    g_state->Players[entityId].Stamina = 0;
    g_state->Players[entityId].ReadyOrder = 0;
}

void apply_acquire_artifact(int entityId, int isPlayer, int artifactId)
{
    char logBuf[128];
    if (artifactId < 0 || artifactId > 2)
    {
        if (isPlayer)
        {
            sprintf(logBuf, "Player %d tried to acquire invalid artifact", entityId);
        }
        else
        {
            sprintf(logBuf, "Enemy %d tried to acquire invalid artifact", entityId);
        }
        add_log(logBuf);
        if (isPlayer)
        {
            g_state->Players[entityId].Stamina = 0;
            g_state->Players[entityId].ReadyOrder = 0;
        }
        else
        {
            g_state->Enemies[entityId].Stamina = 0;
            g_state->Enemies[entityId].ReadyOrder = 0;
        }
        return;
    }
    int hadArtifact = entity_has_artifact(entityId, isPlayer, artifactId);
    int result = try_acquire_artifact(artifactId, entityId, isPlayer);
    if (!result)
    {
        if (isPlayer)
        {
            sprintf(logBuf, "Player %d failed to acquire artifact %d (held by another)", entityId, artifactId);
        }
        else
        {
            sprintf(logBuf, "Enemy %d failed to acquire artifact %d (held by another)", entityId, artifactId);
        }
        add_log(logBuf);
    }
    else if (isPlayer && (artifactId == 0 || artifactId == 1) && !hadArtifact)
    {
        int slot = place_weapon_with_swap(entityId, artifactId);
        if (slot != -1)
        {
            sprintf(logBuf, "Player %d received weapon %d from artifact", entityId, artifactId);
            add_log(logBuf);
        }
        else
        {
            release_artifact(artifactId, entityId, isPlayer);
            sprintf(logBuf, "Player %d could not store artifact %d", entityId, artifactId);
            add_log(logBuf);
        }
    }
    if (isPlayer)
    {
        g_state->Players[entityId].Stamina = 0;
        g_state->Players[entityId].ReadyOrder = 0;
    }
    else
    {
        g_state->Enemies[entityId].Stamina = 0;
        g_state->Enemies[entityId].ReadyOrder = 0;
    }
}

void apply_action()
{
    int entityId = g_state->PendingAction.EntityId;
    int isPlayer = g_state->PendingAction.IsPlayer;
    int actionType = g_state->PendingAction.ActionType;
    int targetId = g_state->PendingAction.TargetId;
    int ltsIndex = g_state->PendingAction.LtsIndex;
    int requestedWeaponId = g_state->PendingAction.WeaponId;

    if (isPlayer)
    {
        if (!g_state->Players[entityId].Alive || g_state->Players[entityId].Stunned)
        {
            apply_skip(entityId, isPlayer);
            g_state->PendingAction.Ready = 0;
            return;
        }
    }
    else
    {
        if (!g_state->Enemies[entityId].Alive || g_state->Enemies[entityId].Stunned)
        {
            apply_skip(entityId, isPlayer);
            g_state->PendingAction.Ready = 0;
            return;
        }
    }

    if (isPlayer && (actionType == 0 || actionType == 1 || actionType == 2))
    {
        if (targetId < 0 || targetId >= g_state->Status.NumEnemies || !g_state->Enemies[targetId].Alive)
        {
            actionType = 5;
        }
    }
    if (!isPlayer && actionType == 0)
    {
        if (targetId < 0 || targetId >= g_state->Status.NumPlayers || !g_state->Players[targetId].Alive)
        {
            actionType = 5;
        }
    }

    if (actionType == 0)
    {
        apply_strike(entityId, isPlayer, targetId);
    }
    else if (actionType == 1)
    {
        if (isPlayer)
        {
            apply_exhaust(entityId, targetId);
        }
    }
    else if (actionType == 2)
    {
        if (isPlayer)
        {
            apply_use_weapon(entityId, targetId, requestedWeaponId);
        }
    }
    else if (actionType == 3)
    {
        if (isPlayer)
        {
            apply_swap_in(entityId, ltsIndex);
        }
    }
    else if (actionType == 4)
    {
        if (isPlayer)
        {
            apply_heal(entityId);
        }
    }
    else if (actionType == 5)
    {
        apply_skip(entityId, isPlayer);
    }
    else if (actionType == 6)
    {
        if (isPlayer)
        {
            apply_ultimate(entityId);
        }
    }
    else if (actionType == 7)
    {
        apply_acquire_artifact(entityId, isPlayer, targetId);
    }

    g_state->PendingAction.Ready = 0;
}

int check_game_over()
{
    if (g_state->Status.EnemiesKilled >= 10)
    {
        g_state->Status.GameOver = 1;
        g_state->Status.PlayerWon = 1;
        add_log("Victory! 10 enemies defeated!");
        return 1;
    }
    int allDead = 1;
    for (int i = 0; i < g_state->Status.NumPlayers; i++)
    {
        if (g_state->Players[i].Alive)
        {
            allDead = 0;
            break;
        }
    }
    if (allDead)
    {
        g_state->Status.GameOver = 1;
        g_state->Status.PlayerWon = 0;
        add_log("Defeat! All players have fallen!");
        return 1;
    }
    return 0;
}

void respawn_dead_enemies()
{
    if (g_state->Status.EnemiesKilled >= 10)
    {
        return;
    }
    int numEnemies = g_state->Status.NumEnemies;
    for (int i = 0; i < numEnemies; i++)
    {
        if (!g_state->Enemies[i].Alive)
        {
            if (g_state->Enemies[i].DeathTime > 0 && (time(NULL) - g_state->Enemies[i].DeathTime) < 5)
            {
                continue;
            }
            g_state->Enemies[i].Hp = 20 + (rand() % 151) + 50;
            g_state->Enemies[i].MaxHp = g_state->Enemies[i].Hp;
            g_state->Enemies[i].Stamina = 0;
            g_state->Enemies[i].MaxStamina = 150;
            g_state->Enemies[i].Speed = (rand() % 21) + 10;
            g_state->Enemies[i].Damage = 2 + 10;
            g_state->Enemies[i].Alive = 1;
            g_state->Enemies[i].Stunned = 0;
            g_state->Enemies[i].StunEndTime = 0;
            g_state->Enemies[i].HasWeapon = 0;
            g_state->Enemies[i].WeaponId = -1;
            g_state->Enemies[i].HoldsArtifact = -1;
            for (int a = 0; a < 3; a++)
            {
                g_state->Enemies[i].ArtifactHeld[a] = 0;
            }
            g_state->Enemies[i].WaitingForArtifact = -1;
            g_state->Enemies[i].ReadyOrder = 0;
            char logBuf[128];
            sprintf(logBuf, "A new Enemy %d has appeared!", i);
            add_log(logBuf);
        }
    }
}

void drain_action_ready()
{
    while (sem_trywait(&g_state->ActionReady) == 0)
    {
        g_state->PendingAction.Ready = 0;
    }
}

int pending_action_is_current(int readyId, int isPlayer, int turnId)
{
    if (!g_state->PendingAction.Ready)
    {
        return 0;
    }
    if (g_state->PendingAction.EntityId != readyId)
    {
        return 0;
    }
    if (g_state->PendingAction.IsPlayer != isPlayer)
    {
        return 0;
    }
    if (g_state->PendingAction.TurnId != turnId)
    {
        return 0;
    }
    return 1;
}

void run_scheduler()
{
    while (!g_state->Status.GameOver && !g_sigterm_received)
    {
        sleep(1);

        if (g_sigterm_received)
        {
            sem_wait(&g_state->GlobalLock);
            g_state->Status.GameOver = 1;
            g_state->Status.PlayerWon = 0;
            sem_post(&g_state->GlobalLock);
            break;
        }

        sem_wait(&g_state->GlobalLock);

        tick_stamina();

        int isPlayer = 0;
        int readyId = find_ready_entity(&isPlayer);
        int currentTurnId = 0;

        if (readyId == -1)
        {
            sem_post(&g_state->GlobalLock);
            continue;
        }

        g_state->PendingAction.Ready = 0;
        drain_action_ready();
        g_state->Turn.TurnId = g_state->Turn.TurnId + 1;
        currentTurnId = g_state->Turn.TurnId;
        g_state->Turn.ActiveEntityId = readyId;
        g_state->Turn.IsPlayerTurn = isPlayer;
        g_state->Turn.Phase = 1;

        char logBuf[128];
        if (isPlayer)
        {
            sprintf(logBuf, "Player %d's turn (Stamina full)", readyId);
        }
        else
        {
            sprintf(logBuf, "Enemy %d's turn (Stamina full)", readyId);
        }
        add_log(logBuf);

        sem_post(&g_state->GlobalLock);

        if (isPlayer)
        {
            sem_post(&g_state->PlayerTurnReady);
        }
        else
        {
            sem_post(&g_state->EnemyTurnReady);
        }

        int waited = 0;
        int gotAction = 0;
        int maxWait = 150;
        if (!isPlayer)
        {
            maxWait = 15;
        }
        while (waited < maxWait && !g_state->Status.GameOver)
        {
            int result = sem_trywait(&g_state->ActionReady);
            if (result == 0)
            {
                sem_wait(&g_state->GlobalLock);
                if (pending_action_is_current(readyId, isPlayer, currentTurnId))
                {
                    gotAction = 1;
                    sem_post(&g_state->GlobalLock);
                    break;
                }
                g_state->PendingAction.Ready = 0;
                sem_post(&g_state->GlobalLock);
            }
            usleep(200000);
            waited = waited + 1;
        }

        sem_wait(&g_state->GlobalLock);

        int dropPending = 0;

        if (gotAction && pending_action_is_current(readyId, isPlayer, currentTurnId))
        {
            apply_action();
            if (isPlayer && g_state->DropEvent.Pending)
            {
                dropPending = 1;
            }
        }
        else
        {
            if (isPlayer)
            {
                g_state->Players[readyId].Stamina = g_state->Players[readyId].MaxStamina / 2;
                g_state->Players[readyId].ReadyOrder = 0;
                sprintf(logBuf, "Player %d timed out - skip", readyId);
            }
            else
            {
                g_state->Enemies[readyId].Stamina = g_state->Enemies[readyId].MaxStamina / 2;
                g_state->Enemies[readyId].ReadyOrder = 0;
                sprintf(logBuf, "Enemy %d timed out - skip", readyId);
            }
            add_log(logBuf);
            g_state->PendingAction.Ready = 0;
        }

        g_state->Turn.ActiveEntityId = -1;
        g_state->Turn.Phase = 0;

        check_game_over();

        if (!g_state->Status.GameOver)
        {
            respawn_dead_enemies();
        }

        if (g_state->Status.GameOver)
        {
            for (int i = 0; i < g_state->Status.NumPlayers; i++)
            {
                sem_post(&g_state->PlayerTurnReady);
            }
            for (int i = 0; i < g_state->Status.NumEnemies; i++)
            {
                sem_post(&g_state->EnemyTurnReady);
            }
        }

        sem_post(&g_state->GlobalLock);

        if (dropPending && !g_state->Status.GameOver)
        {
            process_weapon_drop();
        }
    }
}

int main()
{
    srand(720);

    int numPlayers = 0;
    cout << "Select party size (1-4): ";
    cin >> numPlayers;
    if (numPlayers < 1)
    {
        numPlayers = 1;
    }
    if (numPlayers > 4)
    {
        numPlayers = 4;
    }

    int numEnemies = (rand() % 8) + 2;

    if (create_shared_memory() == -1)
    {
        cout << "Failed to create shared memory" << endl;
        return 1;
    }

    init_shared_state(numPlayers, numEnemies);
    init_players(numPlayers);
    init_enemies(numEnemies);
    init_inventories(numPlayers);
    init_artifacts();

    setup_signals();

    print_init_summary(numPlayers, numEnemies);

    add_log("Game started");

    pid_t hipPid = fork_hip(g_shmid);
    if (hipPid == -1)
    {
        cout << "Failed to fork HIP" << endl;
        cleanup_shared_memory();
        return 1;
    }
    g_state->HipPid = hipPid;
    cout << "HIP forked with PID: " << hipPid << endl;

    pid_t aspPid = fork_asp(g_shmid);
    if (aspPid == -1)
    {
        cout << "Failed to fork ASP" << endl;
        kill(hipPid, SIGTERM);
        waitpid(hipPid, NULL, 0);
        cleanup_shared_memory();
        return 1;
    }
    g_state->AspPid = aspPid;
    g_asp_pid_for_signal = aspPid;
    cout << "ASP forked with PID: " << aspPid << endl;

    cout << "Arbiter scheduler starting..." << endl;

    pthread_create(&g_render_thread, NULL, render_thread_func, (void*)g_state);
    pthread_create(&g_deadlock_thread, NULL, deadlock_monitor_func, NULL);

    run_scheduler();

    stop_renderer();
    pthread_join(g_render_thread, NULL);
    pthread_join(g_deadlock_thread, NULL);

    alarm(0);

    cout << "Game over. Cleaning up..." << endl;
    wait_for_children();
    cleanup_shared_memory();
    cout << "Arbiter exiting." << endl;

    return 0;
}
