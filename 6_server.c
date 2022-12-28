#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <time.h>
#include <string.h>

#define CONFIG_NAME "config"

struct configstruct
{
    int port;
    char *logfile;
};

typedef struct
{
    bool started;
    bool created;
    int places;
    char password[100];
    int count1;
    int count2;
    int defend;
    int attack;
    int result;
}gamestruct;

struct configstruct config = {0};
gamestruct *game;
int pid1 = -1;
int pid2 = -1;
int pid3 = -1;

int client1_socket;
int client2_socket;
int client3_socket;

void sig_term(int sig)
{
    shmdt(game);
    close(client1_socket);
    close(client2_socket);
    close(client3_socket);
    exit(0);
}

void sig_hup(int sig)
{
    parseConfig();
}

//Функция считывания файла конфигурации
void parseConfig()
{
    FILE *config_file = fopen(CONFIG_NAME, "r+");

    int port;
    char *logfile;
    char buf[100] = {0};
    fscanf(config_file, "port = %s\n", buf);
    port = atoi(buf);
    memset(buf, 0, 100);
    fscanf(config_file, "logfile = %s", buf);
    logfile = calloc(strlen(buf) + 1, sizeof(char));
    strcpy(logfile, buf);

    fclose(config_file);

    config.logfile = logfile;
    config.port = port;

    return NULL;
}

//Функция для рассчета результата партии
int strike(int attack, int defend, int att_player)
{
    srand(time(NULL));
    int miss_chance = 0;
    int save_chance = 0;
    int result, random;

    if(attack == 0 || attack == 2 || attack == 3)
        miss_chance = 0;
    if(attack == 1 || attack == 4)
        miss_chance = 20;
    if(attack == 7 || attack == 8)
        miss_chance = 30;
    if(attack == 6 || attack == 9)
        miss_chance = 40;
    if(attack == 5 || attack == 10)
        miss_chance = 50;

    if(defend == attack)
        save_chance = 95;
    if(defend == 0 && (attack == 2 || attack == 3))
        save_chance = 70;
    if(defend == 0 && (attack == 1 || attack == 4))
        save_chance = 50;

    game->attack = -1;
    game->defend = -1;

    random = rand() % 100;
    if(random < miss_chance)
        return 1;
    else
    {
        random = rand() % 100;
        if(random < save_chance)
        {
            if(att_player == 1)
                game->count2++;
            else
                game->count1++;
            return 0;
        }
        else
        {
            if(att_player == 1)
                game->count1++;
            else
                game->count2++;
            return 2;
        }
    }
}

//Функция для записи логов
void log(int logfile_local, char *s)
{
    long long t = time(NULL);
    char *ts = ctime(&t);
    char *fts = strtok(ts, " ");
    fts = strtok(NULL, " ");
    fts = strtok(NULL, " ");
    fts = strtok(NULL, " ");
    if(fts == NULL)
        fts = "\t";
    write(logfile_local, fts, strlen(fts));
    write(logfile_local, "\t\t", strlen("\t\t"));
    write(logfile_local, s, strlen(s));
    write(logfile_local, "\n", strlen("\n"));
}

int main()
{

    if(fork())
    {
        return 0;
    }
    else
    {
        setsid();
    }

    //Обработка сигналов
    signal(SIGTERM, sig_term);
    signal(SIGINT, sig_term);
    signal(SIGHUP, sig_hup);

    parseConfig();

    //Создание файла с логами
    int t = time(NULL);
    char *t_str = ctime(&t);
    int loc;
    while((loc = strchr(t_str, ' ')) != NULL)
    {
        t_str[loc - (int)t_str] = '_';
    }
    char *log_name = calloc(strlen(t_str) + 2 + strlen(config.logfile), sizeof(char));
    strcpy(log_name, config.logfile);
    strcat(log_name, "_");
    strcat(log_name, t_str);
    int logfile_local = open(log_name, O_WRONLY | O_CREAT, 0777);
    log(logfile_local, "Log created");

    //Создание серверного сокета
    struct sockaddr_in server = {0};
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(config.port);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    bind(server_socket, &server, sizeof(server));
    listen(server_socket, 5);

    //Сознание сокетов клиентов
    struct sockaddr_in client1 = {0};
    struct sockaddr_in client2 = {0};
    struct sockaddr_in client3 = {0};
    int client1_len;
    int client2_len;
    int client3_len;
    char msg1[100];
    char msg2[100];
    char msg3[100];
    int msg1_len, msg2_len, msg3_len;

    //Создание главной структуры в разделяемой памяти, а также семафора для блокировки доступа
    int shmkey = ftok("shmfile", 1);
    int semkey = ftok("semfile", 1);
    int shmid = shmget (shmkey, sizeof(gamestruct), IPC_CREAT | 0777);
    int semid = semget (semkey, 1, IPC_CREAT | 0777);
    semctl(semid, 0, SETVAL, 0);
    game = (gamestruct*)shmat(shmid, NULL, 0);
    game->started = false;
    game->created = false;
    game->places = 2;
    game->defend = -1;
    game->attack = -1;
    game->result = -1;
    game->count1 = 0;
    game->count2 = 0;
    memset(game->password, 0, 100);

    //Коды, которыми сервер общается с клиентами
    char zero_code = '0';
    char one_code = '1';
    char two_code = '2';
    char three_code = '3';
    int choise, result;

    //Первый и второй процесс обрабатывают двух игроков на свободных местах, третий активируется когда места заняты и сообщает об этом новым подключившимся
    pid1 = fork();
    if(!pid1)
    {
        log(logfile_local, "Creating 1st process");
        while(true)
        {
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            //Авторизация
            while(true)
            {
                client1_socket = accept(server_socket, &client1, &client1_len);
                game->places--;
                log(logfile_local, "1st user connected");
                read(client1_socket, msg1, 1);
                if(msg1[0] == '1')
                {
                    if(!game->created)
                    {
                        write(client1_socket, &zero_code, 1);
                        msg1_len = read(client1_socket, msg1, 100);
                        write(client1_socket, &zero_code, 1);
                        strncpy(game->password, msg1, msg1_len);
                        game->created = true;
                        log(logfile_local, "1st player joined the game");
                        log(logfile_local, "Game created");
                        break;
                    }
                    else
                    {
                        write(client1_socket, &one_code, 1);
                        close(client1_socket);
                        game->places++;
                        log(logfile_local, "1st user disconnected");
                        continue;
                    }
                }
                if(msg1[0] == '2')
                {
                    if(game->created)
                    {
                        write(client1_socket, &zero_code, 1);
                        msg1_len = read(client1_socket, msg1, 100);
                        if(!strncmp(game->password, msg1, msg1_len))
                        {
                            game->started = true;
                            write(client1_socket, &zero_code, 1);
                            log(logfile_local, "2nd player joined the game");
                            break;
                        }
                        else
                        {
                            write(client1_socket, &one_code, 1);
                            close(client1_socket);
                            game->places++;
                            log(logfile_local, "1st user disconnected");
                            continue;
                        }
                    }
                    else
                    {
                        write(client1_socket, &two_code, 1);
                        close(client1_socket);
                        game->places++;
                        log(logfile_local, "1st user disconnected");
                        continue;
                    }
                }
            }
            semctl(semid, 0, SETVAL, 0);

            //Ожидание второго игрока
            while(!game->started)
            {
                log(logfile_local, "Waiting for the 2nd player!");
                sleep(3);
            }

            log(logfile_local, "Game started!");

            write(client1_socket, &one_code, 1);

            //Начало игры
            for(int i=0; i<5; i++)
            {
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                game->result = -1;
                semctl(semid, 0, SETVAL, 0);
                result = -1;
                write(client1_socket, &result, 4);
                read(client1_socket, &choise, 4);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                game->attack = choise;
                semctl(semid, 0, SETVAL, 0);
                while(game->defend == -1)
                    sleep(1);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                result = strike(game->attack, game->defend, 1);
                game->result = result;
                semctl(semid, 0, SETVAL, 0);
                write(client1_socket, &result, 4);

                read(client1_socket, &choise, 4);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                game->defend = choise;
                semctl(semid, 0, SETVAL, 0);
                while(game->result == -1)
                    sleep(1);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                result = game->result;
                semctl(semid, 0, SETVAL, 0);
                write(client1_socket, &result, 4);
            }

            while(game->count1 == game->count2)
            {
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                game->result = -1;
                semctl(semid, 0, SETVAL, 0);
                result = -1;
                write(client1_socket, &result, 4);
                read(client1_socket, &choise, 4);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                game->attack = choise;
                semctl(semid, 0, SETVAL, 0);
                while(game->defend == -1)
                    sleep(1);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                result = strike(game->attack, game->defend, 1);
                game->result = result;
                semctl(semid, 0, SETVAL, 0);
                write(client1_socket, &result, 4);

                read(client1_socket, &choise, 4);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                game->defend = choise;
                semctl(semid, 0, SETVAL, 0);
                while(game->result == -1)
                    sleep(1);
                while(semctl(semid, 0, GETVAL, 0));
                semctl(semid, 0, SETVAL, 1);
                result = game->result;
                semctl(semid, 0, SETVAL, 0);
                write(client1_socket, &result, 4);
            }

            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            if(game->count1 > game->count2)
                result = 3;
            else
                result = 4;
            semctl(semid, 0, SETVAL, 0);
            write(client1_socket, &result, 4);


            log(logfile_local, "Game ended!");

            return 0;
        }
    }

    pid2 = fork();
    if(!pid2)
    {
        log(logfile_local, "Creating 2nd process");

        while(semctl(semid, 0, GETVAL, 0));
        semctl(semid, 0, SETVAL, 1);
        //Авторизация
        while(true)
        {
            client2_socket = accept(server_socket, &client2, &client2_len);
            game->places--;
            log(logfile_local, "2nd user connected");
            read(client2_socket, msg2, 1);
            if(msg2[0] == '1')
            {
                if(!game->created)
                {
                    write(client2_socket, &zero_code, 1);
                    msg2_len = read(client2_socket, msg2, 100);
                    write(client2_socket, &zero_code, 1);
                    strncpy(game->password, msg2, msg2_len);
                    game->created = true;
                    log(logfile_local, "1st player joined the game");
                    log(logfile_local, "Game created");
                    break;
                }
                else
                {
                    write(client2_socket, &one_code, 1);
                    close(client2_socket);
                    game->places++;
                    log(logfile_local, "2nd user disconnected");
                    continue;
                }
            }
            if(msg2[0] == '2')
            {
                if(game->created)
                {
                    write(client2_socket, &zero_code, 1);
                    msg2_len = read(client2_socket, msg2, 100);
                    if(!strncmp((*game).password, msg2, msg2_len))
                    {
                        game->started = true;
                        write(client2_socket, &zero_code, 1);
                        log(logfile_local, "2nd player joined the game");
                        break;
                    }
                    else
                    {
                        write(client2_socket, &one_code, 1);
                        close(client2_socket);
                        game->places++;
                        log(logfile_local, "2nd user disconnected");
                        continue;
                    }
                }
                else
                {
                    write(client2_socket, &two_code, 1);
                    close(client2_socket);
                    game->places++;
                    log(logfile_local, "2nd user disconnected");
                    continue;
                }
            }
        }
        semctl(semid, 0, SETVAL, 0);

        //Ожидание авторизации другого игрока
        while(!game->started)
        {
            log(logfile_local, "Waiting for the 2nd player!");
            sleep(3);
        }

        write(client2_socket, &two_code, 1);

        //Начало игры
        for(int i=0; i<5; i++)
        {
            result = -1;
            write(client2_socket, &result, 4);
            read(client2_socket, &choise, 4);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            game->defend = choise;
            semctl(semid, 0, SETVAL, 0);
            while(game->result == -1)
                sleep(1);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            result = game->result;
            semctl(semid, 0, SETVAL, 0);
            write(client2_socket, &result, 4);

            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            game->result = -1;
            semctl(semid, 0, SETVAL, 0);
            read(client2_socket, &choise, 4);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            game->attack = choise;
            semctl(semid, 0, SETVAL, 0);
            while(game->defend == -1)
                sleep(1);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            result = strike(game->attack, game->defend, 2);
            game->result = result;
            semctl(semid, 0, SETVAL, 0);
            write(client2_socket, &result, 4);
        }

        while(game->count1 == game->count2)
        {
            result = -1;
            write(client2_socket, &result, 4);
            read(client2_socket, &choise, 4);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            game->defend = choise;
            semctl(semid, 0, SETVAL, 0);
            while(game->result == -1)
                sleep(1);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            result = game->result;
            semctl(semid, 0, SETVAL, 0);
            write(client2_socket, &result, 4);

            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            game->result = -1;
            semctl(semid, 0, SETVAL, 0);
            read(client2_socket, &choise, 4);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            game->attack = choise;
            semctl(semid, 0, SETVAL, 0);
            while(game->defend == -1)
                sleep(1);
            while(semctl(semid, 0, GETVAL, 0));
            semctl(semid, 0, SETVAL, 1);
            result = strike(game->attack, game->defend, 2);
            game->result = result;
            semctl(semid, 0, SETVAL, 0);
            write(client2_socket, &result, 4);
        }

        while(semctl(semid, 0, GETVAL, 0));
        semctl(semid, 0, SETVAL, 1);
        if(game->count1 < game->count2)
            result = 3;
        else
            result = 4;
        semctl(semid, 0, SETVAL, 0);
        write(client2_socket, &result, 4);

        return 0;
    }

    int pid3 = fork();
    if(!pid3)
    {
        while(true)
        {
            if(!game->places)
            {
                client3_socket = accept(server_socket, &client3, &client3_len);
                log(logfile_local, "3rd user tried to connect");
                read(client3_socket, msg3, 1);
                write(client3_socket, &three_code, 1);
                close(client3_socket);
            }
        }
    }

    wait(pid1);
    wait(pid2);
    kill(pid3, 2);

    close(client1_socket);
    close(client2_socket);

    log(logfile_local, "Cleaning up");
    close(log);

    return 0;
}
