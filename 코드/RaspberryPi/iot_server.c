/* author : KSH */
#include <arpa/inet.h>
#include <curl/curl.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BUF_SIZE 100
#define MAX_CLNT 32
#define ID_SIZE 10
#define ARR_CNT 5

#define INFLUXDB_URL "http://localhost:8086"
#define INFLUXDB_DB "mydb"
#define INFLUX_WRITE_URL INFLUXDB_URL "/write?db=" INFLUXDB_DB

#define DEBUG
typedef struct {
    char fd;
    char *from;
    char *to;
    char *msg;
    int len;
} MSG_INFO;

typedef struct {
    int index;
    int fd;
    char ip[20];
    char id[ID_SIZE];
    char pw[ID_SIZE];
} CLIENT_INFO;

typedef enum { MANUAL, AUTO } STATE;
typedef enum { RUN, STOP } ISRUNNING;

typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        fprintf(stderr, "Mem alloc failed\n");
        return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void checkPolicy(void) {
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        char query_url[512];
        // SHOW RETENTION POLICIES ON mydb
        snprintf(query_url, sizeof(query_url),
                 "%s/query?db=%s&q=SHOW%%20RETENTION%%20POLICIES%%20ON%%20%s",
                 INFLUXDB_URL, INFLUXDB_DB, INFLUXDB_DB);
        curl_easy_setopt(curl, CURLOPT_URL, query_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "InfluxDB policy query failed: %s\n",
                    curl_easy_strerror(res));
        } else {
            printf("InfluxDB policy query result:\n%s\n", chunk.memory);
            if (strstr(chunk.memory, "7d") != NULL) {
                printf("Retention policy already met the condition\n");
            } else {
                printf("Change Retention policy\n");
                char alter_query[512];
                snprintf(
                    alter_query, sizeof(alter_query),
                    "ALTER RETENTION POLICY \"default\" ON \"%s\" DURATION 7d",
                    INFLUXDB_DB);
                char *encoded_query = curl_easy_escape(curl, alter_query, 0);
                if (!encoded_query) {
                    fprintf(stderr, "query URL encoding failed\n");
                } else {
                    char alter_url[512];
                    snprintf(alter_url, sizeof(alter_url),
                             "%s/query?db=%s&q=%s", INFLUXDB_URL, INFLUXDB_DB,
                             encoded_query);
                    curl_free(encoded_query);
                    free(chunk.memory);
                    chunk.memory = malloc(1);
                    chunk.size = 0;
                    curl_easy_setopt(curl, CURLOPT_URL, alter_url);
                    res = curl_easy_perform(curl);
                    if (res != CURLE_OK)
                        fprintf(stderr, "Retention policy request failed: %s\n",
                                curl_easy_strerror(res));
                    else
                        printf("Retention policy set to 7D\n");
                }
            }
        }
        curl_easy_cleanup(curl);
    }
    free(chunk.memory);
    curl_global_cleanup();
}

void *clnt_connection(void *arg);
void send_msg(MSG_INFO *msg_info, CLIENT_INFO *first_client_info);
void error_handling(char *msg);
void log_file(char *msgstr);
void getlocaltime(char *buf);

int clnt_cnt = 0;
pthread_mutex_t mutx;

int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    int clnt_adr_sz;
    int sock_option = 1;
    pthread_t t_id[MAX_CLNT] = {0};
    int str_len = 0;
    int i;
    char idpasswd[(ID_SIZE * 2) + 3];
    char *pToken;
    char *pArray[ARR_CNT] = {0};
    char msg[BUF_SIZE];

    CLIENT_INFO client_info[MAX_CLNT] = {
        {0, -1, "", "1", "PASSWD"},       {0, -1, "", "2", "PASSWD"},
        {0, -1, "", "3", "PASSWD"},       {0, -1, "", "4", "PASSWD"},
        {0, -1, "", "5", "PASSWD"},       {0, -1, "", "6", "PASSWD"},
        {0, -1, "", "7", "PASSWD"},       {0, -1, "", "8", "PASSWD"},
        {0, -1, "", "9", "PASSWD"},       {0, -1, "", "10", "PASSWD"},
        {0, -1, "", "11", "PASSWD"},      {0, -1, "", "12", "PASSWD"},
        {0, -1, "", "13", "PASSWD"},      {0, -1, "", "14", "PASSWD"},
        {0, -1, "", "15", "PASSWD"},      {0, -1, "", "16", "PASSWD"},
        {0, -1, "", "17", "PASSWD"},      {0, -1, "", "18", "PASSWD"},
        {0, -1, "", "19", "PASSWD"},      {0, -1, "", "20", "PASSWD"},
        {0, -1, "", "21", "PASSWD"},      {0, -1, "", "22", "PASSWD"},
        {0, -1, "", "23", "PASSWD"},      {0, -1, "", "24", "PASSWD"},
        {0, -1, "", "25", "PASSWD"},      {0, -1, "", "HCY_SQL", "PASSWD"},
        {0, -1, "", "HCY_BT", "PASSWD"},  {0, -1, "", "HCY_STM32", "PASSWD"},
        {0, -1, "", "HCY_LIN", "PASSWD"}, {0, -1, "", "HCY_AND", "PASSWD"},
        {0, -1, "", "HCY_ARD", "PASSWD"}, {0, -1, "", "HM_CON", "PASSWD"}};

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    checkPolicy();
    fputs("IoT Server Start!!\n", stdout);

    if (pthread_mutex_init(&mutx, NULL)) error_handling("mutex init error");

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void *)&sock_option,
               sizeof(sock_option));
    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1) error_handling("listen() error");

    while (1) {
        clnt_adr_sz = sizeof(clnt_adr);
        clnt_sock =
            accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
        if (clnt_cnt >= MAX_CLNT) {
            printf("socket full\n");
            shutdown(clnt_sock, SHUT_WR);
            continue;
        } else if (clnt_sock < 0) {
            perror("accept()");
            continue;
        }

        str_len = read(clnt_sock, idpasswd, sizeof(idpasswd));
        idpasswd[str_len] = '\0';

        if (str_len > 0) {
            i = 0;
            pToken = strtok(idpasswd, "[:]");

            while (pToken != NULL) {
                pArray[i] = pToken;
                if (i++ >= ARR_CNT) break;
                pToken = strtok(NULL, "[:]");
            }
            for (i = 0; i < MAX_CLNT; i++) {
                if (!strcmp(client_info[i].id, pArray[0])) {
                    if (client_info[i].fd != -1) {
                        sprintf(msg, "[%s] Already logged!\n", pArray[0]);
                        write(clnt_sock, msg, strlen(msg));
                        log_file(msg);
                        shutdown(clnt_sock, SHUT_WR);
#if 1  // for MCU
                        client_info[i].fd = -1;
#endif
                        break;
                    }
                    if (!strcmp(client_info[i].pw, pArray[1])) {
                        strcpy(client_info[i].ip, inet_ntoa(clnt_adr.sin_addr));
                        pthread_mutex_lock(&mutx);
                        client_info[i].index = i;
                        client_info[i].fd = clnt_sock;
                        clnt_cnt++;
                        pthread_mutex_unlock(&mutx);
                        sprintf(
                            msg,
                            "[%s] New connected! (ip:%s,fd:%d,sockcnt:%d)\n",
                            pArray[0], inet_ntoa(clnt_adr.sin_addr), clnt_sock,
                            clnt_cnt);
                        log_file(msg);
                        write(clnt_sock, msg, strlen(msg));

                        pthread_create(t_id + i, NULL, clnt_connection,
                                       (void *)(client_info + i));
                        pthread_detach(t_id[i]);
                        break;
                    }
                }
            }
            if (i == MAX_CLNT) {
                sprintf(msg, "[%s] Authentication Error!\n", pArray[0]);
                write(clnt_sock, msg, strlen(msg));
                log_file(msg);
                shutdown(clnt_sock, SHUT_WR);
            }
        } else
            shutdown(clnt_sock, SHUT_WR);
    }
    return 0;
}

void *clnt_connection(void *arg) {
    CLIENT_INFO *client_info = (CLIENT_INFO *)arg;
    int str_len = 0;
    int index = client_info->index;
    char msg[BUF_SIZE];
    char to_msg[MAX_CLNT * ID_SIZE + 1];
    int i = 0;
    char *pToken;
    char *pArray[ARR_CNT] = {0};
    char strBuff[130] = {0};

    const float tempLow = 18.0, tempHigh = 25.0;
    const int humiLow = 40, humiHigh = 60;

    MSG_INFO msg_info;
    CLIENT_INFO *first_client_info;
    static STATE curState = AUTO;
    static ISRUNNING humi = STOP;
    static ISRUNNING temp = STOP;

    first_client_info = (CLIENT_INFO *)((void *)client_info -
                                        (void *)(sizeof(CLIENT_INFO) * index));
    while (1) {
        memset(msg, 0x0, sizeof(msg));
        str_len = read(client_info->fd, msg, sizeof(msg) - 1);
        if (str_len <= 0) break;
        msg[str_len] = '\0';

        pToken = strtok(msg, "[:]");
        i = 0;
        while (pToken != NULL) {
            pArray[i] = pToken;
            if (i++ >= ARR_CNT) break;
            pToken = strtok(NULL, "[:]");
        }
        msg_info.fd = client_info->fd;
        msg_info.from = client_info->id;
        // 온도 25>이상 모터(relay2 ON) 습도>40이상(제습기 relay1 on)
        // curl to influxdb 1.x
        // no auth

        if (strcmp(pArray[0], "room1") == 0) {
            char data[256];
            snprintf(data, sizeof(data),
                     "environment,room=%s humidity=%s,temperature=%s",
                     pArray[0], pArray[1], pArray[2]);

            CURL *curl = curl_easy_init();
            if (curl) {
                curl_easy_setopt(curl, CURLOPT_URL, INFLUX_WRITE_URL);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
                CURLcode res = curl_easy_perform(curl);
                if (res != CURLE_OK)
                    fprintf(stderr, "InfluxDB fail: %s\n",
                            curl_easy_strerror(res));
                curl_easy_cleanup(curl);
            }
            printf("[room1] Humidity : %s%% Temperature : %s'C\n", pArray[1],
                   pArray[2]);
            pArray[0] = "HCY_ARD";
            if (curState == MANUAL) continue;

            if (humi == STOP && atoi(pArray[1]) < humiLow) {
                humi = RUN;
                sprintf(to_msg, "[%s]RELAY1@ON", msg_info.from);
                msg_info.to = pArray[0];
                msg_info.msg = to_msg;
                msg_info.len = strlen(to_msg);
                send_msg(&msg_info, first_client_info);
            } else if (humi == RUN && atoi(pArray[1]) >= humiHigh) {
                humi = STOP;
                sprintf(to_msg, "[%s]RELAY1@OFF", msg_info.from);
                msg_info.to = pArray[0];
                msg_info.msg = to_msg;
                msg_info.len = strlen(to_msg);
                send_msg(&msg_info, first_client_info);
            }

            if (temp == STOP && atof(pArray[2]) < tempLow) {
                temp = RUN;
                sprintf(to_msg, "[%s]RELAY2@ON", msg_info.from);
                msg_info.to = pArray[0];
                msg_info.msg = to_msg;
                msg_info.len = strlen(to_msg);
                send_msg(&msg_info, first_client_info);
            } else if (temp == RUN && atof(pArray[2]) >= tempHigh) {
                temp = STOP;
                sprintf(to_msg, "[%s]RELAY2@OFF", msg_info.from);
                msg_info.to = pArray[0];
                msg_info.msg = to_msg;
                msg_info.len = strlen(to_msg);
                send_msg(&msg_info, first_client_info);
            }
        } else {
            sprintf(to_msg, "[%s]%s", msg_info.from, pArray[1]);
            sprintf(strBuff, "msg : [%s->%s] %s", msg_info.from, msg_info.to,
                    pArray[1]);
            log_file(strBuff);
            if (strcmp(pArray[1], "MANUAL@OFF"))
                curState = AUTO;
            else if (strcmp(pArray[1], "MANUAL@ON"))
                curState = MANUAL;
            msg_info.to = pArray[0];
            msg_info.msg = to_msg;
            msg_info.len = strlen(to_msg);
            send_msg(&msg_info, first_client_info);
        }
    }

    close(client_info->fd);

    sprintf(strBuff, "Disconnect ID:%s (ip:%s,fd:%d,sockcnt:%d)\n",
            client_info->id, client_info->ip, client_info->fd, clnt_cnt - 1);
    log_file(strBuff);

    pthread_mutex_lock(&mutx);
    clnt_cnt--;
    client_info->fd = -1;
    pthread_mutex_unlock(&mutx);

    return 0;
}

void send_msg(MSG_INFO *msg_info, CLIENT_INFO *first_client_info) {
    int i = 0;

    if (!strcmp(msg_info->to, "ALLMSG")) {
        for (i = 0; i < MAX_CLNT; i++)
            if ((first_client_info + i)->fd != -1)
                write((first_client_info + i)->fd, msg_info->msg,
                      msg_info->len);
    } else if (!strcmp(msg_info->to, "IDLIST")) {
        char *idlist = (char *)malloc(ID_SIZE * MAX_CLNT);
        msg_info->msg[strlen(msg_info->msg) - 1] = '\0';
        strcpy(idlist, msg_info->msg);

        for (i = 0; i < MAX_CLNT; i++) {
            if ((first_client_info + i)->fd != -1) {
                strcat(idlist, (first_client_info + i)->id);
                strcat(idlist, " ");
            }
        }
        strcat(idlist, "\n");
        write(msg_info->fd, idlist, strlen(idlist));
        free(idlist);
    } else if (!strcmp(msg_info->to, "GETTIME")) {
        sleep(1);
        getlocaltime(msg_info->msg);
        write(msg_info->fd, msg_info->msg, strlen(msg_info->msg));
    } else
        for (i = 0; i < MAX_CLNT; i++)
            if ((first_client_info + i)->fd != -1)
                if (!strcmp(msg_info->to, (first_client_info + i)->id)) {
                    write((first_client_info + i)->fd, msg_info->msg,
                          msg_info->len);
                }
}

void error_handling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void log_file(char *msgstr) {
    fputs(msgstr, stdout);
}
void getlocaltime(char *buf) {
    struct tm *t;
    time_t tt;
    char wday[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    tt = time(NULL);
    if (errno == EFAULT) perror("time()");
    t = localtime(&tt);
    sprintf(buf, "[GETTIME]%02d.%02d.%02d %02d:%02d:%02d %s\n",
            t->tm_year + 1900 - 2000, t->tm_mon + 1, t->tm_mday, t->tm_hour,
            t->tm_min, t->tm_sec, wday[t->tm_wday]);
    return;
}
