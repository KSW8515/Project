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
#include <sys/select.h>
#include <unistd.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define PSWD_SIZE 20
#define ARR_CNT 5

#define ARDID "DOOR1"
#define STMID "DOOR2"

void* send_msg(void* arg);
void* recv_msg(void* arg);
void error_handling(char* msg);
void insert_attendance(MYSQL* conn, char* uid);
int check_attendance(MYSQL* conn, char* name);
int check_leave(MYSQL* conn, char* name);

char name[NAME_SIZE] = "[Default]";
char pswd[PSWD_SIZE] = "[Default]";
char msg[BUF_SIZE];

int main(int argc, char* argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void* thread_return;

	if (argc != 5) {
		printf("Usage : %s <IP> <port> <name> <passwd>\n", argv[0]);
		exit(1);
	}

	sprintf(name, "%s", argv[3]);
	sprintf(pswd, "%s", argv[4]);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if (connect(sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error");

	sprintf(msg, "[%s:%s]", name, pswd);
	write(sock, msg, strlen(msg));
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
	int str_len;
	int ret;
	fd_set initset, newset;
	struct timeval tv;
	char name_msg[NAME_SIZE + BUF_SIZE + 2];

	FD_ZERO(&initset);
	FD_SET(STDIN_FILENO, &initset);

	fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n", stdout);
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
	int res;
	char sql_cmd[200] = { 0 };
	char msg[50] = { 0 };
	char* host = "localhost";
	char* user = "iot1";
	char* pass = "pwiot";
	char* dbname = "iotdb";

	int* sock = (int*)arg;
	int i;
	char* pToken;
	char* pArray[ARR_CNT] = { 0 };

	char name_msg[NAME_SIZE + BUF_SIZE + 1];
	int str_len;

	conn = mysql_init(NULL);

	puts("MYSQL startup");
	if (!(mysql_real_connect(conn, host, user, pass, dbname, 0, NULL, 0)))
	{
		fprintf(stderr, "ERROR : %s[%d]\n", mysql_error(conn), mysql_errno(conn));
		exit(1);
	}
	else
		printf("Connection Successful!\n\n");

	while (1) {
		memset(name_msg, 0x0, sizeof(name_msg));
		str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE);
		if (str_len <= 0)
		{
			*sock = -1;
			return NULL;
		}
		fputs(name_msg, stdout);

		name_msg[strcspn(name_msg,"\n")] = '\0';

		pToken = strtok(name_msg, "[:@]");
		i = 0;
		while (pToken != NULL)
		{
			pArray[i] = pToken;
			if ( ++i >= ARR_CNT)
				break;
			pToken = strtok(NULL, "[:@]");

		}
		if (!strcmp(pArray[1], "DOOR"))
		{
			// 문열기 요청
			if ((!strcmp(pArray[2],"OPEN")))
			{
				if (pArray[3] != NULL)
				{
					sprintf(sql_cmd, "select exists (select 1 from student where uid='%s')",pArray[3]);	
					// UID 유무 확인
					if (mysql_query(conn, sql_cmd))
					{
						fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
						break;
					}
					MYSQL_RES *result = mysql_store_result(conn);
					if (result == NULL) 
					{
						fprintf(stderr, "store_result error : %s\n", mysql_error(conn));
						break;
					}
					MYSQL_ROW row = mysql_fetch_row(result);
					if (row != NULL) 
					{
						int exists = atoi(row[0]);

						// 등록된 UID
						if (exists == 1) 
						{
							// STM 전달
							sprintf(msg,"[%s]DOOR@OPEN\n", STMID);
							write(*sock, msg, strlen(msg));
							
							insert_attendance(conn, pArray[3]);

							// 아두이노 전달
							usleep(100 * 1000);
							sprintf(msg,"[%s]DOOR@OPEN\n", ARDID);
							write(*sock, msg, strlen(msg));
						}
						// 미등록 UID
						else 
						{
							// STM 전달
							sprintf(msg,"[%s]DOOR@FAIL\n", STMID);
							write(*sock, msg, strlen(msg));

							// 아두이노 전달
							usleep(100 * 1000);
							sprintf(msg,"[%s]DOOR@FAIL\n", ARDID);
							write(*sock, msg, strlen(msg));
						}
					}
					mysql_free_result(result);
				}
			}
		}
		// 사용자 추가 요청
		else if (!strcmp(pArray[1], "ADD"))
		{
			if (pArray[2] != NULL)
			{
				sprintf(sql_cmd, "insert into student(uid, name, allergy) values('%s', NULL, NULL)",pArray[2]);
				res = mysql_query(conn, sql_cmd);
				// 사용자 추가 성공
				if (!res)
				{
					// STM 전달
					sprintf(msg,"[%s]ADD@SUCCESS\n", STMID);
					write(*sock, msg, strlen(msg));

					// 아두이노 전달
					usleep(100 * 1000);
					sprintf(msg,"[%s]ADD@FINISH\n", ARDID);
					write(*sock, msg, strlen(msg));
				}
				// 사용자 추가 실패
				else
				{
					// STM 전달
					usleep(100 * 1000);
					sprintf(msg,"[%s]ADD@FAIL\n", STMID);
					write(*sock, msg, strlen(msg));
				}
			}
		}
		else if (!strcmp(pArray[1], "LEAVE"))
		{
			if (pArray[2] == NULL)
				break;

			// 하원 완료 확인
			if (!strcmp(pArray[2],"CHECK"))
			{
				sprintf(sql_cmd, "SELECT SUM(attendance = 1) AS attend_cnt, SUM(is_leave = 1) AS leave_cnt FROM attendance WHERE attendance_date = CURDATE()");

				if (mysql_query(conn, sql_cmd))
				{
					fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
					break;
				}
				MYSQL_RES *result = mysql_store_result(conn);
				if (result == NULL) 
				{
					fprintf(stderr, "store_result error : %s\n", mysql_error(conn));
					break;
				}
				MYSQL_ROW row = mysql_fetch_row(result);
				if (row != NULL)
				{
					int attend_cnt = row[0] ? atoi(row[0]) : 0;
					int leave_cnt  = row[1] ? atoi(row[1]) : 0;

					// 하원 인원 정상
					if (leave_cnt == attend_cnt && attend_cnt > 0)
					{
						usleep(100 * 1000);
						sprintf(msg, "[%s]LEV@SUCCESS\n", ARDID);
						write(*sock, msg, strlen(msg));
					}
					// 하원 인원 비정상
					else
					{
						usleep(100 * 1000);
						sprintf(msg, "[%s]LEV@FAIL\n", ARDID);
						write(*sock, msg, strlen(msg));
					}
				}

				mysql_free_result(result);
			}
			// 하원 등록
			else
			{
    			sprintf(sql_cmd, "UPDATE attendance SET is_leave = 1 WHERE uid = '%s' AND attendance_date = CURDATE() AND is_leave = 0", pArray[2]);

				if (mysql_query(conn, sql_cmd))
				{
					fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
					break;
				}

				usleep(100 * 1000);
				sprintf(msg, "[%s]LEV@FINISH\n", ARDID);
				write(*sock, msg, strlen(msg));

				sprintf(sql_cmd, "SELECT name FROM student WHERE uid = '%s'", pArray[2]);

				if (mysql_query(conn, sql_cmd))
				{
					fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
					break;
				}

				MYSQL_RES *result = mysql_store_result(conn);
				if (result == NULL)
				{
					fprintf(stderr, "store_result error : %s\n", mysql_error(conn));
					break;
				}

				MYSQL_ROW row = mysql_fetch_row(result);

				if (row && row[0] != NULL)
					sprintf(msg, "[%s]LEV@%s\n", STMID, row[0]);
				else
					sprintf(msg, "[%s]LEV@???\n", STMID);
				write(*sock, msg, strlen(msg));

				mysql_free_result(result);
			}
		}
		// 출결 확인
		else if (!strcmp(pArray[1], "CHECK"))
		{
			int result = check_attendance(conn, pArray[2]);

			if (result == 1)
			{
				result = check_leave(conn, pArray[2]);

				if (result == 1)
				{
					sprintf(msg, "[%s]ATTENDANCE DONE, LEAVE DONE\n", pArray[0]);
				}
				else
				{
					sprintf(msg, "[%s]ATTENDANCE DONE, LEAVE YET\n", pArray[0]);
				}
			}
			else
			{
				sprintf(msg, "[%s]ATEENDANCE YET\n", pArray[0]);
			}
			write(*sock, msg, strlen(msg));
		}
	}
	mysql_close(conn);

}

void error_handling(char* msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

void insert_attendance(MYSQL* conn, char* uid)
{
	char sql_cmd[200] = { 0 };

	sprintf(sql_cmd, "select exists (select 1 from attendance WHERE uid='%s' and attendance_date = CURDATE())", uid);
	// 출석 유무 확인
	if (mysql_query(conn, sql_cmd))
	{
		fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
		return;
	}
	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL) 
	{
		fprintf(stderr, "store_result error : %s\n", mysql_error(conn));
		return;
	}
	MYSQL_ROW row = mysql_fetch_row(result);
	if (row != NULL) 
	{
		int exists = atoi(row[0]);

		// 출석 정보 미등록 상태
		if (exists == 0) 
		{
			sprintf(sql_cmd, "insert into attendance (uid, name, attendance_date, attendance, is_leave) select s.uid, s.name, CURDATE(), 1, 0 from student s where s.uid = '%s'", uid);	
			mysql_query(conn, sql_cmd);
		}
	}
	mysql_free_result(result);
}

// 1 : 출석완료
int check_attendance(MYSQL* conn, char* name)
{
	char sql_cmd[200] = { 0 };

	sprintf(sql_cmd, "select exists (select 1 from attendance WHERE name='%s' and attendance_date = CURDATE())", name);

	// 출석 유무 확인
	if (mysql_query(conn, sql_cmd))
	{
		fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
		return 0;
	}

	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL) 
	{
		fprintf(stderr, "store_result error : %s\n", mysql_error(conn));
		return 0;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (row != NULL) 
	{
		int exists = atoi(row[0]);

		mysql_free_result(result);
		return exists;
	}

	mysql_free_result(result);
	return 0;
}

int check_leave(MYSQL* conn, char* name)
{
	char sql_cmd[200] = { 0 };

	sprintf(sql_cmd, "select exists (select 1 from attendance WHERE name='%s' and is_leave = 1 and attendance_date = CURDATE())", name);

	// 출석 유무 확인
	if (mysql_query(conn, sql_cmd))
	{
		fprintf(stderr, "mysql_query error : %s\n", mysql_error(conn));
		return 0;
	}

	MYSQL_RES *result = mysql_store_result(conn);
	if (result == NULL) 
	{
		fprintf(stderr, "store_result error : %s\n", mysql_error(conn));
		return 0;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	if (row != NULL) 
	{
		int exists = atoi(row[0]);

		mysql_free_result(result);
		return exists;
	}

	mysql_free_result(result);
	return 0;
}