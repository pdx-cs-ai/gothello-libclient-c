#include "libgame.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static char *client_version = "0.9";
static enum gd_who who = GD_WHO_NONE;
static int sock;
static FILE *fsock_in, *fsock_out;
static int server_base = 29057;

static char buf[1024];
static char *msg_text;
static int msg_code;
static int serial = 0;

static void get_move(char *from, char *to) {
    int msg_serial;

    switch(msg_code) {
    case 311:
    case 313:
    case 321:
    case 322:
    case 325:
	sscanf(msg_text, "%d %2s-%2s", &msg_serial, from, to);
	break;
    case 312:
    case 314:
    case 323:
    case 324:
    case 326:
	sscanf(msg_text, "%d ... %2s-%2s", &msg_serial, from, to);
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
    perror("fgets");
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
}

static enum gd_who opponent(enum gd_who w) {
  assert(w == GD_WHO_WHITE || w == GD_WHO_BLACK);
  if (w == GD_WHO_WHITE)
    return GD_WHO_BLACK;
  return GD_WHO_WHITE;
}

int gd_time_controls = 0;
int gd_white_time_control = 0;
int gd_black_time_control = 0;
int gd_my_time = 0;
int gd_opp_time = 0;

static void get_time_controls(void) {
  int i;
  for (i = 0; i < strlen(msg_text); i++)
    if (isdigit(msg_text[i])) {
      gd_white_time_control = atoi(&msg_text[i]);
      while (isdigit(msg_text[i]))
	i++;
      break;
    }
  for (; i < strlen(msg_text); i++)
    if (isdigit(msg_text[i])) {
      gd_black_time_control = atoi(&msg_text[i]);
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

enum gd_who gd_winner = GD_WHO_NONE;

int gd_start_game(enum gd_who side, char *host, int server) {
  struct sockaddr_in serv_addr;
  struct hostent *hostent;
  int result;
  
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
    perror("sock");
    return -1;
  }
  result = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (result == -1) {
    perror("connect");
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
		   side == GD_WHO_WHITE ? "white" : "black");
  (void)fflush(fsock_out);
  if (result == -1) {
    perror("start_game: fprintf");
    closeall();
    return GD_STATE_ERROR;
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
    gd_time_controls = 1;
    get_time_controls();
    if (side == GD_WHO_WHITE) {
      gd_my_time = gd_white_time_control;
      gd_opp_time = gd_black_time_control;
    } else {
      gd_my_time = gd_black_time_control;
      gd_opp_time = gd_white_time_control;
    }
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return -1;
  }
  if ((msg_code != 351 && side == GD_WHO_WHITE) ||
      (msg_code != 352 && side == GD_WHO_BLACK)) {
    fprintf(stderr, "side failure %03d: %s\n", msg_code, msg_text);
    closeall();
    return -1;
  }
  who = side;
  return 0;
}

enum gd_state gd_make_move(char *from, char *to) {
  char *ellipses = "";
  int result;
  char movebuf[6];
  
  if (who == GD_WHO_NONE) {
    fprintf(stderr, "make_move: not initialized\n");
    return GD_STATE_ERROR;
  }
  if (gd_winner != GD_WHO_NONE) {
    fprintf(stderr, "make_move: game over\n");
    return GD_STATE_ERROR;
  }
  sprintf(movebuf, "%.2s-%.2s", from, to);
  if (who == GD_WHO_BLACK)
    serial++;
  if (who == GD_WHO_WHITE)
    ellipses = " ...";
  result = fprintf(fsock_out, "%d%s %s\r", serial, ellipses, movebuf);
  (void)fflush(fsock_out);
  if (result == -1) {
    perror("make_move: fprintf");
    closeall();
    return GD_STATE_ERROR;
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return GD_STATE_ERROR;
  }
  switch(msg_code) {
  case 201:
    gd_winner = who;
    break;
  case 202:
    gd_winner = opponent(who);
    break;
  case 203:
    gd_winner = GD_WHO_OTHER;
    break;
  }
  if (gd_winner != GD_WHO_NONE) {
    closeall();
    return GD_STATE_DONE;
  }
  if (msg_code != 200 && msg_code != 207) {
    fprintf(stderr, "make_move: bad result code %03d: %s\n", msg_code, msg_text);
    closeall();
    return GD_STATE_ERROR;
  }
  if (msg_code == 207) {
    gd_my_time = get_time();
    if (gd_my_time == -1) {
      fprintf(stderr, "make_move: bad time info in %d: %s\n", msg_code, msg_text);
      closeall();
      return GD_STATE_ERROR;
    }
  }
  result = get_msg();
  if (result == -1) {
    closeall();
    return GD_STATE_ERROR;
  }
  if (msg_code != 311 && msg_code != 312 && msg_code != 313 && msg_code != 314) {
    fprintf(stderr, "make_move: bad status code %03d: %s\n", msg_code, msg_text);
    closeall();
    return GD_STATE_ERROR;
  }
  return GD_STATE_CONTINUE;
}

enum gd_state gd_get_move(char *from, char *to) {
  int result;
  
  if (who == GD_WHO_NONE) {
    fprintf(stderr, "get_move: not initialized\n");
    return GD_STATE_ERROR;
  }
  if (gd_winner != GD_WHO_NONE) {
    fprintf(stderr, "get_move: game over\n");
    return GD_STATE_ERROR;
  }
  if (who == GD_WHO_WHITE)
    serial++;
  result = get_msg();
  if (result == -1) {
    closeall();
    return GD_STATE_ERROR;
  }
  if ((msg_code < 311 || msg_code > 326) && msg_code != 361 && msg_code != 362) {
    fprintf(stderr, "get_move: bad status code %03d: %s\n", msg_code, msg_text);
    closeall();
    return GD_STATE_ERROR;
  }
  if ((who == GD_WHO_WHITE &&
       (msg_code == 312 || msg_code == 314 || msg_code == 323 ||
	msg_code == 324 || msg_code == 326)) ||
      (who == GD_WHO_BLACK &&
       (msg_code == 311 || msg_code == 313 || msg_code == 321 ||
	msg_code == 322 || msg_code == 325))) {
    fprintf(stderr, "get_move: status code from wrong side %03d: %s\n", msg_code, msg_text);
    closeall();
    return GD_STATE_ERROR;
  }
  if ((msg_code >= 311 && msg_code <= 314) ||
      (msg_code >= 321 && msg_code <= 326)) {
    get_move(from, to);
    if (msg_code == 313 || msg_code == 314)
      gd_opp_time = get_time();
  }
  switch(who) {
  case GD_WHO_WHITE:
    switch(msg_code) {
    case 311:
    case 313:
      return GD_STATE_CONTINUE;
    case 321:
    case 361:
      gd_winner = GD_WHO_BLACK;
      return GD_STATE_DONE;
    case 322:
    case 362:
      gd_winner = GD_WHO_WHITE;
      return GD_STATE_DONE;
    case 325:
      gd_winner = GD_WHO_OTHER;
      return GD_STATE_DONE;
    }
    break;
  case GD_WHO_BLACK:
    switch(msg_code) {
    case 312:
    case 314:
      return GD_STATE_CONTINUE;
    case 323:
    case 362:
      gd_winner = GD_WHO_WHITE;
      return GD_STATE_DONE;
    case 324:
    case 361:
      gd_winner = GD_WHO_BLACK;
      return GD_STATE_DONE;
    case 326:
      gd_winner = GD_WHO_OTHER;
      return GD_STATE_DONE;
    }
    break;
  default:
    break;
  }
  fprintf(stderr, "get_move: unknown status code %03d: %s\n", msg_code, msg_text);
  closeall();
  return GD_STATE_ERROR;
}
