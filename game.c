#include <ascii_charmap.h>
#include <cbm.h>
#include <peekpoke.h>
#include <stdlib.h>

#include "helper.h"
#include "game.h"

#define MAX_DECK_SIZE 52
#define STARTING_HAND_SIZE 10

#define CHR_BELL 0x7

#define CLICKED_EMPTY ((unsigned char *)0xFFFF)
#define CLICKED_BOARD ((unsigned char *)0xFFFE)
#define CLICKED_LEFT ((unsigned char *)0xFFFD)
#define CLICKED_RIGHT ((unsigned char *)0xFFFC)

#define NEW_MATCH ((struct match *)0xFFFF)

extern unsigned char deck[MAX_DECK_SIZE];

extern unsigned char player_hand[MAX_DECK_SIZE];
extern unsigned char computer_hand[MAX_DECK_SIZE];

extern unsigned char discard[MAX_DECK_SIZE];

#define MAX_SELECT_SIZE 6

struct mouse mouse_input;
unsigned char *selected_cards[MAX_SELECT_SIZE];
unsigned char selected_cards_size;

struct match player_matches[MAX_DECK_SIZE];
struct match computer_matches[MAX_DECK_SIZE];

unsigned char player_matches_size;
unsigned char computer_matches_size;

unsigned char hand_display_offset;

unsigned char *picked_up_card;

unsigned char is_player_turn;
unsigned char already_drew_card;

unsigned char game_over;
unsigned char player_went_out_first;

unsigned char show_computer_hand = 0;

#define DEFAULT_BG_COLOR 0x5
unsigned char default_color = (DEFAULT_BG_COLOR << 4) | 0x1;

void main() {
	setup_video();
	display_game_controls();
	display_game_rules();

	while (1) {
		start_game();
		display_piles();
		
		while (!game_over) {
			waitforjiffy();
			if (is_player_turn) {
				parse_keys();
				parse_mouse_mvmt();
			} else {
				computer_turn();
				start_player_turn();
				display_piles();
			}
		};
		display_win();
		clear_screen();
	}
}

void start_player_turn() {
	is_player_turn = 1;
	already_drew_card = 0;
	
	selected_cards[0] = NULL;
	selected_cards_size = 0;
}

void start_game() {
	unsigned char i, deck_size;
	
	game_over = 0;
	player_went_out_first = 0;
	
	selected_cards[0] = NULL;
	selected_cards_size = 0;

	player_matches_size = 0;
	computer_matches_size = 0;	
	
	picked_up_card = NULL;
	
	hand_display_offset = 0;
	discard[0] = CARD_NP;
	
	shuffle_deck();
	
	// Deal out into hands
	deck_size = MAX_DECK_SIZE;
	
	for (i = 0; i < STARTING_HAND_SIZE; ++i) {
		--deck_size;
		computer_hand[i] = deck[deck_size];
		--deck_size;
		player_hand[i] = deck[deck_size];
	}
	computer_hand[STARTING_HAND_SIZE] = CARD_NP;
	player_hand[STARTING_HAND_SIZE] = CARD_NP;
	deck[deck_size] = CARD_NP;

	move_cards(discard, 0, deck, deck_size - 1);
	--deck_size;
	
	start_player_turn();
}

unsigned char get_pile_size(unsigned char *pile) {
	unsigned char i;
	i = 0;
	
	while (pile[i] != CARD_NP) {
		++i;
	}
	return i;
}

void draw_card_from_deck() {
	unsigned char *hand;
	unsigned char deck_size, hand_size;
	
	hand = (is_player_turn ? player_hand : computer_hand);
	
	deck_size = get_pile_size(deck);
	hand_size = get_pile_size(hand);
	
	if (deck_size > 0) {
		move_cards(hand, hand_size, deck, deck_size - 1);
	} else {
		unsigned char discard_size;
		deck_size = 0;
		while (discard_size != 0) {
			deck[deck_size] = --discard[discard_size];
			--discard_size;
			++deck_size;
		}
		discard[1] = CARD_NP;
		deck[deck_size] = CARD_NP;
	}
}

void move_cards(unsigned char *dest_pile, unsigned char dest_offset,
	unsigned char *src_pile, unsigned char src_offset) {
	
	unsigned char i;
	
	dest_pile += dest_offset;
	src_pile += src_offset;
	
	for (i = 0; src_pile[i] != CARD_NP; ++i) {
		dest_pile[i] = src_pile[i];
		src_pile[i] = CARD_NP;
	}
	dest_pile[i] = CARD_NP;
}

void shuffle_deck() {
	unsigned char ind, i;
	i = 1;
	// Populate deck
	for (ind = 0; ind < MAX_DECK_SIZE; ++ind) {
		deck[ind] = i;
		if ((i & 0xF) == CARD_KING) {
			i = (i & 0xF0) + 17;
		} else {
			++i;
		}
	}
	// Now shuffle it
	for (ind = 0; ind < MAX_DECK_SIZE - 1; ++ind) {
		i = (rand_word() % (MAX_DECK_SIZE - ind)) + ind;
		// Swap cards at indices ind and i
		__asm__ ("ldy %v", i);
		__asm__ ("ldx %v", ind);
		__asm__ ("lda %v, Y", deck);
		__asm__ ("pha");
		__asm__ ("lda %v, X", deck);
		__asm__ ("sta %v, Y", deck);
		__asm__ ("pla");
		__asm__ ("sta %v, X", deck);
		
	}
}

void remove_card_from_hand(unsigned char *card) {
	unsigned char *hand;
	unsigned char i;
	unsigned char offset;
	unsigned char hand_size;
	
	hand = (is_player_turn ? player_hand : computer_hand);
	offset = card - hand;
	hand_size = get_pile_size(hand);
	
	for (i = offset + 1; i < hand_size; ++i) {
		hand[i - 1] = hand[i];
	}
	hand[i - 1] = CARD_NP;
	
	display_piles();
}

void discard_card_from_hand() {
	unsigned char pile_size;
	
	pile_size = get_pile_size(discard);
	discard[pile_size] = *(selected_cards[0]);
	discard[pile_size + 1] = CARD_NP;
	
	remove_card_from_hand(selected_cards[0]);
}

unsigned char max_hand_display_offset(unsigned char hand_size) {
	unsigned char x = (get_x_offset(6) - CARD_GRAPHICS_WIDTH + CARD_DRAW_XOFF - get_x_offset(0)) / CARD_DRAW_XOFF;

	if (hand_size < x) {
		return 0;
	} else {
		return hand_size - x;
	}
}

void move_hand_offset_left() {
	if (hand_display_offset != 0) {
		--hand_display_offset;
		draw_hands();
	}
}

void move_hand_offset_right() {
	unsigned char player_hand_size = get_pile_size(player_hand);

	if (hand_display_offset < max_hand_display_offset(player_hand_size)) {
		++hand_display_offset;
		draw_hands();
	}
}

void parse_keys() {
	unsigned char key_pressed = cbm_k_getin();

	if (key_pressed == CH_CURS_LEFT) {
		move_hand_offset_left();
	} else if (key_pressed == CH_CURS_RIGHT) {
		move_hand_offset_right();
	}
}

void parse_mouse_mvmt() {
	unsigned char *selection;
	
	mouse_get(&mouse_input);
	
	selection = find_selection();
	
	// If clicked empty (a right click, deselect cards)
	if (selection == CLICKED_EMPTY) {
		selected_cards[0] = NULL;
		selected_cards_size = 0;
		draw_selected_card();
		return;
	} else if (selection == CLICKED_LEFT) {
		move_hand_offset_left();
	} else if (selection == CLICKED_RIGHT) {
		move_hand_offset_right();
	// If selection is from deck, draw a card if possible
	} else if (selection == deck) {
		if (already_drew_card) {
			//cbm_k_chrout(CHR_BELL);
		} else {
			draw_card_from_deck();
			already_drew_card = 1;
			picked_up_card = NULL;
			draw_deck_pile();
			draw_hands();
		}
	// If selection is play, then try to put selected_cards into play
	} else if (selection == CLICKED_BOARD) {
		struct match *matchptr;
		matchptr = check_valid_move();
		if (matchptr == NULL) {
			cbm_k_chrout(CHR_BELL);
			return;
		}
		picked_up_card = NULL;
		integrate_cards_match(matchptr);
	// If selection is a card in discard pile, either pick it up or discard a card from hand
	} else if (selection >= discard && selection < discard + sizeof(discard)) {
		if (already_drew_card) {
			// Discard the card in hand
			if (selected_cards_size == 1 && picked_up_card == NULL) {
				// Move card from hand to discard pile
				discard_card_from_hand();
				check_player_hand_size();

				selected_cards_size = 0;
				selected_cards[0] = NULL;
				draw_selected_card();
				
				is_player_turn = 0;
			} else if (picked_up_card != NULL) {
				// Undo pickup and put cards back
				move_cards(discard, get_pile_size(discard), picked_up_card, 0);
				selected_cards_size = 0;
				picked_up_card = NULL;
				already_drew_card = 0;
				display_piles();
			}
		} else if (discard[0] != CARD_NP) {
			// Draw cards from discard
			unsigned char pile_size;
			pile_size = get_pile_size(player_hand);
			
			picked_up_card = &( player_hand[pile_size] );
			move_cards(player_hand, pile_size, selection, 0);
			already_drew_card = 1;
			draw_deck_pile();
			draw_hands();
		}
	// If selection is a card in the player's hand, add it to selected_cards array
	} else if (selection >= player_hand && selection < player_hand + sizeof(player_hand)) { // valid pointer to a card
		unsigned char i;
		for (i = 0; i < selected_cards_size; ++i) {
			if (selected_cards[i] == selection) return;
		}
		if (selected_cards_size >= MAX_SELECT_SIZE) {
			return;
		}
		selected_cards[selected_cards_size] = selection;
		++selected_cards_size;
		selected_cards[selected_cards_size] = NULL;
		draw_selected_card();
	}
}

void check_player_hand_size() {
	unsigned char max_hand_offset;
	unsigned char player_hand_size = get_pile_size(player_hand);

	if (player_hand_size == 0) {
		game_over = 1;
		player_went_out_first = 1;
	}
	max_hand_offset = max_hand_display_offset(player_hand_size);
	if (hand_display_offset >= max_hand_offset) {
		hand_display_offset = max_hand_offset;
		draw_hands();
	}
}

void check_computer_hand_size() {
	if (get_pile_size(computer_hand) == 0) {
		game_over = 1;
		player_went_out_first = 0;
	}
}

unsigned char *find_selection() {
	static unsigned char mouse_click_last = 0;
	unsigned char click_eor;
	
	unsigned char tile_x, tile_y, tile_val;
	unsigned char pile_size;
	unsigned char offset;
	
	tile_x = mouse_input.x_pos >> 3;
	tile_y = mouse_input.y_pos >> 3;
	
	click_eor = mouse_input.buttons & (~mouse_click_last);
	mouse_click_last = mouse_input.buttons;
	
	if (click_eor & 0x2) {
		return CLICKED_EMPTY;
	} else if (click_eor & 0x1) {
		// If mouse clicked, see what card was clicked
		POKE(0x9F20, tile_x << 1);
		POKE(0x9F21, tile_y);
		POKE(0x9F22, 0);
		// if we clicked on a empty tile, return
		tile_val = PEEK(0x9F23);
		
		// We clicked on something!
		// Check if we've clicked on the deck or the discard pile
		if (tile_val != 0x20 && tile_y >= PILE_Y_OFFSET && tile_y < PILE_Y_END) {
			if (tile_x >= get_x_offset(0) && tile_x < get_x_offset(1)) {
				return deck;
			} else {
				offset = (tile_x - get_x_offset(1)) / 3;
				pile_size = get_pile_size(discard);
				if (pile_size == 0) {
					offset = 0;
				} else if (offset >= pile_size) {
					offset = pile_size - 1;
				}
				return &( discard[offset] );
			}
		// Check if we've clicked on the icon to put cards into play
		} else if (tile_val != 0x20 && tile_y >= PLAYER_PLAY_Y_OFFSET
			&& tile_y < PLAYER_PLAY_Y_OFFSET + CARD_GRAPHICS_HEIGHT) {
			if (tile_x >= get_x_offset(0) && tile_x < get_x_offset(1)) {
				return CLICKED_BOARD;
			}
		
		// Check if we've clicked on the player's hand
		} else if (tile_val != 0x20 && tile_y >= HAND_Y_OFFSET && tile_y < HAND_Y_END) {
			if (tile_x < get_x_offset(0)) {
				return CLICKED_LEFT;
			} else if (tile_x >= get_x_offset(6) - CARD_GRAPHICS_WIDTH + CARD_DRAW_XOFF 
				&& tile_x < get_x_offset(6)) {
				return CLICKED_RIGHT;
			}
			
			pile_size = get_pile_size(player_hand);
			
			if (pile_size != 0) {
				offset = (tile_x - get_x_offset(0)) / 3 + hand_display_offset;
				if (offset >= pile_size) { offset = pile_size - 1; }
				return &( player_hand[offset] );
			}
		}		
	}
	return NULL;
}

unsigned char rank_match;
unsigned char suite_match;
unsigned char contains_picked_up_card;
unsigned char in_row;

struct match *check_valid_move() {
	unsigned char first_card_rank;
	unsigned char first_card_suite;
	unsigned char i;
	
	unsigned char scardranks[MAX_SELECT_SIZE + 1];
	struct match *found_match;
	
	// If no cards, not going to be valid
	if (selected_cards_size == 0) { return 0; }
	
	// Check if selected_cards is a valid move by itself
	contains_picked_up_card = (picked_up_card == NULL);
	
	first_card_rank = get_card_rank(*(selected_cards[0]));
	rank_match = 1;
	first_card_suite = get_card_suite(*(selected_cards[0]));
	suite_match = 1;
	in_row = 0;
	
	for (i = 0; i < selected_cards_size; ++i) {
		unsigned char loop_card;
		loop_card = *(selected_cards[i]);
		scardranks[i] = get_card_rank(loop_card);
		
		if (rank_match && first_card_rank != get_card_rank(loop_card)) {
			rank_match = 0;
		}
		if (suite_match && first_card_suite != get_card_suite(loop_card)) {
			suite_match = 0;
		}
		if (!contains_picked_up_card && selected_cards[i] == picked_up_card) {
			contains_picked_up_card = 1;
		}
	}
	if (!contains_picked_up_card) return NULL;
	
	// Must have 3 of a kind or in a row to play out
	if (selected_cards_size >= 3 && rank_match) {
			return NEW_MATCH;
	} 
	if (suite_match) {
		scardranks[selected_cards_size] = CARD_NP;
		sort_pile_rank(scardranks);
		// Go through cards to see if in order
		for (i = selected_cards_size - 1; i != 0; --i) {
			if (!is_one_rank_lower( scardranks[i - 1], scardranks[i] )) break;
		}
		if (i == 0 && is_lower_rank_both_aces(scardranks[0], scardranks[selected_cards_size - 1])) in_row = 1;
	}
	if (selected_cards_size >= 3 && in_row) {
		return NEW_MATCH;
	}

	// Compare against same player's matches
	found_match = compare_selection_player_matches(is_player_turn ? player_matches : computer_matches);
	if (found_match) return found_match;
	// Compare agains other matches
	found_match = compare_selection_player_matches(is_player_turn ? computer_matches : player_matches);
	if (found_match) return found_match;

	return NULL;
}

unsigned char comp_mcths_temp_pile[MAX_SELECT_SIZE + 1];
unsigned char comp_mtchs_psize;

struct match *compare_selection_player_matches(struct match *matches) {
	unsigned char size = (matches == player_matches) ? player_matches_size : computer_matches_size;
	unsigned char i;

	for (i = 0; i < selected_cards_size; ++i) {
		comp_mcths_temp_pile[i] = *( selected_cards[i] );
	}
	comp_mtchs_psize = i;
	comp_mcths_temp_pile[i] = CARD_NP;

	sort_pile_rank(comp_mcths_temp_pile);

	for (i = 0; i < size; ++i) {
		if (compare_selection_match(&( matches[i] ))) {
			return &( matches[i] );
		}
	}
	
	return NULL;	
}

unsigned char compare_selection_match(struct match *matchptr) {
	if (rank_match != 0 && selected_cards_size == 1 && matchptr->shared_rank) {
		if (matchptr->shared_rank == get_card_rank( comp_mcths_temp_pile[0] )) return 1;

	} else if (suite_match != 0 && in_row != 0 && matchptr->shared_suite != 0) {
		// Suites match
		if (matchptr->shared_suite == get_card_suite( comp_mcths_temp_pile[0] )) {
			// matchptr is [6, 7, 8] and temp_pile [9, 10]
			if (is_one_rank_lower( matchptr->highest_rank, get_card_rank(comp_mcths_temp_pile[0]) )) return 1;
			// matchptr is [6, 7, 8] and temp_pile [4, 5]
			if (is_one_rank_lower( get_card_rank(comp_mcths_temp_pile[comp_mtchs_psize - 1]), matchptr->lowest_rank )) return 1;
			
		}
	}
	// TODO: add more checks here

	return 0;
}

void remove_selections_from_hand() {
	unsigned char i, j;

	for (i = 0; i < selected_cards_size; ++i) {
		remove_card_from_hand(selected_cards[i]);
		for (j = i + 1; j < selected_cards_size; ++j) {
			if (selected_cards[j] > selected_cards[i]) {
				--(selected_cards[j]);
			}
		}
	}
}

void create_new_match() {
	struct match *newmtch;
	unsigned char i;
	
	if (is_player_turn) {
		newmtch = &player_matches[player_matches_size];
		++player_matches_size;
	} else {
		newmtch = &computer_matches[computer_matches_size];
		++computer_matches_size;
	}
	
	for (i = 0; i < selected_cards_size; ++i) {
		newmtch->cards[i] = *(selected_cards[i]);
	}
	newmtch->cards[i] = CARD_NP;
	
	remove_selections_from_hand();
	
	if (get_card_rank(newmtch->cards[0]) == get_card_rank(newmtch->cards[1])) {
		// 3 of a kind
		newmtch->shared_rank = get_card_rank(newmtch->cards[0]);
		newmtch->shared_suite = 0;	
	} else {
		// 3 in a row
		newmtch->shared_rank = 0;
		newmtch->shared_suite = get_card_suite(newmtch->cards[0]);
		sort_pile_rank(newmtch->cards);
		newmtch->lowest_rank = get_card_rank(newmtch->cards[0]);
		newmtch->highest_rank = get_card_rank(newmtch->cards[ i - 1 ]);
	}
}

unsigned char get_max_rank_pile(unsigned char *pile) {
	unsigned char i;
	unsigned char max_rank;
	
	max_rank = get_card_rank(pile[0]);
	for (i = 0; pile[i] != CARD_NP; ++i) {
		unsigned char c_rank = get_card_rank(pile[i]);
		if (c_rank > max_rank) { max_rank = c_rank; }
	}
	return max_rank;
}

unsigned char get_max_rank_pile_ace_high(unsigned char *pile) {
	unsigned char i;
	unsigned char max_rank;
	
	max_rank = get_card_rank(*pile);
	//	if (max_rank == CARD_ACE) { return CARD_ACE_HIGH; }
	for (i = 0; pile[i] != CARD_NP; ++i) {
		unsigned char c_rank = get_card_rank(pile[i]);
		if (c_rank == CARD_ACE) { return CARD_ACE_HIGH; }
		if (c_rank > max_rank) { max_rank = c_rank; }
	}
	return max_rank;
}

unsigned char is_lower_rank(unsigned char r1, unsigned char r2, unsigned char ace_high) {
	if (ace_high) {
		if (r1 == CARD_ACE) r1 += 13;
		if (r2 == CARD_ACE) r2 += 13;
	}
	return r1 <= r2;
}

unsigned char is_lower_rank_both_aces(unsigned char r1, unsigned char r2) {
	if (r2 == CARD_ACE) return 1;
	return r1 <= r2;
}

unsigned char is_one_rank_lower(unsigned char r1, unsigned char r2) {
	if (r1 == CARD_KING && r2 == CARD_ACE) {
		return 1;
	}
	return (r1 + 1) == r2;
}

void sort_pile_rank(unsigned char *pile) {
	unsigned char i;
	unsigned char lowest_rank, lowind;
	unsigned char max_rank = 0;
	unsigned char ace_high;
	
	begin_function:
	
	if (*pile == CARD_NP) { return; }
	if (max_rank == 0) { 
		max_rank = get_max_rank_pile(pile); 
		ace_high = (max_rank == CARD_KING);
	}
	
	lowest_rank = get_card_rank(pile[0]);
	lowind = 0;
	if (pile[0] != CARD_NP) for (i = 1; pile[i] != CARD_NP; ++i) {
		unsigned char comp_rank = get_card_rank(pile[i]);
		if (is_lower_rank(comp_rank, lowest_rank, ace_high)) {
			lowest_rank = comp_rank;
			lowind = i;
		}
	}
	i = pile[0];
	pile[0] = pile[lowind];
	pile[lowind] = i;
	
	++pile;
	goto begin_function;
}

void add_to_match(struct match *matchptr) {
	unsigned char i;

	if (matchptr->shared_rank != 0) {
		// Add to 3-of-a-kind
		matchptr->cards[3] = *(selected_cards[0]);
		matchptr->cards[4] = CARD_NP;
	} else {
		unsigned char temp_pile[MAX_SELECT_SIZE + 1];
		for (i = 0; i < selected_cards_size; ++i) {
			temp_pile[i] = *(selected_cards[i]);
		}
		temp_pile[i] = CARD_NP;

		sort_pile_rank(temp_pile);
		if (is_one_rank_lower( matchptr->highest_rank, get_card_rank(temp_pile[0]) )) {
			matchptr->highest_rank = get_card_rank(temp_pile[selected_cards_size - 1]);
		} else {
			matchptr->lowest_rank = get_card_rank(temp_pile[0]);
		}
		move_cards(matchptr->cards, get_pile_size(matchptr->cards), temp_pile, 0);
		sort_pile_rank(matchptr->cards);
	}
	remove_selections_from_hand();
}

void play_off_new_match(struct match *matchptr) {
	struct match *newmtch;
	unsigned char i;

	if (is_player_turn) {
		newmtch = &player_matches[player_matches_size];
		++player_matches_size;
	} else {
		newmtch = &computer_matches[computer_matches_size];
		++computer_matches_size;
	}

	for (i = 0; i < selected_cards_size; ++i) {
		newmtch->cards[i] = *(selected_cards[i]);
	}
	newmtch->cards[selected_cards_size] = CARD_NP;
	remove_selections_from_hand();

	if (matchptr->shared_rank != 0) {
		newmtch->shared_rank = matchptr->shared_rank;
	} else {
		newmtch->shared_suite = matchptr->shared_suite;

		sort_pile_rank(newmtch->cards);
		if (is_one_rank_lower(matchptr->highest_rank, get_card_rank(newmtch->cards[0]))) {
			// Play to the higher end of the existing match
			// [6,7,8] is matchptr, [9,10] is newmtch
			newmtch->lowest_rank = matchptr->lowest_rank;
			newmtch->highest_rank = get_card_rank(newmtch->cards[selected_cards_size - 1]);
		} else {
			// Opposite
			// [6,7,8] is matchptr [4,5] is newmtch
			newmtch->lowest_rank = get_card_rank(newmtch->cards[0]);
			newmtch->highest_rank = matchptr->highest_rank;
		}
	}
}

void integrate_cards_match(struct match *matchptr) {
	struct match *which_play = is_player_turn ? player_matches : computer_matches;

	if (matchptr == NEW_MATCH) {
		create_new_match();
	} else if (matchptr >= which_play && (unsigned char *)matchptr < ((unsigned char *)which_play + sizeof(player_matches)) ) {
		// Adding on to the same player's pile
		add_to_match(matchptr);
	} else {
		// Playing off of the other player's pile, needs a new pile
		play_off_new_match(matchptr);
	}

	selected_cards[0] = NULL;
	selected_cards_size = 0;
	check_player_hand_size();
	draw_selected_card();
	display_piles();
	return;
}

/*
	calc_rank_counts()
	fills rank_count_arr with counts of each rank in pile
	returns get_pile_size(pile)
*/
unsigned char calc_rank_counts(unsigned char *rank_count_arr, unsigned char *pile) {
	unsigned char i;
	unsigned char pile_size;

	for (i = 0; i < MAX_RANK_EXCL; ++i) {
		rank_count_arr[i] = 0;
	}
	pile_size = get_pile_size(pile);
	for (i = 0; i < pile_size; ++i) {
		++(rank_count_arr[ get_card_rank(pile[i]) ]);
	}

	return pile_size;
}

extern unsigned char rank_counts[MAX_RANK_EXCL];
//unsigned char rank_counts[MAX_RANK_EXCL];

void computer_turn() {
	unsigned char comp_hand_size;
	unsigned char i;

	unsigned char pile_size;
	unsigned char minind, minval;

	selected_cards_size = 0;
	selected_cards[0] = NULL;

	// Populate array of count of each rank
	comp_hand_size = calc_rank_counts(rank_counts, computer_hand);
	
	// Decide between drawing and picking up a card
	already_drew_card = 0;
	
	// If picking up the top card would be a match of 3, do so
	pile_size = get_pile_size(discard);
	for (i = 1; i < 2; ++i) {
		unsigned char card_rank;
		
		if (i > pile_size) continue;
		card_rank = get_card_rank(discard[pile_size - i]);

		++rank_counts[ card_rank ];
		if (rank_counts[ card_rank ] >= 3) {
			move_cards(computer_hand, comp_hand_size, discard, pile_size - i);
			already_drew_card = 1; // Card is taken from discard, now we don't draw from the deck
			break;
		}
	}
	// Recalc rank_counts since we messed with it
	comp_hand_size = calc_rank_counts(rank_counts, computer_hand);
	
	// We haven't picked up anything, draw instead
	if (!already_drew_card) {
		draw_card_from_deck();
		++(rank_counts[ get_card_rank(computer_hand[comp_hand_size]) ]);
		++comp_hand_size;
	}

	// Draw hands again so player can we we picked up / drew cards
	// Wait 1 sec before doing anything else
	display_piles();
	for (i = 0; i < JIFFIES_PER_SEC; ++i) {
		waitforjiffy();
	}
	
	// Figure out if we can lay down anything
	// Lay down whatever we can

	// Check for 3 of a kinds
	for (i = 0; i < MAX_RANK_EXCL; ++i) {
		unsigned char j;

		if (rank_counts[i] < 3) continue;
		// We have three of a kind
		selected_cards_size = 0;
		for (j = 0; j < comp_hand_size; ++j) {
			if (get_card_rank(computer_hand[j]) == i) {
				selected_cards[selected_cards_size] = &( computer_hand[j] );
				++selected_cards_size;
				--rank_counts[i];
			}
		}
		integrate_cards_match(NEW_MATCH);
		comp_hand_size = get_pile_size(computer_hand);
	}
	// Check for 3 in a row
	sort_pile_rank(computer_hand);

	for (i = 0; i < comp_hand_size - 2; ++i) {
		unsigned char r0, r1, r2;

		if (get_card_suite(computer_hand[i]) != get_card_suite(computer_hand[i + 1])) continue;
		if (get_card_suite(computer_hand[i]) != get_card_suite(computer_hand[i + 2])) continue;

		r0 = get_card_rank(computer_hand[i]);
		r1 = get_card_rank(computer_hand[i + 1]);
		r2 = get_card_rank(computer_hand[i + 2]);

		if (!is_one_rank_lower(r0, r1)) continue;
		if (!is_one_rank_lower(r1, r2)) continue;
		if (!is_lower_rank_both_aces(r0, r2)) continue;

		selected_cards_size = 3;
		selected_cards[0] = &( computer_hand[i] );
		selected_cards[1] = &( computer_hand[i + 1] );
		selected_cards[2] = &( computer_hand[i + 2] );
		integrate_cards_match(NEW_MATCH);
		selected_cards_size = 0;
		comp_hand_size -= 3;
		--rank_counts[ r0 ];
		--rank_counts[ r1 ];
		--rank_counts[ r2 ];
		--i;
	}
	// Check if each card goes with anything on the board
	for (i = 0; i < comp_hand_size; ++i) {
		struct match *match_found;

		selected_cards[0] = & ( computer_hand[i] );
		selected_cards_size = 1;
		match_found = compare_selection_player_matches(computer_matches);
		if (match_found == NULL) match_found = compare_selection_player_matches(player_matches);
		if (match_found != NULL) {
			--rank_counts[ get_card_rank(computer_hand[i]) ];
			integrate_cards_match(match_found);
			--comp_hand_size;
			--i;
		}
		selected_cards_size = 0;
		selected_cards[0] = NULL;
	}


	check_computer_hand_size();

	// Discard something
	if (comp_hand_size > 0) {
		unsigned char discard_rank_counts[MAX_RANK_EXCL];
		unsigned char discard_size = calc_rank_counts(discard_rank_counts, discard);

		// Find out what rank we have least of that is not zero
		minind = 0xFF;
		minval = 0xFF;
		for (i = 1; i < MAX_RANK_EXCL; ++i) {
			if (discard_rank_counts[i] < 2 && rank_counts[i] != 0 && rank_counts[i] < minval) {
				minval = rank_counts[i];
				minind = i;
			}
		}
		// minind is the rank we have least of that wont be a rummy
		if (minind == 0xFF) {
			selected_cards[0] = &( computer_hand[0] );	
		} else {
			for (i = 0; i < comp_hand_size; ++i) {
				if (minind == get_card_rank( computer_hand[i] )) {
					selected_cards[0] = &( computer_hand[i] );
					break;
				}
			}
		}

		// If we haven't discarded anything we will discard the first card in our hand
		if (selected_cards[0] == NULL) {
			selected_cards[0] = &( computer_hand[0] );
		}
		selected_cards_size = 1;
		discard_card_from_hand();
	}

	check_computer_hand_size();

	for (i = 0; i < JIFFIES_PER_SEC; ++i) {
		waitforjiffy();
	}
}

unsigned short calc_pile_score(unsigned char *pile) {
	unsigned short score = 0;
	unsigned char i;

	for (i = 0; pile[i] != CARD_NP; ++i) {
		unsigned char card_rank = get_card_rank(pile[i]);
		if (card_rank == CARD_ACE) {
			score += 15;
		} else if (card_rank < CARD_JACK) {
			// 2-10
			score += 5;
		} else {
			// Face card
			score += 10;
		}
	}

	return score;
}

unsigned short calc_matches_score(struct match *matchptr, unsigned char matches_size) {
	unsigned short score = 0;
	unsigned char i;

	for (i = 0; i < matches_size; ++i) {
		score += calc_pile_score(matchptr[i].cards);
	}

	return score;
}

// Spade, Club, Heart, Diamond
unsigned char suite_petscii_table[] = {0x41, 0x58, 0x53, 0x5A};
unsigned char suite_colors_table[] = {0x60, 0x60, 0x62, 0x62};

unsigned char get_card_suite(unsigned char card) {
	return suite_petscii_table[(card & 0xF0) >> 4];
}

unsigned char get_card_color(unsigned char card) {
	return suite_colors_table[(card & 0xF0) >> 4];
}

unsigned char get_card_rank(unsigned char card) {
	return card & 0xF;
}

// Ten, Jack, Queen, King
unsigned char rank_petscii_table[] = {0x14, 0x0A, 0x11, 0x0B};

unsigned char get_card_char(unsigned char card) {
	unsigned char card_num = card & 0xF;
	
	if (card_num == 1) {
		return 0x01; // screen code for 'A'
	} else if (card_num >= 10) {
		return rank_petscii_table[card_num - 10];
	} else {
		return card_num | 0x30;
	}
}

unsigned char last4_char_updates[] = {
	0x00, 0x00, 0x00, 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 
	0x00, 0x00, 0x00, 0xE0, 0xF0, 0xF8, 0xF8, 0xF8, 
	0x1F, 0x1F, 0x1F, 0x0F, 0x07, 0x00, 0x00, 0x00, 
	0xF8, 0xF8, 0xF8, 0xF0, 0xE0, 0x00, 0x00, 0x00,
};

/*
	More graphics code
*/

void setup_video() {
	unsigned char i;
	
	cbm_k_chrout(0x8E);
	
	POKE(0x9F25, 0);
	// 80x60
	POKE(0x9F2A, 128);
	POKE(0x9F2B, 128);
	// set mapbase to 00000
	POKE(0x9F35, 0);
	
	POKEW(0x9F20, 0xF7E0);
	POKE(0x9F22, 0x11);
	for (i = 0; i < sizeof(last4_char_updates); ++i) {
		POKE(0x9F23, last4_char_updates[i]);
	}
	
	POKEW(0x9F20, 0xFA00 + (5 * 2));
	POKE(0x9F23, 0x61);
	POKE(0x9F23, 0x0);
	
	enable_mouse();
	
	clear_screen();
}

unsigned char card_graphics_array_top[] = {0x55, 0x40, 0x40, 0x40, 0x40, 0x40, 0x49, 0x20};
unsigned char card_graphics_array_middle[] = {0x5D, 0x66, 0x66, 0x66, 0x66, 0x66, 0x5D, 0x20};
unsigned char card_graphics_array_bottom[] = {0x4A, 0x40, 0x40, 0x40, 0x40, 0x40, 0x4B, 0x20};

unsigned char card_graphics_blank[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
unsigned char card_graphics_empty[] = {0x5D, 0x60, 0x60, 0x60, 0x60, 0x60, 0x5D, 0x20};

unsigned char card_graphics_undo[] = {0x5D, 'U' - 0x40, 'N' - 0x40, 'D' - 0x40, 'O' - 0x40, 0x60, 0x5D, 0x20};

unsigned char card_front_array_top[] = {0xFC, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xFD, 0x20};
unsigned char card_front_array_bottom[] = {0xFE, 0xF9, 0xF9, 0xF9, 0xF9, 0xF9, 0xFF, 0x20};

unsigned char card_graphics_tables[][CARD_GRAPHICS_HEIGHT - 2] = {
	{0, 0, 0, 0, 0, 0, 0, 0}, // N/A
	{3, 30, 0, 1, 0, 31, 4, 0}, // Ace
	{3, 30, 1, 0, 1, 31, 4, 0}, // 2
	{3, 32, 0, 1, 0, 33, 4, 0}, // 3
	{7, 0, 0, 0, 0, 0, 8, 0}, // 4
	{7, 0, 0, 1, 0, 0, 8, 0}, // 5
	{7, 0, 0, 2, 0, 0, 8, 0}, // 6
	{7, 1, 0, 2, 0, 0, 8, 0}, // 7
	{7, 1, 0, 2, 0, 1, 8, 0}, // 8
	{7, 0, 2, 1, 2, 0, 8, 0}, // 9
	{7, 1, 2, 0, 2, 1, 8, 0}, // 10
	{9, 10, 11, 12, 13, 14, 15, 0}, // Jack
	{23, 24, 25, 26, 27, 28, 29, 0}, // Queen
	{16, 17, 18, 19, 20, 21, 22, 0}, // King
};

unsigned char card_front_rows[][CARD_GRAPHICS_WIDTH - 2] = {
	{0x60, 0x60, 0x60, 0x60, 0x60}, // 0
	{0x60, 0x60, 0x01, 0x60, 0x60}, // 1
	{0x60, 0x01, 0x60, 0x01, 0x60}, // 2
	
	{0x02, 0x60, 0x60, 0x60, 0x60}, // 3
	{0x60, 0x60, 0x60, 0x60, 0x02}, // 4
	
	{0x02, 0x60, 0x01, 0x60, 0x60}, // 5
	{0x60, 0x60, 0x01, 0x60, 0x02}, // 6
	
	{0x02, 0x01, 0x60, 0x01, 0x60}, // 7
	{0x60, 0x01, 0x60, 0x01, 0x02}, // 8
	
	{0x02, 0x60, 0x5F, 0xA0, 0x69}, // 9
	{0x01, 0x60, 0x60, 0x51, 0x60}, // 10
	{0x6A, 0x60, 0xE9, 0xA0, 0x60}, // 11
	{0x6A, 0xE9, 0xA0, 0x69, 0x74}, // 12
	{0x60, 0xA0, 0x69, 0x60, 0x74}, // 13
	{0x60, 0x51, 0x60, 0x60, 0x01}, // 14
	{0xE9, 0xA0, 0xDF, 0x60, 0x02}, // 15
	
	{0x02, 0x5F, 0xA0, 0x69, 0x60}, // 16
	{0x01, 0x60, 0x51, 0x60, 0x60}, // 17
	{0x60, 0x60, 0xA0, 0xDF, 0x74}, // 18
	{0x60, 0xE9, 0xA0, 0x69, 0x60}, // 19
	{0x6A, 0x5F, 0xA0, 0x60, 0x60}, // 20
	{0x60, 0x60, 0x51, 0x60, 0x01}, // 21
	{0x60, 0xE9, 0xA0, 0xDF, 0x02}, // 22
	
	{0x02, 0x60, 0x51, 0x5F, 0x60}, // 23
	{0x01, 0x60, 0xE9, 0xDF, 0x60}, // 24
	{0x60, 0x60, 0xA0, 0x69, 0x60}, // 25
	{0x60, 0x60, 0xA0, 0x60, 0x60}, // 26
	{0x60, 0xE9, 0xA0, 0x60, 0x60}, // 27
	{0x60, 0x5F, 0x69, 0x60, 0x01}, // 28
	{0x60, 0xDF, 0x51, 0x60, 0x02}, // 29
	
	{0x01, 0x60, 0x60, 0x60, 0x60}, // 30
	{0x60, 0x60, 0x60, 0x60, 0x01}, // 31
	
	{0x01, 0x60, 0x01, 0x60, 0x60}, // 32
	{0x60, 0x60, 0x01, 0x60, 0x01}, // 33
};

void clear_rect(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2) {
	unsigned char xoff; 
	unsigned char i, times;
	
	if (x1 >= x2) { return; }
	
	xoff = x1 << 1;
	times = x2 - x1;
	
	POKE(0x9F22, 0x10);
	POKE(0x9F21, y1);
	while (PEEK(0x9F21) < y2) {
		POKE(0x9F20, xoff);
		for (i = times; i != 0; --i) {
			POKE(0x9F23, 0x20);
			POKE(0x9F23, default_color);
		}
		__asm__ ("inc $9F21");
	}
}

void draw_line(unsigned char *graphics) {
	unsigned char i;
	
	for (i = 0; i < CARD_GRAPHICS_WIDTH; ++i) {
		POKE(0x9F23, graphics[i]);
		POKE(0x9F23, default_color);
	}
	__asm__ ("inc $9F21");
}

void draw_blank_card() {
	unsigned char i;
	unsigned char xoff, yoff;
	
	xoff = PEEK(0x9F20);
	yoff = PEEK(0x9F21);	
	
	for (i = 0; i < CARD_GRAPHICS_HEIGHT; ++i) {
		POKE(0x9F20, xoff);
		draw_line(card_graphics_blank);
	}
	
	POKE(0x9F21, yoff + CARD_DRAW_YOFF);
	POKE(0x9F20, xoff + (CARD_DRAW_XOFF << 1));
}

void draw_undo_card() {
	unsigned char i;
	unsigned char xoff, yoff;

	xoff = PEEK(0x9F20);
	yoff = PEEK(0x9F21);

	draw_line(card_graphics_array_top);

	for (i = 1; i < CARD_GRAPHICS_HEIGHT - 1; ++i) {
		POKE(0x9F20, xoff);
		if (i + 1 == (CARD_GRAPHICS_HEIGHT >> 1)) {
			draw_line(card_graphics_undo);
		} else {
			draw_line(card_graphics_empty);
		}
	}

	POKE(0x9F20, xoff);
	draw_line(card_graphics_array_bottom);

	POKE(0x9F21, yoff + CARD_DRAW_YOFF);
	POKE(0x9F20, xoff + (CARD_DRAW_XOFF << 1));
}

void draw_empty_card() {
	unsigned char xoff, yoff, i;
	yoff = PEEK(0x9F21);
	xoff = PEEK(0x9F20);
	
	draw_line(card_graphics_array_top);
	for (i = 0; i < CARD_GRAPHICS_HEIGHT - 2; ++i) {
		POKE(0x9F20, xoff);
		draw_line(card_graphics_empty);
	}
	POKE(0x9F20, xoff);
	draw_line(card_graphics_array_bottom);
	
	POKE(0x9F21, yoff + CARD_DRAW_YOFF);
	POKE(0x9F20, xoff + (CARD_DRAW_XOFF << 1));
}

void draw_card(unsigned char card) {
	if (card != 0) {
		draw_card_front(card);
	} else {
		draw_card_back();
	}
}

void draw_front_line(unsigned char card, unsigned char *line) {
	unsigned char i;
	unsigned char schar, scolor, suite;
	
	suite = get_card_suite(card);
	schar = get_card_char(card);
	scolor = get_card_color(card);
	
	POKE(0x9F23, 0xF5);
	POKE(0x9F23, default_color);
	
	scolor = (scolor & 0xf) | 0x10;
	
	for (i = 0; i < CARD_GRAPHICS_WIDTH - 2; ++i) {
		if (line[i] == 0x02) {
			POKE(0x9F23, schar);
			POKE(0x9F23, scolor);
		} else if (line[i] == 0x01) {
			POKE(0x9F23, suite);
			POKE(0x9F23, scolor);
		} else {
			POKE(0x9F23, line[i]);
			POKE(0x9F23, scolor);
		}
	}
	
	POKE(0x9F23, 0xF6);
	POKE(0x9F23, default_color);
	__asm__ ("inc $9F21");
}

void draw_card_front(unsigned char card) {
	unsigned char x_offset, y_offset;
	unsigned char j;
	unsigned char suite_char, suite_color, card_rank;
	
	suite_char = get_card_suite(card);
	suite_color = get_card_color(card);
	card_rank = get_card_rank(card);
	
	x_offset = PEEK(0x9F20);
	y_offset = PEEK(0x9F21);
	
	// Draw top row of card
	draw_line(card_front_array_top);
	
	// Draw middle of card
	for (j = 0; j < (CARD_GRAPHICS_HEIGHT - 2); ++j) {
		POKE(0x9F20, x_offset);
		draw_front_line(card, card_front_rows[ card_graphics_tables[card_rank][j] ]);
	}
	// Draw last row of card
	POKE(0x9F20, x_offset);
	draw_line(card_front_array_bottom);
	
	POKE(0x9F20, x_offset + (CARD_DRAW_XOFF << 1));
	POKE(0x9F21, y_offset + CARD_DRAW_YOFF);
}

void draw_card_back() {
	unsigned char x_offset, y_offset;
	unsigned char j;
	
	x_offset = PEEK(0x9F20);
	y_offset = PEEK(0x9F21);
	
	// Draw top row of card
	draw_line(card_graphics_array_top);
	
	// Draw middle of card
	for (j = 0; j < (CARD_GRAPHICS_HEIGHT - 2); ++j) {
		POKE(0x9F20, x_offset);
		draw_line(card_graphics_array_middle);
	}
	// Draw last row of card
	POKE(0x9F20, x_offset);
	draw_line(card_graphics_array_bottom);
	
	POKE(0x9F20, x_offset + (CARD_DRAW_XOFF << 1));
	POKE(0x9F21, y_offset + CARD_DRAW_YOFF);
}

void draw_half_card_back() {
	unsigned char x_offset, y_offset;
	unsigned char j;
	
	x_offset = PEEK(0x9F20);
	y_offset = PEEK(0x9F21);
	
	for (j = (CARD_GRAPHICS_HEIGHT / 2) - 1; j != 0; --j) {
		POKE(0x9F20, x_offset);
		draw_line(card_graphics_array_middle);
	}
	// Draw last row of card
	POKE(0x9F20, x_offset);
	draw_line(card_graphics_array_bottom);
	
	POKE(0x9F20, x_offset + (CARD_DRAW_XOFF << 1));
	POKE(0x9F21, y_offset + CARD_DRAW_YOFF);
}

unsigned char x_offset_table[7 + 1];

unsigned char get_x_offset(unsigned char pile_num) {
	unsigned char val;
	
	val = x_offset_table[pile_num];
	if (val != 0) {
		return val;
	}
	// Need to calculate offset
	val = (2 + (CARD_GRAPHICS_WIDTH + 1) * pile_num);
	x_offset_table[pile_num] = val;
	return val;
}

void draw_selected_card() {
	unsigned char i, xoff;
	
	POKE(0x9F20, get_x_offset(6) << 1);
	POKE(0x9F21, HAND_Y_OFFSET);
	POKE(0x9F22, 0x10);
	
	if (is_player_turn && selected_cards_size != 0) {		
		draw_card(**selected_cards);
	} else {
		draw_blank_card();
	}
	if (is_player_turn) for (i = 1; i < selected_cards_size; ++i) {
		draw_card(*(selected_cards[i]));
	}
	xoff = (PEEK(0x9F20) >> 1) + (CARD_GRAPHICS_WIDTH - CARD_DRAW_XOFF);
	if (xoff < SCREEN_WIDTH) {
		clear_rect(xoff, PEEK(0x9F21), SCREEN_WIDTH, HAND_Y_END);
	}
}

//#define SHOW_COMPUTER_HAND

void draw_hands() {
	unsigned char i;
	unsigned char selected_card_x_offset = get_x_offset(6);
	unsigned char hand_size;
	
	// Draw face down cards representing computer's hand
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F21, 0);
	POKE(0x9F22, 0x10);
	for (i = 0; computer_hand[i] != CARD_NP; ++i) {
		POKE(0x9F21, 0);
		draw_half_card_back();
		
		// You can peek if you're a cheater
		if (show_computer_hand) {
			POKE(0x9F20, PEEK(0x9F20) - 6);
			POKE(0x9F21, CARD_GRAPHICS_HEIGHT >> 1);
			POKE(0x9F23, get_card_char(computer_hand[i]));
			POKE(0x9F23, default_color);
			POKE(0x9F23, get_card_suite(computer_hand[i]));
			POKE(0x9F20, PEEK(0x9F20) + 3);
		}
	}
	if (i) { 
		POKE(0x9F20, PEEK(0x9F20) + ((CARD_GRAPHICS_WIDTH - CARD_DRAW_XOFF) << 1)); 
	}
	clear_rect(PEEK(0x9F20) >> 1, 0, SCREEN_WIDTH, (CARD_GRAPHICS_HEIGHT >> 1));
	
	// Now draw player's hand face up
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F21, HAND_Y_OFFSET);
	
	hand_size = get_pile_size(player_hand);
	for (i = 0; i + hand_display_offset < hand_size && 
		(PEEK(0x9F20) >> 1) + CARD_GRAPHICS_WIDTH < selected_card_x_offset; ++i) {
		draw_card(player_hand[i + hand_display_offset]);
	}
	if (i) { 
		POKE(0x9F20, PEEK(0x9F20) + ((CARD_GRAPHICS_WIDTH - CARD_DRAW_XOFF) << 1)); 
	}
	clear_rect(PEEK(0x9F20) >> 1, HAND_Y_OFFSET, get_x_offset(6), SCREEN_HEIGHT);

	POKE(0x9F21, HAND_Y_OFFSET + (CARD_GRAPHICS_HEIGHT >> 1));
	POKE(0x9F22, 0x20);

	POKE(0x9F20, 0);
	if (hand_display_offset != 0) {
		POKE(0x9F23, CH_LESS_THAN);
		POKE(0x9F23, CH_DASH);
	} else {
		POKE(0x9F23, 0x20);
		POKE(0x9F23, 0x20);
	}

	POKE(0x9F20, (get_x_offset(6) - 2) << 1);
	if (hand_display_offset < max_hand_display_offset(get_pile_size(player_hand))) {
		POKE(0x9F23, CH_DASH);
		POKE(0x9F23, CH_GREATER_THAN);
	} else {
		POKE(0x9F23, 0x20);
		POKE(0x9F23, 0x20);
	}
	
}

char draw_str[]		= "PLAY OR DRAW:       ";
char discard_str[]	= "PLAY OR DISCARD:    ";
char empty_str[]	= "WAITING FOR COMPUTER";

void draw_deck_pile() {
	unsigned char i;
	
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F21, PILE_Y_OFFSET);
	POKE(0x9F22, 0x10);
	
	if (deck[0] == CARD_NP) {
		draw_empty_card();
	} else {
		draw_card_back();
	}
	
	POKE(0x9F20, get_x_offset(1) << 1);
	
	if (discard[0] == CARD_NP) {
		if (picked_up_card == NULL) {
			draw_empty_card();
		}
	} else {
		for (i = 0; discard[i] != CARD_NP; ++i) {
			draw_card(discard[i]);
		}
	}
	if (picked_up_card != NULL) {
		draw_undo_card();
	}

	POKE(0x9F20, PEEK(0x9F20) + (CARD_GRAPHICS_WIDTH - CARD_DRAW_XOFF) * 2);
	clear_rect(PEEK(0x9F20) >> 1, PILE_Y_OFFSET, SCREEN_WIDTH, PILE_Y_END + 1);
	
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F21, PILE_Y_OFFSET - 1);
	POKE(0x9F22, 0x10);
	if (is_player_turn) {
		poke_str(already_drew_card ? discard_str : draw_str);
	} else {
		poke_str(empty_str);
	}
}

void draw_match(struct match *matchptr) {
	unsigned char i;
	unsigned char pile_size = get_pile_size(matchptr->cards);
	
	for (i = 0; i < pile_size; ++i) {
		draw_card(matchptr->cards[i]);
	}
}

void add_space_match() {
	unsigned char xoff, yoff;

	xoff = (PEEK(0x9F20) >> 1) + CARD_GRAPHICS_WIDTH - CARD_DRAW_XOFF;
	yoff = PEEK(0x9F21);
	clear_rect(xoff, yoff, xoff + 1, yoff + CARD_GRAPHICS_HEIGHT);
	POKE(0x9F20, (xoff + 1) << 1);
	POKE(0x9F21, yoff);
}

void check_space_left(struct match *matchptr) {
	unsigned char pile_size = get_pile_size(matchptr->cards);

	if (pile_size * CARD_DRAW_XOFF + (CARD_GRAPHICS_WIDTH - CARD_DRAW_XOFF) + (PEEK(0x9F20) >> 1) >= SCREEN_WIDTH) {
		unsigned char yoff = PEEK(0x9F21);

		clear_rect(PEEK(0x9F20) >> 1, yoff, SCREEN_WIDTH, yoff + CARD_GRAPHICS_HEIGHT);

		POKE(0x9F20, get_x_offset(0) << 1);
		POKE(0x9F21, 4 + yoff);
	}
}

void draw_cards_in_play() {
	unsigned char i;
	
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F21, COMP_PLAY_Y_OFFSET);
	POKE(0x9F22, 0x10);
	for (i = 0; i < computer_matches_size; ++i) {
		if (i) add_space_match();
		check_space_left(&computer_matches[i]);
		draw_match(&computer_matches[i]);
	}
	
	POKE(0x9F20, get_x_offset(0) << 1);
	POKE(0x9F21, PLAYER_PLAY_Y_OFFSET);
	draw_empty_card();
	
	POKE(0x9F20, get_x_offset(1) << 1);
	for (i = 0; i < player_matches_size; ++i) {
		if (i) add_space_match();
		check_space_left(&player_matches[i]);
		draw_match(&player_matches[i]);
	}
}

void display_piles() {
	//unsigned char vera_x_offset;
	//unsigned char pile_num, pile_ind;
	waitforjiffy();
	
	// draw cards in play
	draw_cards_in_play();
	
	// Draw deck and discard pile
	draw_deck_pile();
	
	// Draw player's hand
	draw_hands();
	draw_selected_card();
}


/*
	Menu codes
*/

void poke_str(char *str) {
	unsigned char c;
	while (*str) {
		c = *str;
		if (c >= 'A' && c <= 'Z') { c -= 0x40; }
		POKE(0x9F23, c);
		POKE(0x9F23, default_color);
		
		++str;
	}
}

unsigned char center_str_offset(unsigned char strlen) {
	return (40 - (strlen >> 1)) << 1;
}

void wait_click_space_pressed() {
	unsigned char last_key_pressed = 0x20;
	unsigned char last_mouse = 0xFF;

	unsigned char key_pressed;
	while (1) {
		key_pressed = cbm_k_getin();
		if (key_pressed == 'S') { show_computer_hand = 1; }
		
		if (last_key_pressed == 0x20) {
			if (key_pressed != 0x20) {
				last_key_pressed = key_pressed;
			}
		} else {
			if (key_pressed == 0x20) return;
		}

		mouse_get(&mouse_input);
		if ((last_mouse & 0x1) == 0 && (mouse_input.buttons & 0x1) == 1) return;
		last_mouse &= mouse_input.buttons;
	}

}

char press_space_continue_str[] = "-- CLICK OR PRESS SPACE TO CONTINUE --";

char control_strings[][70] = {
	"CONTROLS:"
	"",
	"LEFT CLICK TO SELECT CARDS",
	"RIGHT CLICK TO UNSELECT ALL CARDS",
	"",
	"CLICK THE DECK TO DRAW FROM THE DECK",
	"CLICK A CARD IN THE DISCARD PILE TO PICK IT UP AND ALL CARDS ABOVE IT",
	"ONCE A CARD IS DRAWN, CLICK THE DISCARD PILE WITH ONE CARD SELECTED",
	"TO DISCARD IT AND END YOUR TURN.",
	"TO PLAY A MATCH, SELECT ALL THE CARDS IN THE MATCH",
	"AND CLICK THE EMPTY CARD SHAPE BELOW THE DECK.",
	"IF YOUR PLAY WAS NOT A VALID MATCH, IT WILL PLAY A BEEP.",
};
#define CONTROL_STRINGS_LEN 11

void display_game_controls() {
	unsigned char i;

	display_rummy_bigtext(1);

	POKE(0x9F22, 0x10);
	POKE(0x9F21, 10);
	for (i = 0; i < CONTROL_STRINGS_LEN; ++i) {
		POKE(0x9F20, 1 << 1);
		poke_str(control_strings[i]);

		POKE(0x9F21, 2 + PEEK(0x9F21));
	}
	POKE(0x9F21, 4 + PEEK(0x9F21));
	POKE(0x9F20, 1 << 1);
	poke_str(press_space_continue_str);

	wait_click_space_pressed();
	clear_screen();
}

char rummy_logo[][40] = {
	{ 0xA0,0xA0,0xA0,0xA0,0x20,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0x20,0x20,0x20, 0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0, 0x00},
	{ 0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0xA0,0x20,0xA0, 0xA0,0x20,0xA0,0xA0,0x20,0xA0,0xA0,0x20,0x20,0xA0,0x20,0xA0,0x20, 0x00},
	{ 0xA0,0xA0,0xA0,0xA0,0x20,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0x20,0xA0,0x20, 0xA0,0x20,0xA0,0x20,0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0x20, 0x00},
	{ 0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0x20,0x20,0x20, 0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0x20, 0x00},
	{ 0xA0,0x20,0x20,0x20,0xA0,0x20,0xA0,0xA0,0xA0,0xA0,0xA0,0x20,0xA0,0x20,0x20,0x20, 0xA0,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0x20,0x20,0xA0,0x20,0x20, 0x00},
};

void display_rummy_bigtext(unsigned char x_offset) {
	unsigned char i;
	POKE(0x9F22, 0x10);
	POKE(0x9F21, 2);
	
	for (i = 0; i < 5; ++i) {
		POKE(0x9F20, x_offset << 1);
		poke_str(rummy_logo[i]);
		__asm__ ("inc $9F21");
	}
}

char game_rule_strs[][80] = {
	"THE OBJECTIVE OF THE GAME IS TO SCORE MORE POINTS THAN YOUR OPPONENT,",
	"ACHIEVED BY PLAYING 3 CARDS OF THE SAME RANK OR 3 OF THE SAME SUITE IN A ROW.",
	"ONCE EITHER PLAYER RUNS OUT OF CARDS,",
	"THEY ALSO ADD THE POINT VALUES OF THE OTHER PLAYER'S HAND TO THEIR SCORE.",
	"PLAYERS CAN PLAY OFF BOTH THEIR EXISTING MATCHES AND THOSE OF THEIR OPPONENT.",
	"",
	"AT THE START OF A PLAYER'S TURN, THEY MAY EITHER DRAW FROM THE DECK",
	"OR PICK UP A PORTION OF THE CARDS FROM THE DISCARD PILE.",
	"IF THEY DO SO, THEY MUST PICK UP ALL THE CARDS ABOVE THE FIRST THEY PICK UP,",
	"AND THEY MUST PLAY THAT CARD TO FORM A MATCH THIS TURN.",
	"(THE GAME ALSO CHECKS THAT IT IS THE FIRST MATCH YOU PLAY THAT TURN)",
	"AT ANY TIME THE PLAYER MAY PLAY ANY MATCHES FROM THEIR HAND. ",
	"ONCE THEY ARE DONE, THEY MUST DISCARD A CARD TO THE TOP OF THE DISCARD PILE.",
	"",
	"SCORING:",
	"2-10 ARE WORTH 5 POINTS",
	"J,Q,K (FACE CARDS) ARE WORTH 10",
	"ACES ARE WORTH 15 POINTS",
};
#define GAME_RULE_STRS_LEN 18

void display_game_rules() {
	unsigned char i;

	display_rummy_bigtext(1);

	POKE(0x9F22, 0x10);
	POKE(0x9F21, 10);

	for (i = 0; i < GAME_RULE_STRS_LEN; ++i) {
		POKE(0x9F20, 1 << 1);
		poke_str(game_rule_strs[i]);

		POKE(0x9F21, 2 + PEEK(0x9F21));
	}
	POKE(0x9F21, 4 + PEEK(0x9F21));
	POKE(0x9F20, 1 << 1);
	poke_str(press_space_continue_str);

	wait_click_space_pressed();
	clear_screen();
}

char comp_out_string[] = "COMPUTER WENT OUT FIRST";
#define COMP_OUT_STRLEN (sizeof(comp_out_string) - 1)

char player_out_string[] = "YOU WENT OUT FIRST! ";
#define PLAYER_OUT_STRLEN (sizeof(player_out_string) - 1)

char loser_string[] = "YOU LOSE";
#define LOSER_STRLEN (sizeof(loser_string) - 1)

char victory_string[] = "CONGRATS! YOU WIN ";
#define VICTORY_STRLEN (sizeof(victory_string) - 1)

char tie_string[] = "IT'S A TIE";
#define TIE_STRLEN (sizeof(tie_string) - 1)

char press_string[] = "PRESS SPACE TO PLAY AGAIN ";
#define PRESS_STRLEN (sizeof(press_string) - 1)

char player_score_text_str[] = "YOUR SCORE: ";
char comp_score_text_str[] = "COMPUTER'S SCORE: ";

char score_so_far_str[] = "YOU SO FAR: ";
char computer_text_str[] = "COMPUTER SO FAR: ";

#define SCORE_SO_FAR_XOFF 19
#define PLAYER_SCORE_XOFF 19
#define COMPUTER_SCORE_XOFF 44

char player_score_str[6];
char computer_score_str[6];

#define WIN_TOP_Y 25
#define WIN_WIN_Y 28
#define WIN_SCORES_Y 31
#define WIN_BOT_Y 34

unsigned short player_score_so_far = 0;
unsigned short computer_score_so_far = 0;
unsigned char is_first_game = 1;

void display_win() {
	unsigned short player_score, computer_score;
	unsigned char first_game_offset = is_first_game ? 0 : 3;

	clear_rect(0, WIN_TOP_Y - 1, SCREEN_WIDTH, WIN_BOT_Y + 2 + 3);

	player_score = calc_matches_score(player_matches, player_matches_size);
	computer_score = calc_matches_score(computer_matches, computer_matches_size);
	POKEW(0x10, computer_score);
	if (player_went_out_first) {
		player_score += calc_pile_score(computer_hand);
	} else {
		computer_score += calc_pile_score(player_hand);
	}

	player_score_so_far += player_score;
	computer_score_so_far += computer_score;

	itoa(player_score, player_score_str, 10);
	itoa(computer_score, computer_score_str, 10);

	POKE(0x9F22, 0x10);
	POKE(0x9F21, WIN_TOP_Y);
	if (player_went_out_first) {
		POKE(0x9F20, center_str_offset(PLAYER_OUT_STRLEN));
		poke_str(player_out_string);
	} else {
		POKE(0x9F20, center_str_offset(COMP_OUT_STRLEN));
		poke_str(comp_out_string);
	}

	POKE(0x9F21, WIN_WIN_Y);
	if (player_score > computer_score) {
		POKE(0x9F20, center_str_offset(VICTORY_STRLEN));
		poke_str(victory_string);
	} else if (player_score == computer_score) {
		POKE(0x9F20, center_str_offset(TIE_STRLEN));
		poke_str(tie_string);
	} else {
		POKE(0x9F20, center_str_offset(LOSER_STRLEN));
		poke_str(loser_string);
	}

	POKE(0x9F21, WIN_SCORES_Y);
	POKE(0x9F20, PLAYER_SCORE_XOFF << 1);
	poke_str(player_score_text_str);
	poke_str(player_score_str);

	POKE(0x9F20, COMPUTER_SCORE_XOFF << 1);
	poke_str(comp_score_text_str);
	poke_str(computer_score_str);

	if (!is_first_game) {
		POKE(0x9F21, WIN_SCORES_Y + first_game_offset);
		POKE(0x9F20, SCORE_SO_FAR_XOFF << 1);
		poke_str(score_so_far_str);

		itoa(player_score_so_far, player_score_str, 10);
		poke_str(player_score_str);
		
		POKE(0x9F20, COMPUTER_SCORE_XOFF << 1);
		itoa(computer_score_so_far, computer_score_str, 10);
		poke_str(computer_text_str);
		poke_str(computer_score_str);
	}

	POKE(0x9F21, WIN_BOT_Y + first_game_offset);
	POKE(0x9F20, center_str_offset(PRESS_STRLEN));
	poke_str(press_string);

	is_first_game = 0;

	while (cbm_k_getin() != 0x20);
}