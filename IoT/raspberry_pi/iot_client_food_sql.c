#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <mysql/mysql.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5
#define STM32_ID "LDY_STM" 

void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);

char name[NAME_SIZE] = "[Default]";
char msg[BUF_SIZE];

int main(int argc, char* argv[])
{
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void* thread_return;

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:PASSWD]", name);
    write(sock, msg, strlen(msg));
    
    // 쓰레드 생성 (수신/송신 분리)
    pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
    pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);

    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    if(sock != -1)
        close(sock);
    return 0;
}

void* send_msg(void* arg)
{
    int* sock = (int*)arg;
    int ret;
    fd_set initset, newset;
    struct timeval tv;
    char name_msg[NAME_SIZE + BUF_SIZE + 2];

    FD_ZERO(&initset);
    FD_SET(STDIN_FILENO, &initset);

    while (1) {
        memset(msg, 0, sizeof(msg));
        name_msg[0] = '\0';
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        newset = initset;
        ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
        if (FD_ISSET(STDIN_FILENO, &newset))
        {
            fgets(msg, BUF_SIZE, stdin);
            if (!strncmp(msg, "quit\n", 5)) {
                *sock = -1;
                return NULL;
            }
            else if (msg[0] != '[')
            {
                strcat(name_msg, "[ALLMSG]");
                strcat(name_msg, msg);
            }
            else
                strcpy(name_msg, msg);
            if (write(*sock, name_msg, strlen(name_msg)) <= 0)
            {
                *sock = -1;
                return NULL;
            }
        }
        if (ret == 0)
        {
            if (*sock == -1)
                return NULL;
        }
    }
}

void* recv_msg(void* arg)
{
    MYSQL* conn;
    MYSQL_ROW sqlrow;
    MYSQL_RES *result;
    char sql_cmd[256] = { 0 };
    
    char* host = "10.10.141.61";
    char* user = "iot";
    char* pass = "pwiot";
    char* dbname = "iotdb";

    int* sock = (int*)arg;
    int i;
    char* pToken;
    char* pArray[ARR_CNT] = { 0 };
    char name_msg[NAME_SIZE + BUF_SIZE + 1];
    int str_len;

    conn = mysql_init(NULL);

    if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0)))
    {
        fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
        exit(1);
    }
    else
        printf("DB Connection Successful!\n");

    while (1) {
        memset(name_msg, 0x0, sizeof(name_msg));
        str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
        if (str_len <= 0)
        {
            *sock = -1;
            return NULL;
        }
        
        // [핵심 수정] \r과 \n을 모두 찾아서 문자열을 끝냅니다.
        name_msg[strcspn(name_msg, "\r\n")] = '\0';

        // 구분자 분리
        pToken = strtok(name_msg, "[:@]]"); 
        i = 0;
        while (pToken != NULL)
        {
            pArray[i] = pToken;
            if ( ++i >= ARR_CNT) break;
            pToken = strtok(NULL, "[:@]]");
        }
        
        if(pArray[1] != NULL && !strcmp(pArray[1], "RFID")) {
            char *tag_uid = pArray[2]; 
            printf(" >> RFID Tag Detected: [%s]\n", tag_uid); // 대괄호[]로 감싸서 공백 확인

            char stu_name[20] = "Unknown";
            char stu_allergy[50] = "None"; 
            int is_registered = 0;

            // 1. 학생 정보 조회
            sprintf(sql_cmd, "SELECT name, allergy FROM student WHERE uid = '%s'", tag_uid);
            if(mysql_query(conn, sql_cmd) == 0) {
                result = mysql_store_result(conn);
                if(result != NULL && mysql_num_rows(result) > 0) {
                    sqlrow = mysql_fetch_row(result);
                    if(sqlrow[0]) strcpy(stu_name, sqlrow[0]);
                    if(sqlrow[1]) strcpy(stu_allergy, sqlrow[1]);
                    is_registered = 1;
                    printf(" >> Student Found: %s (Allergy: %s)\n", stu_name, stu_allergy);
                } else {
                    printf(" >> Unregistered Card (Query: %s)\n", sql_cmd); // 쿼리문 출력해서 확인
                }
                mysql_free_result(result);
            }

            // 2. 오늘 식단 조회 및 비교
            int danger_count = 0;
            char danger_menu_list[512] = {0}; 
            char safe_food_sample[50] = "No Meal"; 

            sprintf(sql_cmd, "SELECT food_name, allergy FROM menu WHERE menu_date = CURDATE()");
            
            if(mysql_query(conn, sql_cmd) == 0) {
                result = mysql_store_result(conn);
                if(result != NULL) {
                    int menu_count = 0;
                    while((sqlrow = mysql_fetch_row(result))) {
                        menu_count++;
                        char current_food[50] = {0};
                        char current_allergy[50] = {0};

                        if(sqlrow[0]) strcpy(current_food, sqlrow[0]);
                        if(sqlrow[1]) strcpy(current_allergy, sqlrow[1]);

                        if(menu_count == 1) strcpy(safe_food_sample, current_food);

                        int current_menu_is_danger = 0;
                        if(strcmp(stu_allergy, "None") != 0) {
                            char allergy_copy[50];
                            strcpy(allergy_copy, stu_allergy);
                            char *ptr = strtok(allergy_copy, ",");
                            while(ptr != NULL) {
                                while(*ptr == ' ') ptr++;
                                if(strstr(current_allergy, ptr) != NULL) {
                                    current_menu_is_danger = 1;
                                    break; 
                                }
                                ptr = strtok(NULL, ",");
                            }
                        }

                        if(current_menu_is_danger) {
                            danger_count++;
                            strcat(danger_menu_list, "@");
                            strcat(danger_menu_list, current_food);
                        }
                    }
                    if(menu_count == 0) printf(" >> No Menu Data for Today!\n");
                    mysql_free_result(result);
                }
            } else {
                printf(" >> DB Query Error! Did you add 'food_name' column?\n");
            }

            // 3. 전송
            char stm_cmd[512]; 

            if(is_registered == 0) {
                sprintf(stm_cmd, "[%s]UNKNOWN@Card@Error\n", STM32_ID);
            }
            else if(danger_count > 0) {
                printf(" >> WARNING! %d Dangerous menus found.\n", danger_count);
                sprintf(stm_cmd, "[%s]WARNING@%s(%d)%s\n", STM32_ID, stu_name, danger_count, danger_menu_list);
            }
            else {
                printf(" >> Safe. Enjoy your meal.\n");
                
                // [수정] 안전할 경우 메뉴 이름 대신 무조건 "NONE"을 보냅니다.
                sprintf(stm_cmd, "[%s]SAFE@%s@NONE\n", STM32_ID, stu_name);
            }

            printf(" >> Sending: %s", stm_cmd);
            write(*sock, stm_cmd, strlen(stm_cmd));
        }
    }
    mysql_close(conn);
    return NULL;
}

void error_handling(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}