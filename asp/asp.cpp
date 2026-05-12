#include <shared.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <iostream>
using namespace std;

SharedState* g_state = NULL;
int g_running = 1;
sem_t g_enemy_turn[9];

struct EnemyThreadArg
{
    int enemyId;
};

void sigterm_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

void sigusr1_handler(int sig)
{
    (void)sig;
    sleep(3);
    long now = time(NULL);
    int numEnemies = g_state->Status.NumEnemies;
    for (int i = 0; i < numEnemies; i++)
    {
        if (g_state->Enemies[i].Stunned && g_state->Enemies[i].StunEndTime <= now)
        {
            g_state->Enemies[i].Stunned = 0;
            g_state->Enemies[i].StunEndTime = 0;
        }
    }
}

int pick_alive_player()
{
    int numPlayers = g_state->Status.NumPlayers;
    int alivePlayers[4];
    int aliveCount = 0;

    for (int i = 0; i < numPlayers; i++)
    {
        if (g_state->Players[i].Alive)
        {
            alivePlayers[aliveCount] = i;
            aliveCount = aliveCount + 1;
        }
    }

    if (aliveCount == 0)
    {
        return -1;
    }

    int pick = rand() % aliveCount;
    return alivePlayers[pick];
}

void submit_enemy_action(int enemyId, int actionType, int targetId, int turnId)
{
    sem_wait(&g_state->GlobalLock);
    if (g_state->Turn.TurnId != turnId || g_state->Turn.IsPlayerTurn || g_state->Turn.ActiveEntityId != enemyId)
    {
        sem_post(&g_state->GlobalLock);
        return;
    }
    g_state->PendingAction.EntityId = enemyId;
    g_state->PendingAction.IsPlayer = 0;
    g_state->PendingAction.ActionType = actionType;
    g_state->PendingAction.TargetId = targetId;
    g_state->PendingAction.WeaponId = -1;
    g_state->PendingAction.LtsIndex = -1;
    g_state->PendingAction.TurnId = turnId;
    g_state->PendingAction.Ready = 1;
    sem_post(&g_state->GlobalLock);
    sem_post(&g_state->ActionReady);
}

void enemy_do_strike(int enemyId, int turnId)
{
    int target = pick_alive_player();
    if (target == -1)
    {
        submit_enemy_action(enemyId, 5, -1, turnId);
        return;
    }
    submit_enemy_action(enemyId, 0, target, turnId);
}

void enemy_do_skip(int enemyId, int turnId)
{
    submit_enemy_action(enemyId, 5, -1, turnId);
}

void enemy_do_acquire_artifact(int enemyId, int turnId)
{
    int freeArtifact = -1;
    for (int i = 0; i < 3; i++)
    {
        if (g_state->Artifacts[i].InWorld && g_state->Artifacts[i].Free)
        {
            freeArtifact = i;
            break;
        }
    }
    if (freeArtifact == -1)
    {
        enemy_do_strike(enemyId, turnId);
        return;
    }
    submit_enemy_action(enemyId, 7, freeArtifact, turnId);
}

void enemy_decide_action(int enemyId, int turnId)
{
    int thinkTime = rand() % 3;
    if (thinkTime > 0)
    {
        sleep(thinkTime);
    }

    if (g_state->Status.GameOver || !g_running)
    {
        return;
    }

    if (!g_state->Enemies[enemyId].Alive)
    {
        enemy_do_skip(enemyId, turnId);
        return;
    }

    int decision = rand() % 10;
    if (decision < 6)
    {
        enemy_do_strike(enemyId, turnId);
    }
    else if (decision < 8)
    {
        enemy_do_skip(enemyId, turnId);
    }
    else
    {
        enemy_do_acquire_artifact(enemyId, turnId);
    }
}

void* enemy_thread_func(void* arg)
{
    EnemyThreadArg* eArg = (EnemyThreadArg*)arg;
    int enemyId = eArg->enemyId;

    while (g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_enemy_turn[enemyId]);

        if (!g_running || g_state->Status.GameOver)
        {
            break;
        }

        sem_wait(&g_state->GlobalLock);
        int turnId = g_state->Turn.TurnId;
        int activeId = g_state->Turn.ActiveEntityId;
        int isPlayerTurn = g_state->Turn.IsPlayerTurn;
        sem_post(&g_state->GlobalLock);

        if (isPlayerTurn || activeId != enemyId)
        {
            continue;
        }

        if (!g_state->Enemies[enemyId].Alive)
        {
            submit_enemy_action(enemyId, 5, -1, turnId);
            continue;
        }


        enemy_decide_action(enemyId, turnId);
    }

    return NULL;
}

void* enemy_dispatch_func(void* arg)
{
    (void)arg;
    while (g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_state->EnemyTurnReady);

        if (!g_running || g_state->Status.GameOver)
        {
            int numEnemies = g_state->Status.NumEnemies;
            for (int i = 0; i < numEnemies; i++)
            {
                sem_post(&g_enemy_turn[i]);
            }
            break;
        }

        int activeId = g_state->Turn.ActiveEntityId;
        if (activeId >= 0 && activeId < g_state->Status.NumEnemies)
        {
            sem_post(&g_enemy_turn[activeId]);
        }
    }
    return NULL;
}

void attach_shared_memory(int shmid)
{
    g_state = (SharedState*)shmat(shmid, NULL, 0);
    if (g_state == (SharedState*)-1)
    {
        cout << "ASP: shmat failed" << endl;
        exit(1);
    }
}

void setup_signals()
{
    signal(SIGTERM, sigterm_handler);
    signal(SIGUSR1, sigusr1_handler);
}

void create_enemy_threads(pthread_t threads[], EnemyThreadArg threadArgs[], int numEnemies)
{
    for (int i = 0; i < numEnemies; i++)
    {
        threadArgs[i].enemyId = i;
        pthread_create(&threads[i], NULL, enemy_thread_func, &threadArgs[i]);
    }
}

void join_enemy_threads(pthread_t threads[], int numEnemies)
{
    for (int i = 0; i < numEnemies; i++)
    {
        pthread_join(threads[i], NULL);
    }
}

void detach_shared_memory()
{
    if (g_state != NULL)
    {
        shmdt(g_state);
        g_state = NULL;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout << "ASP: No shmid provided" << endl;
        return 1;
    }

    int shmid = atoi(argv[1]);

    attach_shared_memory(shmid);

    setup_signals();

    srand(720);

    cout << "ASP attached to shared memory. PID: " << getpid() << endl;

    int numEnemies = g_state->Status.NumEnemies;

    for (int i = 0; i < numEnemies; i++)
    {
        sem_init(&g_enemy_turn[i], 0, 0);
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    pthread_t dispatchThread;
    pthread_create(&dispatchThread, NULL, enemy_dispatch_func, NULL);

    pthread_t threads[9];
    EnemyThreadArg threadArgs[9];

    create_enemy_threads(threads, threadArgs, numEnemies);

    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

    join_enemy_threads(threads, numEnemies);

    pthread_join(dispatchThread, NULL);

    for (int i = 0; i < numEnemies; i++)
    {
        sem_destroy(&g_enemy_turn[i]);
    }

    cout << "ASP exiting." << endl;

    detach_shared_memory();

    return 0;
}
