#include <pebble.h>

#define ACTION_PAUSE 0
#define ACTION_PLAY 1
#define ACTION_STOP 2
#define ACTION_PREVIOUS 3
#define ACTION_NEXT 4
#define ACTION_VOL_UP 5
#define ACTION_VOL_DOWN 6
#define ACTION_GETINFO 255

#define STATE_PLAY 0
#define STATE_PAUSE 1
#define STATE_STOP 2

#define MENUSTATE_OUTER 0
#define MENUSTATE_INNER 1


static Window *s_window;
static TextLayer *s_author_layer;
static TextLayer *s_title_layer;
static ActionBarLayer *s_action_bar_layer;
char artist_buffer[21];
char title_buffer[31];

/*
static const GPathInfo PLAY_BUTTON = {
	.num_points = 3,
	.points = (GPoint []) {{0,0},{12,8},{0,16}}
};
static const GPathInfo FOR_BUTTON = {
	.num_points = 3,
	.points = (GPoint []) {{9,0},{0,8},{9,16}}
};
static const GPathInfo BACK_BUTTON = {
	.num_points = 3,
	.points = (GPoint []) {{0,0},{9,8},{0,16}}
};
*/

bool pause = true;
uint8_t state = STATE_PLAY;
uint8_t menu_state = MENUSTATE_OUTER;
time_t last_interact_time;

static bool s_js_ready = false;

static void update_interact_time(){
	last_interact_time = time(NULL);
}

static void sendAction(uint8_t action)  {
	if(!s_js_ready){
		return;
	}
	DictionaryIterator *iter;
	
	//dict_write_begin(&iter,buffer,sizeof(buffer));
	
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter,0,action);
	app_message_outbox_send();
	
}

static void update_action_buttons(void){
	if(menu_state == MENUSTATE_OUTER){
		if(state == STATE_PLAY){
			action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, gbitmap_create_with_resource(RESOURCE_ID_MORE_BUTTON), true);
		}else{
			action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, gbitmap_create_with_resource(RESOURCE_ID_PLAY_BUTTON), true);
		}
		action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_UP, gbitmap_create_with_resource(RESOURCE_ID_PREVIOUS_BUTTON), true);
		action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_DOWN, gbitmap_create_with_resource(RESOURCE_ID_NEXT_BUTTON), true);
	}else{
		action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, gbitmap_create_with_resource(RESOURCE_ID_PAUSE_BUTTON), true);
		action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_UP, gbitmap_create_with_resource(RESOURCE_ID_VOL_UP_BUTTON), true);
		action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_DOWN, gbitmap_create_with_resource(RESOURCE_ID_VOL_DOWN_BUTTON), true);
	}
}

static void hard_update(void){
	APP_LOG(APP_LOG_LEVEL_INFO, "Hard updating...");
	//layer_mark_dirty(s_side_layer);
	sendAction(ACTION_GETINFO);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed){
	time_t curtime = time(NULL);
	if(menu_state == MENUSTATE_INNER && difftime(curtime,last_interact_time) >= 2){
		menu_state = MENUSTATE_OUTER;
		update_action_buttons();
	}
	if(difftime(curtime,last_interact_time) >= 2*60){ // quit the app after 2 min
		window_stack_pop_all(false);
	}
}

static void inbox_recieved_callback(DictionaryIterator *iterator, void *context){
	APP_LOG(APP_LOG_LEVEL_INFO, "Recieved some message....");
	
	Tuple *tmp_tuple;
	// check for ready-setting
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_JSReady);
	APP_LOG(APP_LOG_LEVEL_INFO, "Ready tuple: %p", tmp_tuple);
	if(tmp_tuple){
		s_js_ready = true;
		APP_LOG(APP_LOG_LEVEL_INFO, "JS is ready!");
	}
	
	// check for changing state
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_state);
	APP_LOG(APP_LOG_LEVEL_INFO, "Recieved inbox! %p",tmp_tuple);
	if(tmp_tuple){
		state = tmp_tuple->value->int8;
		update_action_buttons(); // state changed, so this might need to change, too
		APP_LOG(APP_LOG_LEVEL_INFO, "State: %d",state);
	}
	
	// check for changing artist
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_artist);
	if(tmp_tuple){
		snprintf(artist_buffer, sizeof(artist_buffer), "%s", tmp_tuple->value->cstring);
		text_layer_set_text(s_author_layer, artist_buffer);
	}
	
	// chekc for changing title
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_title);
	if(tmp_tuple){
		snprintf(title_buffer, sizeof(title_buffer), "%s", tmp_tuple->value->cstring);
		text_layer_set_text(s_title_layer, title_buffer);
	}
	
	if(state != STATE_PLAY && menu_state != MENUSTATE_OUTER){
		menu_state = MENUSTATE_OUTER;
		update_action_buttons();
	}
}
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(state != STATE_PLAY){
		sendAction(ACTION_PLAY);
	}else if(menu_state == MENUSTATE_OUTER){
		menu_state = MENUSTATE_INNER;
		update_action_buttons();
	}else{
		sendAction(ACTION_PAUSE);
	}
}

static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *context){
	sendAction(ACTION_STOP);
	
	action_bar_layer_set_icon_animated(s_action_bar_layer, BUTTON_ID_SELECT, gbitmap_create_with_resource(RESOURCE_ID_STOP_BUTTON), true);
	
	static const uint32_t const segments[] = { 100 };
	VibePattern pat = {
		.durations = segments,
		.num_segments = ARRAY_LENGTH(segments),
	};
	vibes_enqueue_custom_pattern(pat);
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(menu_state == MENUSTATE_OUTER){
		sendAction(ACTION_PREVIOUS);
	}else{
		sendAction(ACTION_VOL_UP);
	}
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(menu_state == MENUSTATE_OUTER){
		sendAction(ACTION_NEXT);
	}else{
		sendAction(ACTION_VOL_DOWN);
	}
}

static void prv_click_config_provider(void *context) {
	window_long_click_subscribe(BUTTON_ID_SELECT, 0, prv_select_long_click_handler, NULL);
	window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
	
	window_raw_click_subscribe(BUTTON_ID_SELECT,update_interact_time,update_interact_time,NULL);
	window_raw_click_subscribe(BUTTON_ID_UP,update_interact_time,update_interact_time,NULL);
	window_raw_click_subscribe(BUTTON_ID_DOWN,update_interact_time,update_interact_time,NULL);
}

static void prv_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	//GRect bounds = layer_get_bounds(window_layer);
	
	

	window_set_background_color(window,GColorLightGray);

	s_action_bar_layer = action_bar_layer_create();
	action_bar_layer_set_click_config_provider(s_action_bar_layer, prv_click_config_provider);
	update_action_buttons();
	action_bar_layer_add_to_window(s_action_bar_layer,window);

	s_author_layer = text_layer_create(GRect(12,25,95,20));
	text_layer_set_background_color(s_author_layer,GColorLightGray);
	text_layer_set_text_color(s_author_layer,GColorBlack);
	text_layer_set_text(s_author_layer,"Loading...");
	text_layer_set_font(s_author_layer,fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_author_layer,GTextAlignmentLeft);

	layer_add_child(window_layer,text_layer_get_layer(s_author_layer));
	
	s_title_layer = text_layer_create(GRect(12,43,95,52));
	text_layer_set_background_color(s_title_layer,GColorLightGray);
	text_layer_set_text_color(s_title_layer,GColorBlack);
	text_layer_set_text(s_title_layer,"Loading...");
	text_layer_set_font(s_title_layer,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_title_layer,GTextAlignmentLeft);
	
	layer_add_child(window_layer,text_layer_get_layer(s_title_layer));
	
}

static void prv_window_unload(Window *window) {
	text_layer_destroy(s_author_layer);
	text_layer_destroy(s_title_layer);
	action_bar_layer_destroy(s_action_bar_layer);
}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void prv_init(void) {
	s_window = window_create();
	
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = prv_window_load,
		.unload = prv_window_unload,
	});
	
	tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
	
	app_message_register_inbox_received(inbox_recieved_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	
	update_interact_time();
	
	app_message_open(128,128);
	
	window_stack_push(s_window, true);
}

static void prv_deinit(void) {
	window_destroy(s_window);
	tick_timer_service_unsubscribe();
}

int main(void) {
	prv_init();
	APP_LOG(APP_LOG_LEVEL_INFO, "Inited app");
	app_event_loop();
	prv_deinit();
}
