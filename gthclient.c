/*
 * Gothello daemon client library
 * Bart Massey <bart@cs.pdx.edu> 1999,2000,2001
 * $Id: gthclient.c,v 2.7 2001/12/06 02:22:47 bart Exp $
 *
 * Fixes and winsock support in
 * current version by Joe Parker <jparker@pdx.edu>
 */

/* Define to compile for Windows WinSock */
/* #undef WIN32 */
/* Define to use the inet_aton() routine instead
   of inet_addr() for numeric host addresses */
/* #undef USE_INET_ATON */

#include "gthclient.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#ifdef WIN32

#include <io.h>
#include <winsock2.h>
#include <stdarg.h>

#define fdopen(arg1, arg2) arg1
#define MAXBUFSIZE 1024

static SOCKET sock, fsock_in, fsock_out;

#else

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define WSAperror perror

static int sock;
static FILE *fsock_in, *fsock_out;

#endif


static char *client_version = "0.9.1";
static enum gth_who who = GTH_WHO_NONE;
static int server_base = 29068;

static char buf[1024];
static char *msg_text;
static int msg_code;
static int serial = 0;


#ifdef WIN32

static int fprintf(SOCKET sock, const char *fmt, ...)
{
  va_list args;
  char temp[MAXBUFSIZE];
  int ret;

  va_start(args, fmt);
  ret = vsprintf(temp, fmt, args);
  va_end(args);

  if(send(sock, temp, strlen(temp), 0) == SOCKET_ERROR)
    return -1;

  return ret;
}


static char* fgets(char *buf, int maxlen, SOCKET sock)
{
  int ret, count = 0;
  char *tempbuf = buf;

  while(1)
  {
    ++count;
    if(count > maxlen)
      return buf;

    ret = recv(sock, tempbuf, 1, 0);

    if((ret == SOCKET_ERROR) || (ret <= 0))
      return NULL;

    if(*tempbuf == '\n')
    {
      ++tempbuf;
      *tempbuf = 0;
      return buf;
    }

    ++tempbuf;
  }
}


static int close(SOCKET sock)
{
  return closesocket(sock);
}


static int fclose(SOCKET sock)
{
  return closesocket(sock);
}


static void fflush(SOCKET sock)
{
}



static void WSAperror(char *msg, int errorcd)
{
  printf("%s: ", msg);
  
  switch(errorcd)
  {
    case WSASYSNOTREADY:
      printf("Network subsystem is unavailable.\n");
      break;
    case WSAVERNOTSUPPORTED:
      printf("WINSOCK.DLL version out of range.\n");
      break;
    case WSAEINPROGRESS:
      printf("Operation now in progress.\n");
      break;
    case WSAEPROCLIM:
      printf("Too many processes.\n");
      break;
    case WSAEFAULT:
      printf("Bad address.\n");
      break;
    case WSANOTINITIALISED:
      printf("Successful WSAStartup not yet performed.\n");
      break;
    case WSAENETDOWN:
      printf("Network is down.\n");
      break;
    case WSAEADDRINUSE:
      printf("Address already in use.\n");
      break;
    case WSAEADDRNOTAVAIL:
      printf("Cannot assign requested address.\n");
      break;
    case WSAECONNREFUSED:
      printf("Connection refused.\n");
      break;
    case WSAEISCONN:
      printf("Socket is already connected.\n");
      break;
    case WSAENETUNREACH:
      printf("Network is unreachable.\n");
      break;
    case WSAENOBUFS:
      printf("No buffer space available.\n");
      break;
    case WSAETIMEDOUT:
      printf("Connection timed out.\n");
      break;
    case WSAECONNRESET:
      printf("Connection reset by peer.\n");
      break;
    default:
      printf("Unknown error.\n");
      break;
  }
}



static void WSAperror(char *msg)
{
  WSAperror(msg, WSAGetLastError());
}
#endif


static void get_move(char *pos) {
    int msg_serial;

    switch(msg_code) {
    case 311:
    case 321:
    case 322:
    case 325:
	sscanf(msg_text, "%d %2s", &msg_serial, pos);
	break;
    case 313:
	sscanf(msg_text, "%d %2s %d", &msg_serial, pos, &gth_opp_time);
	break;
    case 315:
	sscanf(msg_text, "%d pass", &msg_serial);
	strcpy(pos, ".p");
	break;
    case 317:
	sscanf(msg_text, "%d pass %d", &msg_serial, &gth_opp_time);
	strcpy(pos, ".p");
	break;
    case 312:
    case 323:
    case 324:
    case 326:
	sscanf(msg_text, "%d ... %2s", &msg_serial, pos);
	break;
    case 314:
	sscanf(msg_text, "%d ... %2s %d", &msg_serial, pos, &gth_opp_time);
 	break;
    case 316:
	sscanf(msg_text, "%d ... pass", &msg_serial);
	strcpy(pos, ".p");
	break;
    case 318:
	sscanf(msg_text, "%d ... pass %d", &msg_serial, &gth_opp_time);
	strcpy(pos, ".p");
	break;
    default:
	fprintf(stderr,
		"get_move: unexpected status message code %03d\n",
		msg_code);
	abort();
    }
    if (serial != msg_serial) {
	fprintf(stderr,
		"got move serial %d, expected %d\n",
		msg_serial,
		serial);
	abort();
    }
}

static int get_msg(void) {
  char *n = fgets(buf, sizeof(buf), fsock_in);
  int len;
  if (!n) {
    WSAperror("fgets");
    return -1;
  }
  len = strlen(n);
  if (len >= sizeof(buf) - 1 || len < 4) {
    fprintf(stderr, "I/O error in get_msg: read %d bytes\n", len);
    return -1;
  }
  if (!isdigit(buf[0]) ||
      !isdigit(buf[1]) ||
      !isdigit(buf[2]) ||
      buf[3] != ' ') {
    fprintf(stderr, "I/O error in get_msg: bad response code\n");
    return -1;
  }
  buf[3] = '\0';
  msg_code = atoi(buf);
  msg_text = &buf[4];
  return 0;
}

static void closeall(void) {
  (void) fclose(fsock_out);
  (void) fclose(fsock_in);
  (void) close(sock);

#ifdef WIN32
  WSACleanup();
#endif
}

static enum gth_who opponent(enum gth_who w) {
  assert(w == GTH_WHO_WHITE || w == GTH_WHO_BLACK);
  if (w == GTH_WHO_WHITE)
    return GTH_WHO_BLACK;
  return GTH_WHO_WHITE;
}

int gth_time_controls = 0;
int gth_white_time_control = 0;
int gth_black_time_control = 0;
int gth_my_time = 0;
int gth_opp_time = 0;

static void get_time_controls(void) {
  int i;
  for (i = 0; i < strlen(msg_text); i++)
    if (isdigit(msg_text[i])) {
      gth_white_time_control = atoi(&msg_text[i]);
      while (isdigit(msg_text[i]))
	i++;
      break;
    }
  for (; i < strlen(msg_text); i++)
    if (isdigit(msg_text[i])) {
      gth_black_time_control = atoi(&msg_text[i]);
      while (isdigit(msg_text[i]))
	i++;
      break;
    }
}

static int get_time(void) {
  int i, t;
  for (i = 0; i < strlen(msg_text); i++)
    if (isdigit(msg_text[i])) {
      t = atoi(&msg_text[i]);
      while (isdigit(msg_text[i]))
	i++;
      return t;
    }
  return -1;
}

enum gth_who gth_winner = GTH_WHO_NONE;

int gth_start_game(enum gth_who side, char *host, int server) {
  struct sockaddr_in serv_addr;
  struct hostent *hostent;
  int result;

#ifdef WIN32
  WORD wVersionRequested;
  WSADATA wsaData;
  int ret;

  wVersionRequested = MAKEWORD(2, 0);
  ret = WSAStartup(wVersionRequested, &wsaData);
  
  if(ret)
  {
    WSAperror("Winsock initialization", ret);
    return -1;
  }
#endif
  
  hostent = gethostbyname(host);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(server_base + server);
  if (hostent) {
      serv_addr.sin_addr =
	*(struct in_addr *)(hostent->h_addr);
  } else {
#ifdef USE_INET_ATON
      result = inet_aton(host, &serv_addr.sin_addr);
      if (!result) {
	  fprintf(stderr, "cannot find host %s\n", host);
	  return -1;
      }
#else
      result = inet_addr(host);
      if (result == -1) {
	  fprintf(stderr, "cannot find host %s\n", host);
	  return -1;
      }
      serv_addr.sin_addr.s_addr = result;
#endif
  }
  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == -1) {
    WSAperror("sock");
    return -1;
  }
  result = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (result == -1) {
    WSAperror("connect");
    return -1;
  }
  fsock_in = fdopen(sock, "r");
  if (!fsock_in) {
    perror("fdopen fsock_in");
    closeall();
    return -1;
  }
  fsock_out = fdopen(sock, "w");
  if (!fsock_out) {
    perror("fdopen fsock_out");
    closeall();
    return -1;
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return -1;
  }
  if (msg_code != 0) {
    fprintf(stderr, "illegal greeting %03d: %s\n", msg_code, msg_text);
    closeall();
    return -1;
  }
  result = fprintf(fsock_out,
		   "%s player %s\r",
		   client_version,
		   side == GTH_WHO_WHITE ? "white" : "black");
  (void)fflush(fsock_out);
  if (result == -1) {
    WSAperror("start_game: fprintf");
    closeall();
    return GTH_STATE_ERROR;
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return -1;
  }
  if (msg_code != 100 && msg_code != 101) {
    fprintf(stderr, "side failure %03d: %s\n", msg_code, msg_text);
    closeall();
    return -1;
  }
  if (msg_code == 101) {
    gth_time_controls = 1;
    get_time_controls();
    if (side == GTH_WHO_WHITE) {
      gth_my_time = gth_white_time_control;
      gth_opp_time = gth_black_time_control;
    } else {
      gth_my_time = gth_black_time_control;
      gth_opp_time = gth_white_time_control;
    }
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return -1;
  }
  if ((msg_code != 351 && side == GTH_WHO_WHITE) ||
      (msg_code != 352 && side == GTH_WHO_BLACK)) {
    fprintf(stderr, "side failure %03d: %s\n", msg_code, msg_text);
    closeall();
    return -1;
  }
  who = side;
  return 0;
}

enum gth_state gth_make_move(char *pos) {
  char *ellipses = "";
  int result;
  char movebuf[6];
  
  if (who == GTH_WHO_NONE) {
    fprintf(stderr, "make_move: not initialized\n");
    return GTH_STATE_ERROR;
  }
  if (gth_winner != GTH_WHO_NONE) {
    fprintf(stderr, "make_move: game over\n");
    return GTH_STATE_ERROR;
  }
  sprintf(movebuf, "%.2s", pos);
  if(!strcmp(movebuf, ".p")) {
    strcpy(movebuf, "pass");
  }
  if (who == GTH_WHO_BLACK)
    serial++;
  if (who == GTH_WHO_WHITE)
    ellipses = " ...";
  result = fprintf(fsock_out, "%d%s %s\r", serial, ellipses, movebuf);
  (void)fflush(fsock_out);
  if (result == -1) {
    WSAperror("make_move: fprintf");
    closeall();
    return GTH_STATE_ERROR;
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return GTH_STATE_ERROR;
  }
  switch(msg_code) {
  case 201:
    gth_winner = who;
    break;
  case 202:
    gth_winner = opponent(who);
    break;
  case 203:
    gth_winner = GTH_WHO_OTHER;
    break;
  }
  if (gth_winner != GTH_WHO_NONE) {
    closeall();
    return GTH_STATE_DONE;
  }
  if (msg_code != 200 && msg_code != 207) {
    fprintf(stderr, "make_move: bad result code %03d: %s\n", msg_code, msg_text);
    closeall();
    return GTH_STATE_ERROR;
  }
  if (msg_code == 207) {
    gth_my_time = get_time();
    if (gth_my_time == -1) {
      fprintf(stderr, "make_move: bad time info in %d: %s\n", msg_code, msg_text);
      closeall();
      return GTH_STATE_ERROR;
    }
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return GTH_STATE_ERROR;
  }
  if (!(msg_code >= 311 && msg_code <= 318)) {
    fprintf(stderr, "make_move: bad status code %03d: %s\n", msg_code, msg_text);
    closeall();
    return GTH_STATE_ERROR;
  }
  return GTH_STATE_CONTINUE;
}

enum gth_state gth_get_move(char *pos) {
  int result;
  
  if (who == GTH_WHO_NONE) {
    fprintf(stderr, "get_move: not initialized\n");
    return GTH_STATE_ERROR;
  }
  if (gth_winner != GTH_WHO_NONE) {
    fprintf(stderr, "get_move: game over\n");
    return GTH_STATE_ERROR;
  }
  if (who == GTH_WHO_WHITE)
    serial++;
  result = get_msg();
  if (result == -1) {
    closeall();
    return GTH_STATE_ERROR;
  }
  if ((msg_code < 311 || msg_code > 326) && msg_code != 361 && msg_code != 362) {
    fprintf(stderr, "get_move: bad status code %03d: %s\n", msg_code, msg_text);
    closeall();
    return GTH_STATE_ERROR;
  }
  if ((who == GTH_WHO_WHITE &&
       (msg_code == 312 || msg_code == 314 ||
	msg_code == 316 || msg_code == 318 ||
	msg_code == 323 ||
	msg_code == 324 || msg_code == 326)) ||
      (who == GTH_WHO_BLACK &&
       (msg_code == 311 || msg_code == 313 ||
	msg_code == 315 || msg_code == 317 ||
	msg_code == 321 ||
	msg_code == 322 || msg_code == 325))) {
    fprintf(stderr, "get_move: status code from wrong side %03d: %s\n", msg_code, msg_text);
    closeall();
    return GTH_STATE_ERROR;
  }
  if ((msg_code >= 311 && msg_code <= 318) ||
      (msg_code >= 321 && msg_code <= 326)) {
    get_move(pos);
  }
  switch(who) {
  case GTH_WHO_WHITE:
    switch(msg_code) {
    case 311:
    case 313:
    case 315:
    case 317:
      return GTH_STATE_CONTINUE;
    case 321:
    case 361:
      gth_winner = GTH_WHO_BLACK;
      return GTH_STATE_DONE;
    case 322:
    case 362:
      gth_winner = GTH_WHO_WHITE;
      return GTH_STATE_DONE;
    case 325:
      gth_winner = GTH_WHO_OTHER;
      return GTH_STATE_DONE;
    }
    break;
  case GTH_WHO_BLACK:
    switch(msg_code) {
    case 312:
    case 314:
    case 316:
    case 318:
      return GTH_STATE_CONTINUE;
    case 323:
    case 362:
      gth_winner = GTH_WHO_WHITE;
      return GTH_STATE_DONE;
    case 324:
    case 361:
      gth_winner = GTH_WHO_BLACK;
      return GTH_STATE_DONE;
    case 326:
      gth_winner = GTH_WHO_OTHER;
      return GTH_STATE_DONE;
    }
    break;
  default:
    break;
  }
  fprintf(stderr, "get_move: unknown status code %03d: %s\n", msg_code, msg_text);
  closeall();
  return GTH_STATE_ERROR;
}
