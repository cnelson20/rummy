typedef struct match {
	unsigned char shared_rank;
	
	unsigned char shared_suite;
	unsigned char lowest_rank;
	unsigned char highest_rank;
	
	unsigned char cards[16];
};

void shuffle_deck();
void prompt_difficulty();
void start_game();
void start_player_turn();

void draw_card_from_deck();

unsigned char max_hand_display_offset(unsigned char hand_size);

void parse_keys();
void parse_mouse_mvmt();
unsigned char *find_selection();

void remove_card_from_hand(unsigned char *card);
void discard_card_from_hand();
void remove_selections_from_hand();
void check_player_hand_size();

struct match *check_valid_move();
unsigned char compare_selection_match(struct match *matchptr);
struct match *compare_selection_player_matches(struct match *matches);
void integrate_cards_match(struct match *matchptr);
unsigned char is_one_rank_lower(unsigned char r1, unsigned char r2);

unsigned char is_lower_rank(unsigned char r1, unsigned char r2, unsigned char ace_high);
unsigned char is_lower_rank_both_aces(unsigned char r1, unsigned char r2);
void sort_pile_rank(unsigned char *pile);

void computer_turn();

unsigned short calc_pile_score(unsigned char *pile);
unsigned short calc_matches_score(struct match *matchptr, unsigned char matches_size);

void move_cards(unsigned char *dest_addr, unsigned char dest_offset,
	unsigned char *src_pile, unsigned char src_offset);

unsigned char get_card_suite(unsigned char card);
unsigned char get_card_color(unsigned char card);
unsigned char get_card_rank(unsigned char card);
unsigned char get_card_char(unsigned char card);

void setup_video();
void clear_rect(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);
unsigned char get_x_offset(unsigned char pile_num);
void draw_blank_card();
void draw_empty_card();
void draw_card(unsigned char card);
void draw_card_front(unsigned char card);
void draw_card_back();
void draw_deck_scratch_pile();
void draw_selected_card();
void display_piles();
void draw_hands();
void draw_deck_pile();

void poke_str(char *str);
unsigned char center_str_offset(unsigned char strlen);

void display_rummy_bigtext(unsigned char x_offset);
void display_game_controls();
void display_game_rules();

void display_win();

#define CARD_JACK 11
#define CARD_QUEEN 12
#define CARD_KING 13
#define CARD_ACE 1
#define CARD_NP 0

#define MAX_RANK_EXCL 14
#define CARD_ACE_HIGH 14

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 60

#define CARD_GRAPHICS_WIDTH 7
#define CARD_GRAPHICS_HEIGHT 10

#define CARD_DRAW_XOFF 3
#define CARD_DRAW_YOFF 0 

#define CH_LESS_THAN 0x3C
#define CH_GREATER_THAN 0x3E
#define CH_DASH 0x40

#define PILE_Y_OFFSET (COMP_PLAY_Y_END + 1)
#define PILE_Y_END (PILE_Y_OFFSET + CARD_GRAPHICS_HEIGHT)

#define COMP_PLAY_Y_OFFSET 6
#define COMP_PLAY_Y_END (COMP_PLAY_Y_OFFSET + CARD_GRAPHICS_HEIGHT + (CARD_GRAPHICS_HEIGHT >> 1))

#define PLAYER_PLAY_Y_OFFSET (PILE_Y_END + 1)
#define PLAYER_PLAY_Y_END (PLAYER_PLAY_Y_OFFSET + CARD_GRAPHICS_HEIGHT + (CARD_GRAPHICS_HEIGHT >> 1))

#define HAND_Y_OFFSET (SCREEN_HEIGHT - CARD_GRAPHICS_HEIGHT - 1)
#define HAND_Y_END (HAND_Y_OFFSET + CARD_GRAPHICS_HEIGHT + 1)