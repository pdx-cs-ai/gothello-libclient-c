enum gd_who {
  GD_WHO_NONE = 0,
  GD_WHO_WHITE = 1,
  GD_WHO_BLACK = 2,
  GD_WHO_OTHER = 3
};

enum gd_state {
  GD_STATE_ERROR = -1,
  GD_STATE_CONTINUE = 0,
  GD_STATE_DONE = 1
};

extern enum gd_who gd_winner;
extern int gd_time_controls;
extern int gd_white_time_control;
extern int gd_black_time_control;
extern int gd_my_time;
extern int gd_opp_time;

extern int gd_start_game(enum gd_who side, char *host, int server);
extern enum gd_state gd_make_move(char *from, char *to);
extern enum gd_state gd_get_move(char *from, char *to);
