enum gth_who {
  GTH_WHO_NONE = 0,
  GTH_WHO_WHITE = 1,
  GTH_WHO_BLACK = 2,
  GTH_WHO_OTHER = 3
};

enum gth_state {
  GTH_STATE_ERROR = -1,
  GTH_STATE_CONTINUE = 0,
  GTH_STATE_DONE = 1
};

extern enum gth_who gth_winner;
extern int gth_time_controls;
extern int gth_white_time_control;
extern int gth_black_time_control;
extern int gth_my_time;
extern int gth_opp_time;

extern int gth_start_game(enum gth_who side, char *host, int server);
extern enum gth_state gth_make_move(char *pos);
extern enum gth_state gth_get_move(char *pos);
