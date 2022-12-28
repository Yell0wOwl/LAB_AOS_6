#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>

#define SERVER_IP 127.0.0.1
#define SERVER_PORT 6758

int client_socket, count1, count2;

void sig_term(int sig)
{
    close(client_socket);
    exit(0);
}

//Получение пароля и типа команды
char getCommand(char *password)
{
    char buffer[100];
    int len;
    while(true)
    {
        memset(buffer, 0, 100);
        gets(buffer);
        len = strlen(buffer);
        if(len > 6)
        {
            if(!strncmp(buffer, "/start ", 7))
            {
                if(len > 7)
                {
                    password = strncpy(password, buffer + 7, len-7);
                    return '1';
                }
                else
                {
                    printf("Wrong command, try again!");
                    continue;
                }
            }
            else
            {
                if(!strncmp(buffer, "/join ", 6))
                {
                    password = strncpy(password, buffer + 6, len-6);
                    return '2';
                }
                printf("Wrong command, try again!");
                continue;
            }
        }
        else
        {
            if(len > 4)
                if(!strncmp(buffer, "/quit", 5))
                    return '0';
            printf("Wrong command, try again!");
            continue;
        }
    }
}

//Коментарий к результату партии
void comment(int result, char answer, int stage)
{
    if(result == 0)
    {
        printf("Save!\n");
        if((answer=='1' && stage==1)||(answer=='2' && stage==2))
        {
            count2++;
        }
        else
            count1++;
        printf("Count: %i : %i\n", count1, count2);
    }
    if(result == 1)
    {
        printf("Miss!\n");
        printf("Count: %i : %i\n", count1, count2);
    }
    if(result == 2)
    {
        printf("Goal!\n");
        if((answer=='1' && stage==1)||(answer=='2' && stage==2))
        {
            count1++;
        }
        else
            count2++;
        printf("Count: %i : %i\n", count1, count2);
    }
    return NULL;
}

int main()
{
    signal(SIGTERM, sig_term);
    signal(SIGINT, sig_term);

    //Создание сокета
    struct sockaddr_in server = {0};
    int port = SERVER_PORT;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    char password[100] = {0};
    char role, answer;
    printf("Enter\n/start [password] - for starting the game\n/join [password] - for joining the game\n/quit - to quit\n");

    int read_size;

    //Подключение к серверу
    role = getCommand(password);
    if(role == '0')
    {
        close(client_socket);
        return 0;
    }
    connect(client_socket, &server, sizeof(server));
    write(client_socket, &role, 1);
    if(!read(client_socket, &answer, 1))
    {
        close(client_socket);
        printf("Disconnected from server\n");
        exit(0);
    }

    if(answer == '0')
        write(client_socket, password, strlen(password));
    else
        switch(answer)
        {
            case '1':
                printf("The game has already started!\n");
                close(client_socket);
                return 0;
            case '2':
                printf("No game to join!\n");
                close(client_socket);
                return 0;
            case '3':
                printf("No more places!\n");
                close(client_socket);
                return 0;
        }

    if(!read(client_socket, &answer, 1))
    {
        close(client_socket);
        printf("Disconnected from server\n");
        exit(0);
    }

    if(answer == '0')
    {
        printf("Connected! Waiting for the other players...\n");
    }
    else
    {
        printf("Wrong password!\n");
        close(client_socket);
        return 0;
    }


    //Игра
    if(!read(client_socket, &answer, 1))
    {
        close(client_socket);
        printf("Disconnected from server\n");
        exit(0);
    }
    int choice, result;
    count1 = 0;
    count2 = 0;
    int pid = fork();
    if(!pid)
        while(true)
            sleep(60);
    while(true)
    {
        if(!read(client_socket, &result, 4))
        {
            close(client_socket);
            printf("Disconnected from server\n");
            exit(0);
        }
        if(result == 3)
        {
            printf("You won)))\n");
            break;
        }
        if(result == 4)
        {
            printf("You lose(((\n");
            break;
        }
        if(answer == '1')
            printf("Choose attack zone!\n");
        else
            printf("Choose defense zone!\n");
        kill(pid, 2);
        while(true)
        {
            scanf("%i", &choice);
            pid = fork();
            if(!pid)
            {
                while(true)
                {
                    scanf("%i", &choice);
                    printf("Not your turn!\n");
                }
            }
            if(choice >=0 && choice <= 10)
            {
                write(client_socket, &choice, 4);
                break;
            }
            else
                printf("Choose the right zone!\n");
        }
        if(!read(client_socket, &result, 4))
        {
            close(client_socket);
            printf("Disconnected from server\n");
            exit(0);
        }
        comment(result, answer, 1);

        if(answer == '1')
            printf("Choose defense zone!\n");
        else
            printf("Choose attack zone!\n");
        kill(pid, 2);
        while(true)
        {
            scanf("%i", &choice);
            pid = fork();
            if(!pid)
            {
                while(true)
                {
                    scanf("%i", &choice);
                    printf("Not your turn!\n");
                }
            }
            if(choice >=0 && choice <= 10)
            {
                write(client_socket, &choice, 4);
                break;
            }
            else
                printf("Choose the right zone!\n");
        }
        if(!read(client_socket, &result, 4))
        {
            close(client_socket);
            printf("Disconnected from server\n");
            exit(0);
        }
        comment(result, answer, 2);
    }

    close(client_socket);
    return 0;
}
