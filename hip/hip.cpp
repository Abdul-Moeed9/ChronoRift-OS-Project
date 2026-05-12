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
sem_t g_player_turn[4];

struct PlayerThreadArg
{
    int playerId;
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
    int numPlayers = g_state->Status.NumPlayers;
    for (int i = 0; i < numPlayers; i++)
    {
        if (g_state->Players[i].Stunned && g_state->Players[i].StunEndTime <= now)
        {
            g_state->Players[i].Stunned = 0;
            g_state->Players[i].StunEndTime = 0;
        }
    }
}

void submit_action(int playerId, int actionType, int targetId, int weaponId, int ltsIndex, int turnId)
{
    sem_wait(&g_state->GlobalLock);
    if (g_state->Turn.TurnId != turnId || !g_state->Turn.IsPlayerTurn || g_state->Turn.ActiveEntityId != playerId)
    {
        sem_post(&g_state->GlobalLock);
        return;
    }
    g_state->PendingAction.EntityId = playerId;
    g_state->PendingAction.IsPlayer = 1;
    g_state->PendingAction.ActionType = actionType;
    g_state->PendingAction.TargetId = targetId;
    g_state->PendingAction.WeaponId = weaponId;
    g_state->PendingAction.LtsIndex = ltsIndex;
    g_state->PendingAction.TurnId = turnId;
    g_state->PendingAction.Ready = 1;
    sem_post(&g_state->GlobalLock);
    sem_post(&g_state->ActionReady);
}

void request_gui_action_menu(int playerId)
{
    sem_wait(&g_state->GlobalLock);
    g_state->Gui.Phase = 1;
    g_state->Gui.ActivePlayerId = playerId;
    g_state->Gui.Confirmed = 0;
    g_state->Gui.ActionChosen = -1;
    g_state->Gui.TargetChosen = -1;
    g_state->Gui.WeaponChosen = -1;
    g_state->Gui.LtsChosen = -1;
    g_state->Gui.ArtifactChosen = -1;
    g_state->Gui.DropConfirmed = 0;
    sem_post(&g_state->GlobalLock);
}

void request_gui_drop_decision(int playerId)
{
    sem_wait(&g_state->GlobalLock);
    g_state->Gui.Phase = 6;
    g_state->Gui.ActivePlayerId = playerId;
    g_state->Gui.DropConfirmed = 0;
    g_state->Gui.DropChoice = 0;
    sem_post(&g_state->GlobalLock);
}

int wait_for_gui_action()
{
    while (g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_state->GlobalLock);
        int confirmed = g_state->Gui.Confirmed;
        sem_post(&g_state->GlobalLock);

        if (confirmed)
        {
            return 1;
        }
        usleep(50000);
    }
    return 0;
}

int wait_for_gui_drop()
{
    while (g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_state->GlobalLock);
        int confirmed = g_state->Gui.DropConfirmed;
        sem_post(&g_state->GlobalLock);

        if (confirmed)
        {
            return 1;
        }
        usleep(50000);
    }
    return 0;
}

void clear_gui_state()
{
    sem_wait(&g_state->GlobalLock);
    g_state->Gui.Phase = 0;
    g_state->Gui.Confirmed = 0;
    g_state->Gui.DropConfirmed = 0;
    sem_post(&g_state->GlobalLock);
}

void process_turn(int playerId, int turnId)
{
    request_gui_action_menu(playerId);

    int got = wait_for_gui_action();
    if (!got)
    {
        submit_action(playerId, 5, -1, -1, -1, turnId);
        clear_gui_state();
        return;
    }

    sem_wait(&g_state->GlobalLock);
    int action = g_state->Gui.ActionChosen;
    int target = g_state->Gui.TargetChosen;
    int weapon = g_state->Gui.WeaponChosen;
    int lts = g_state->Gui.LtsChosen;
    int artifact = g_state->Gui.ArtifactChosen;
    sem_post(&g_state->GlobalLock);

    clear_gui_state();

    if (action == 9)
    {
        kill(g_state->ArbiterPid, SIGTERM);
        return;
    }

    if (action == 0)
    {
        if (target < 0)
        {
            submit_action(playerId, 5, -1, -1, -1, turnId);
        }
        else
        {
            submit_action(playerId, 0, target, -1, -1, turnId);
        }
    }
    else if (action == 1)
    {
        if (target < 0)
        {
            submit_action(playerId, 5, -1, -1, -1, turnId);
        }
        else
        {
            submit_action(playerId, 1, target, -1, -1, turnId);
        }
    }
    else if (action == 2)
    {
        if (target < 0)
        {
            submit_action(playerId, 5, -1, -1, -1, turnId);
        }
        else
        {
            submit_action(playerId, 2, target, weapon, -1, turnId);
        }
    }
    else if (action == 3)
    {
        submit_action(playerId, 3, -1, -1, lts, turnId);
    }
    else if (action == 4)
    {
        submit_action(playerId, 4, -1, -1, -1, turnId);
    }
    else if (action == 5)
    {
        submit_action(playerId, 5, -1, -1, -1, turnId);
    }
    else if (action == 6)
    {
        submit_action(playerId, 6, -1, -1, -1, turnId);
    }
    else if (action == 7)
    {
        submit_action(playerId, 7, artifact, -1, -1, turnId);
    }
    else
    {
        submit_action(playerId, 5, -1, -1, -1, turnId);
    }
}

void check_weapon_drop(int playerId)
{
    int attempts = 0;
    int found = 0;
    while (attempts < 10 && g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_state->GlobalLock);
        int pending = g_state->DropEvent.Pending;
        int forMe = (g_state->DropEvent.ForPlayerId == playerId);
        int decided = g_state->DropEvent.PlayerDecided;
        sem_post(&g_state->GlobalLock);

        if (pending && forMe && !decided)
        {
            found = 1;
            break;
        }
        if (pending && !forMe)
        {
            return;
        }
        usleep(100000);
        attempts++;
    }

    if (!found)
    {
        return;
    }

    request_gui_drop_decision(playerId);

    int got = wait_for_gui_drop();

    sem_wait(&g_state->GlobalLock);
    if (got)
    {
        g_state->DropEvent.PlayerDecided = 1;
        g_state->DropEvent.PlayerPickedUp = g_state->Gui.DropChoice;
    }
    else
    {
        g_state->DropEvent.PlayerDecided = 1;
        g_state->DropEvent.PlayerPickedUp = 0;
    }
    g_state->Gui.Phase = 0;
    g_state->Gui.DropConfirmed = 0;
    sem_post(&g_state->GlobalLock);
}

void* player_thread_func(void* arg)
{
    PlayerThreadArg* pArg = (PlayerThreadArg*)arg;
    int playerId = pArg->playerId;

    while (g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_player_turn[playerId]);

        if (!g_running || g_state->Status.GameOver)
        {
            break;
        }

        sem_wait(&g_state->Input.InputLock);

        sem_wait(&g_state->GlobalLock);
        int turnId = g_state->Turn.TurnId;
        int activeId = g_state->Turn.ActiveEntityId;
        int isPlayerTurn = g_state->Turn.IsPlayerTurn;
        sem_post(&g_state->GlobalLock);

        if (!isPlayerTurn || activeId != playerId)
        {
            sem_post(&g_state->Input.InputLock);
            continue;
        }

        if (!g_state->Players[playerId].Alive)
        {
            sem_post(&g_state->Input.InputLock);
            submit_action(playerId, 5, -1, -1, -1, turnId);
            continue;
        }

        process_turn(playerId, turnId);
        check_weapon_drop(playerId);

        sem_post(&g_state->Input.InputLock);
    }

    return NULL;
}

void* player_dispatch_func(void* arg)
{
    (void)arg;
    while (g_running && !g_state->Status.GameOver)
    {
        sem_wait(&g_state->PlayerTurnReady);

        if (!g_running || g_state->Status.GameOver)
        {
            int numPlayers = g_state->Status.NumPlayers;
            for (int i = 0; i < numPlayers; i++)
            {
                sem_post(&g_player_turn[i]);
            }
            break;
        }

        int activeId = g_state->Turn.ActiveEntityId;
        if (activeId >= 0 && activeId < g_state->Status.NumPlayers)
        {
            sem_post(&g_player_turn[activeId]);
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout << "HIP: No shmid provided" << endl;
        return 1;
    }

    int shmid = atoi(argv[1]);
    g_state = (SharedState*)shmat(shmid, NULL, 0);
    if (g_state == (SharedState*)-1)
    {
        cout << "HIP: shmat failed" << endl;
        return 1;
    }

    signal(SIGTERM, sigterm_handler);
    signal(SIGUSR1, sigusr1_handler);

    cout << "HIP attached to shared memory. PID: " << getpid() << endl;

    int numPlayers = g_state->Status.NumPlayers;

    for (int i = 0; i < numPlayers; i++)
    {
        sem_init(&g_player_turn[i], 0, 0);
    }

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    pthread_t dispatchThread;
    pthread_create(&dispatchThread, NULL, player_dispatch_func, NULL);

    pthread_t threads[4];
    PlayerThreadArg threadArgs[4];

    for (int i = 0; i < numPlayers; i++)
    {
        threadArgs[i].playerId = i;
        pthread_create(&threads[i], NULL, player_thread_func, &threadArgs[i]);
    }

    pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

    for (int i = 0; i < numPlayers; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_join(dispatchThread, NULL);

    for (int i = 0; i < numPlayers; i++)
    {
        sem_destroy(&g_player_turn[i]);
    }

    cout << "HIP exiting." << endl;
    shmdt(g_state);
    return 0;
}
