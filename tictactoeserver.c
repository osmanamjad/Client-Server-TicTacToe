//a tictactoe server
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>

struct client {
    int fd;
    struct in_addr ipaddr;
    struct client *next;
} *top = NULL;

int howmany = 0;
int xfd = -1;
int ofd = -1;
int listenfd;
int port = 3000;
char board[9];
char turn = 'x';

void activity(struct client *p)  /* select() said activity; check it out */
{
    extern void removeclient(int fd);
    extern void broadcast(char *s, int size);
    extern void broadcastclient(int fd, char *s, int size);
    extern void swapandtellturns();
    extern void broadcastboardtoeveryone();
    extern void chatmessage(int fd, char *message);
    extern char *extractline(char *p, int size);
    extern int game_is_over();
    extern void reassignplayers();
    extern void initializeboard();
    char input[100];
    int len = read(p->fd, input, sizeof input);
    if (isdigit(input[0]) && len == 2) {
        if (input[0] == '0') {
            chatmessage(p->fd, input);
        }
        else {
            char buf[100];
            //if non player sends a digit
            if (p->fd != xfd && p->fd != ofd) {
                printf("%s tries to make move %c, but they aren't playing\n", inet_ntoa(p->ipaddr), input[0]);
                fflush(stdout);
                sprintf(buf, "You can't make moves; you can only watch the game\r\n");
                broadcastclient(p->fd, buf, strlen(buf));
            }
            //if player plays when its not their turn
            else if ( (p->fd == xfd && turn != 'x') || (p->fd == ofd && turn != 'o') ) {
                printf("%s tries to make move %c, but it's not their turn\n", inet_ntoa(p->ipaddr), input[0]);
                fflush(stdout);
                sprintf(buf, "It is not your turn\r\n");
                broadcastclient(p->fd, buf, strlen(buf));
            }
            //if player plays their turn
            else if ( (p->fd == xfd && turn == 'x') || (p->fd == ofd && turn == 'o') ) {
                int index = input[0] - '0';
                //if the space is already occupied
                if (board[index - 1] == 'x' || board[index - 1] == 'o') {
                    printf("%s tries to make move %c, but that space is taken\n", inet_ntoa(p->ipaddr), input[0]);
                    fflush(stdout);
                    sprintf(buf, "That space is taken\r\n");
                    broadcastclient(p->fd, buf, strlen(buf));
                }
                else { //if the space is open
                    board[index - 1] = turn;
                    printf("%s (%c) makes move %c\n", inet_ntoa(p->ipaddr), turn, input[0]);
                    sprintf(buf, "%c makes move %c\r\n", turn, input[0]);
                    broadcast(buf, strlen(buf));
                    broadcastboardtoeveryone();
                    swapandtellturns();
                    char c;
                    if ( (c = game_is_over()) ) {
                        if (c == 'x' || c == 'o') {
                            printf("Game over!\n");
                            printf("%c wins.\n", c);
                            fflush(stdout);
                            char over[100];
                            sprintf(over, "Game over!\r\n");
                            broadcast(over, strlen(over));
                            broadcastboardtoeveryone();
                            memset(over, 0, sizeof over);
                            sprintf(over, "%c wins.\r\nLet's play again!\r\n", c);
                            broadcast(over, strlen(over));
                        }
                        else {
                            printf("Game over!\n");
                            printf("It is a draw.\n");
                            fflush(stdout);
                            char over[100];
                            sprintf(over, "Game over!\r\n");
                            broadcast(over, strlen(over));
                            broadcastboardtoeveryone();
                            memset(over, 0, sizeof over);
                            sprintf(over, "It is a draw.\r\nLet's play again!\r\n");
                            broadcast(over, strlen(over));
                        }
                        reassignplayers();
                        initializeboard();
                    }
                }
            }
        }
    }
    else if (len > 0) {
        chatmessage(p->fd, input);
    }
    else if (len == 0) {
        printf("disconnecting client %s\n", inet_ntoa(p->ipaddr));
        fflush(stdout);
        removeclient(p->fd);
    }
    else {
        /* shouldn't happen */
        perror("read");
    }
}

void reassignplayers()
{
    extern void broadcastclient(int fd, char *s, int size);
    char buf[100];
    int temp = xfd;
    sprintf(buf, "You are o\r\n");
    broadcastclient(xfd, buf, strlen(buf));
    xfd = ofd;
    memset(buf, 0, sizeof buf);
    sprintf(buf, "You are x\r\n");
    broadcastclient(ofd, buf, strlen(buf));
    ofd = temp;
    turn = 'x';
}

void chatmessage(int fd, char *message)
{
    extern char *extractline(char *p, int size);
    extern void broadcastclient(int fd, char *s, int size);
    extractline(message, strlen(message));
    printf("chat message: %s\n", message);
    fflush(stdout);
    struct client *p;
    char buf[100];
    for (p = top; p; p = p->next) {
        if (p->fd != fd) {
            sprintf(buf, "%s\r\n", message);
            broadcastclient(p->fd, buf, strlen(buf));
        }
    }
}

char *extractline(char *p, int size) /* returns pointer to string after, or NULL if there isn't an entire line here.  If non-NULL, original p is now a valid string. */
{
    int nl;
    for (nl = 0; nl < size && p[nl] != '\r' && p[nl] != '\n'; nl++)
        ;
    if (nl == size)
        return(NULL);

    /*
     * There are three cases: either this is a lone \r, a lone \n, or a CRLF.
     */
    if (p[nl] == '\r' && nl + 1 < size && p[nl+1] == '\n') {
        /* CRLF */
        p[nl] = '\0';
        return(p + nl + 2);
    } else {
        /* lone \n or \r */
        p[nl] = '\0';
        return(p + nl + 1);
    }
}

void swapandtellturns()
{
    extern void broadcastclient(int fd, char *s, int size);
    extern void tellturntoeveryoneelse(int fd);
    char buf[100];
    if (turn == 'x') {
        turn = 'o';
        sprintf(buf, "It is your turn\r\n");
        broadcastclient(ofd, buf, strlen(buf));
        tellturntoeveryoneelse(ofd);
    }
    else {
        turn = 'x';
        sprintf(buf, "It is your turn\r\n");
        broadcastclient(xfd, buf, strlen(buf));
        tellturntoeveryoneelse(xfd);
    }
}

void newconnection()  /* accept connection, sing to them, get response, update linked list */
{
    extern void broadcast(char *s, int size);
    extern void removeclient(int fd);
    extern void addclient(int fd, struct in_addr addr);
    extern void broadcastboard(int fd);
    extern void currentturn(int fd);
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof r;

    if ((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0) {
        perror("accept");
    }
    else {
        printf("new connection from %s\n", inet_ntoa(r.sin_addr));
        fflush(stdout);
        broadcastboard(fd);
        currentturn(fd);
        addclient(fd, r.sin_addr);
    }
}

void currentturn(int fd)
{
    extern void broadcastclient(int fd, char *s, int size);
    char buf[100];
    sprintf(buf, "It is %c's turn\r\n", turn);
    broadcastclient(fd, buf, strlen(buf));
}

void tellturntoeveryoneelse(int fd)
{
    extern void currentturn(int fd);
    struct client *p;
    for (p = top; p; p = p->next) {
        if (p->fd != fd) {
            currentturn(p->fd);
        }
    }
}

void addclient(int fd, struct in_addr addr)
{
    extern void broadcastclient(int fd, char *s, int size);
    char buf[100];
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        fprintf(stderr, "out of memory!\n");  /* highly unlikely to happen */
        exit(1);
    }
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    top = p;
    if (xfd == -1) {
        xfd = p->fd;
        printf("client from %s is now x\n", inet_ntoa(addr));
        fflush(stdout);
        sprintf(buf, "You now get to play! You are now x.\r\n");
        broadcastclient(xfd, buf, strlen(buf));
        // if there is already an o player, tell them we're here
        if (ofd != -1) {
            memset(buf, 0, sizeof buf);
            sprintf(buf,"%s is now playing 'x'.\r\n", inet_ntoa(addr));
            broadcastclient(ofd, buf, strlen(buf));
        }
    }
    else if (ofd == -1) {
        ofd = p->fd;
        printf("client from %s is now o\n", inet_ntoa(addr));
        fflush(stdout);
        sprintf(buf, "You now get to play! You are now o.\r\n");
        broadcastclient(ofd, buf, strlen(buf));
        // xfd != -1  is guaranteed from the if condition
        memset(buf, 0, sizeof buf);
        sprintf(buf,"%s is now playing 'o'.\r\n", inet_ntoa(addr));
        broadcastclient(xfd, buf, strlen(buf));
    }
    howmany++;
}

void removeclient(int fd)
{
    extern void assignnewx();
    extern void assignnewo();
    struct client **p;
    int removefd;
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
    ;
    if (*p) {
        removefd = (*p)->fd;
        struct client *t = (*p)->next;
        free(*p);
        *p = t;
        howmany--;

        if (removefd == xfd) {
            xfd = -1;
            assignnewx();
        }
        else if (removefd == ofd) {
            ofd = -1;
            assignnewo();
        }
    }
    else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
        fflush(stderr);
    }
}

void assignnewx()
{
    extern void broadcastclient(int fd, char *s, int size);
    struct client **p;
    char buf[100];
    for (p = &top; *p && ((*p)->fd == xfd || (*p)->fd == ofd); p = &(*p)->next)
    ;
    if (*p) {
        xfd = (*p)->fd;
        printf("client from %s is now x\n", inet_ntoa((*p)->ipaddr));
        fflush(stdout);
        sprintf(buf, "You now get to play! You are now x.\r\n");
        broadcastclient(xfd, buf, strlen(buf));
        memset(buf, 0, sizeof buf);
        sprintf(buf, "%s is now playing 'x'.\r\n", inet_ntoa((*p)->ipaddr));
        broadcastclient(ofd, buf, strlen(buf));
    }
}

void assignnewo()
{
    extern void broadcastclient(int fd, char *s, int size);
    struct client **p;
    char buf[100];
    for (p = &top; *p && ((*p)->fd == ofd || (*p)->fd == xfd); p = &(*p)->next)
    ;
    if (*p) {
        ofd = (*p)->fd;
        printf("client from %s is now o\n", inet_ntoa((*p)->ipaddr));
        fflush(stdout);
        sprintf(buf, "You now get to play! You are now o.\r\n");
        broadcastclient(ofd, buf, strlen(buf));
        memset(buf, 0, sizeof buf);
        sprintf(buf, "%s is now playing 'o'.\r\n", inet_ntoa((*p)->ipaddr));
        broadcastclient(xfd, buf, strlen(buf));
    }
}

void broadcastclient(int fd, char *s, int size)
{
    write(fd, s, size);
}

void broadcast(char *s, int size)
{
    struct client *p;
    for (p = top; p; p = p->next)
        write(p->fd, s, size);
}

void initializeboard()
{
    for (int i = 0; i < 9; i++) {
        board[i] = '1' + i;
    }
}

void broadcastboard(int fd)
{
    char buf[100], *bufp, *boardp;
    int col, row;
    for (bufp = buf, col = 0, boardp = board; col < 3; col++) {
        for (row = 0; row < 3; row++, bufp += 4)
            sprintf(bufp, " %c |", *boardp++);
        bufp -= 2;  // kill last " |"
        strcpy(bufp, "\r\n---+---+---\r\n");
        bufp = strchr(bufp, '\0');
    }
    if (write(fd, buf, bufp - buf) != bufp-buf)
        perror("write");
}


void broadcastboardtoeveryone()
{
    struct client *p;
    for (p = top; p; p = p->next) {
        broadcastboard(p->fd);
    }
}

int game_is_over()  /* returns winner, or ' ' for draw, or 0 for not over */
{
    int i, c;
    extern int allthree(int start, int offset);
    extern int isfull();

    for (i = 0; i < 3; i++)
        if ((c = allthree(i, 3)) || (c = allthree(i * 3, 1)))
            return(c);
    if ((c = allthree(0, 4)) || (c = allthree(2, 2)))
        return(c);
    if (isfull())
        return(' ');
    return(0);
}

int allthree(int start, int offset)
{
    if (board[start] > '9' && board[start] == board[start + offset]
            && board[start] == board[start + offset * 2])
        return(board[start]);
    return(0);
}

int isfull()
{
    int i;
    for (i = 0; i < 9; i++)
        if (board[i] < 'a')
            return(0);
    return(1);
}

int main(int argc, char **argv)
{
    int c, status = 0;
    struct client *p;
    extern void initializeboard(), bindandlisten(), newconnection(), activity(struct client *p);

    initializeboard();

    while ((c = getopt(argc, argv, "p:")) != EOF) {
        switch (c) {
        case 'p':
            port = atoi(optarg);
            break;
        default:
            status = 1;
        }
    }
    if (status || optind < argc) {
        fprintf(stderr, "usage: %s [-p port]\n", argv[0]);
        return(1);
    }

    bindandlisten();  /* aborts on error */
    while (1) {
        fd_set fds;
        int maxfd = listenfd;
        FD_ZERO(&fds);
        FD_SET(listenfd, &fds);
        for (p = top; p; p = p->next) {
            FD_SET(p->fd, &fds);
            if (p->fd > maxfd)
            maxfd = p->fd;
        }
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
        } else {
            for (p = top; p; p = p->next) {
                if (FD_ISSET(p->fd, &fds))
                    break;
            }
                /*
                 * it's not very likely that more than one client will drop at
                * once, so it's not a big loss that we process only one each
                * select(); we'll get it later...
                */
            if (p)
                activity(p);  /* might remove p from list, so can't be in the loop */
            if (FD_ISSET(listenfd, &fds))
                newconnection();
        }
    }

    return(0);
}

void bindandlisten()  /* bind and listen, abort on error */
{
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
}
