#include <arpa/inet.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);

char name[NAME_SIZE] = "[Default]";

typedef struct {
    int sockfd;
    int btfd1;
    int btfd2;
    char sendid[NAME_SIZE];
} DEV_FD;

int main(int argc, char *argv[]) {
    DEV_FD dev_fd;
    struct sockaddr_in serv_addr;
    pthread_t snd_thread, rcv_thread;
    void *thread_return;
    int ret;
    struct sockaddr_rc addr1 = {0}, addr2 = {0};
    char dest1[18] = "98:DA:60:08:1C:0F";  // arduino
    char dest2[18] = "98:DA:60:07:D6:C4";  // STM32
    char msg[BUF_SIZE];

    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        exit(1);
    }

    sprintf(name, "%s", argv[3]);

    dev_fd.sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (dev_fd.sockfd == -1) error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(dev_fd.sockfd, (struct sockaddr *)&serv_addr,
                sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    sprintf(msg, "[%s:PASSWD]", name);
    write(dev_fd.sockfd, msg, strlen(msg));

    // 첫 번째 블루투스 연결 (아두이노)
    // dev_fd.btfd1 = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    // if (dev_fd.btfd1 == -1) error_handling("Bluetooth socket() error");

    // addr1.rc_family = AF_BLUETOOTH;
    // addr1.rc_channel = (uint8_t)1;
    // str2ba(dest1, &addr1.rc_bdaddr);

    // ret = connect(dev_fd.btfd1, (struct sockaddr *)&addr1, sizeof(addr1));
    // if (ret == -1) error_handling("Arduino Bluetooth connect() error");

    // 두 번째 블루투스 연결 (STM32)
    dev_fd.btfd2 = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (dev_fd.btfd2 == -1) error_handling("Bluetooth socket() error");

    addr2.rc_family = AF_BLUETOOTH;
    addr2.rc_channel = (uint8_t)1;
    str2ba(dest2, &addr2.rc_bdaddr);

    ret = connect(dev_fd.btfd2, (struct sockaddr *)&addr2, sizeof(addr2));
    if (ret == -1) error_handling("STM32 Bluetooth connect() error");

    pthread_create(&rcv_thread, NULL, recv_msg, (void *)&dev_fd);
    pthread_create(&snd_thread, NULL, send_msg, (void *)&dev_fd);

    pthread_join(snd_thread, &thread_return);
    pthread_join(rcv_thread, &thread_return);

    close(dev_fd.btfd1);
    close(dev_fd.btfd2);
    close(dev_fd.sockfd);
    return 0;
}

void *send_msg(void *arg)  // STM32에서 읽어 서버로 전송
{
    DEV_FD *dev_fd = (DEV_FD *)arg;
    int ret;
    char msg[BUF_SIZE];
    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        // STM32 블루투스 소켓만 모니터링
        FD_SET(dev_fd->btfd2, &readfds);

        // select()로 btfd2에 데이터가 있는지 확인
        ret = select(dev_fd->btfd2 + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() error");
            break;
        }

        if (FD_ISSET(dev_fd->btfd2, &readfds)) {
            // STM32에서 데이터 읽기
            ret = read(dev_fd->btfd2, msg, sizeof(msg) - 1);
            if (ret <= 0) {
                // 소켓 종료 or 에러
                perror("read() from STM32");
                break;
            }
            msg[ret] = '\0';

            // 서버로 전송
            // printf("[STM32->SRV] %s", msg);
            if (write(dev_fd->sockfd, msg, ret) <= 0) {
                perror("write() to server");
                break;
            }
        }
    }
    return NULL;
}

void *recv_msg(void *arg)  // 서버에서 읽어 아두이노로 전송
{
    DEV_FD *dev_fd = (DEV_FD *)arg;
    char msg[BUF_SIZE];
    int str_len;

    while (1) {
        // 서버에서 데이터 읽기
        str_len = read(dev_fd->sockfd, msg, BUF_SIZE - 1);
        if (str_len <= 0) {
            // 소켓 종료 or 에러
            perror("read() from server");
            break;
        }
        msg[str_len] = '\0';

        // 아두이노에게 전달
        printf("[SRV->ARD] %s", msg);
        if (write(dev_fd->btfd1, msg, str_len) <= 0) {
            perror("write() to Arduino");
            break;
        }
    }
    return NULL;
}

void error_handling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}
