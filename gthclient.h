enum aw_who {
  AW_WHO_NONE = 0,
  AW_WHO_WHITE = 1,
  AW_WHO_BLACK = 2,
  AW_WHO_OTHER = 3
};

enum aw_state {
  AW_STATE_ERROR = -1,
  AW_STATE_CONTINUE = 0,
  AW_STATE_DONE = 1
};

extern enum aw_who aw_winner;
extern int aw_time_controls;
extern int aw_white_time_control;
extern int aw_black_time_control;
extern int aw_my_time;
extern int aw_opp_time;

extern int aw_start_game(enum aw_who side, char *host, int server);
extern enum aw_state aw_make_move(char *from, char *to);
extern enum aw_state aw_get_move(char *from, char *to);
