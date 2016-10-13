#include <pebble.h>

#define ACTION_PAUSE 0
#define ACTION_PLAY 1
#define ACTION_STOP 2
#define ACTION_PREVIOUS 3
#define ACTION_NEXT 4
#define ACTION_VOL_UP 5
#define ACTION_VOL_DOWN 6
#define ACTION_GETINFO 255

#define STATE_LOADING -1
#define STATE_PLAY 0
#define STATE_PAUSE 1
#define STATE_STOP 2

#define MENUSTATE_OUTER 0
#define MENUSTATE_INNER 1


static Window *s_window;
static TextLayer *s_author_layer;
static TextLayer *s_title_layer;
static TextLayer *s_pos_layer;
static TextLayer *s_time_layer;
static TextLayer *s_curtime_layer;
static BitmapLayer *s_state_layer;
static Layer *s_progress_layer;
static ActionBarLayer *s_action_bar_layer;
char artist_buffer[21];
char title_buffer[31];
char time_buffer[10];
char pos_buffer[10];
char curtime_buffer[6];


bool pause = true;
int8_t state = STATE_LOADING;
uint8_t menu_state = MENUSTATE_OUTER;
time_t last_interact_time;

uint32_t song_time = 0;
uint32_t song_pos = 0;
uint16_t app_timeout = 0;

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

static void progress_layer_update_proc(Layer* layer, GContext* ctx){
	GRect bounds = layer_get_bounds(layer);
	int16_t progress_bar_width = song_pos * bounds.size.w / song_time;
	
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, bounds, 1, GCornersAll);
	
	graphics_context_set_fill_color(ctx, GColorRed);
	graphics_fill_rect(ctx, GRect(bounds.origin.x, bounds.origin.y, progress_bar_width, bounds.size.h), 1, GCornersAll);
}

static void get_time_string(char* buffer,uint32_t ts){
	uint8_t sec = ts % 60;
	uint16_t min = ts / 60;
	if(min >= 60){
		uint16_t hour = min / 60;
		min %= 60;
		snprintf(buffer, 10, "%u:%02u:%02u", hour, min, sec);
		return;
	}
	snprintf(buffer, 10, "%u:%02u", min, sec);
}

static void update_song_pos(void){
	get_time_string(pos_buffer,song_pos);
	text_layer_set_text(s_pos_layer, pos_buffer);
	layer_mark_dirty(s_progress_layer);
}

static void update_state_image(void){
	switch(state){
		case STATE_LOADING:
			bitmap_layer_set_bitmap(s_state_layer, gbitmap_create_with_resource(RESOURCE_ID_LOADING_STATE));
			break;
		case STATE_PLAY:
			bitmap_layer_set_bitmap(s_state_layer, gbitmap_create_with_resource(RESOURCE_ID_PLAY_STATE));
			break;
		case STATE_PAUSE:
			bitmap_layer_set_bitmap(s_state_layer, gbitmap_create_with_resource(RESOURCE_ID_PAUSE_STATE));
			break;
		case STATE_STOP:
			bitmap_layer_set_bitmap(s_state_layer, gbitmap_create_with_resource(RESOURCE_ID_STOP_STATE));
			break;
	}
}

static void update_curtime(tm* mTime){
	snprintf(curtime_buffer, sizeof(curtime_buffer), "%u:%02u", mTime->tm_hour, mTime->tm_min);
	text_layer_set_text(s_curtime_layer, curtime_buffer);
}

static void tick_handler(struct tm* mTime, TimeUnits units_changed){
	time_t curtime = time(NULL);
	
	if(state == STATE_PLAY){
		song_pos++;
		if(song_time && song_pos > song_time){
			sendAction(ACTION_GETINFO);
		}
		update_song_pos();
	}
	
	if(mTime->tm_sec == 0){
		update_curtime(mTime);
	}
	
	if(menu_state == MENUSTATE_INNER && difftime(curtime,last_interact_time) >= 2){
		menu_state = MENUSTATE_OUTER;
		update_action_buttons();
		update_state_image();
	}
	if(app_timeout && difftime(curtime,last_interact_time) >= app_timeout){ // quit the app after configured time
		window_stack_pop_all(false);
	}
}

static void inbox_recieved_callback(DictionaryIterator *iterator, void *context){
	APP_LOG(APP_LOG_LEVEL_INFO, "Recieved some message....");
	
	Tuple *tmp_tuple;
	// check for ready-setting
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_JSReady);
	if(tmp_tuple){
		s_js_ready = true;
		app_timeout = tmp_tuple->value->int16;
		APP_LOG(APP_LOG_LEVEL_INFO, "JS is ready! App Timeout: %u", app_timeout);
	}
	
	// check for changing state
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_state);
	if(tmp_tuple){
		state = tmp_tuple->value->int8;
		if(state != STATE_PLAY && menu_state != MENUSTATE_OUTER){
			menu_state = MENUSTATE_OUTER;
		}
		if(menu_state == MENUSTATE_OUTER){
			update_state_image();
		}
		update_action_buttons();
	}
	
	// check for changing artist
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_artist);
	if(tmp_tuple){
		snprintf(artist_buffer, sizeof(artist_buffer), "%s", tmp_tuple->value->cstring);
		text_layer_set_text(s_author_layer, artist_buffer);
	}
	
	// check for changing title
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_title);
	if(tmp_tuple){
		snprintf(title_buffer, sizeof(title_buffer), "%s", tmp_tuple->value->cstring);
		text_layer_set_text(s_title_layer, title_buffer);
	}
	
	// check for changing time
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_time);
	if(tmp_tuple){
		song_time = tmp_tuple->value->int32;
		get_time_string(time_buffer, song_time);
		text_layer_set_text(s_time_layer, time_buffer);
	}
	
	// check for changing pos
	tmp_tuple = dict_find(iterator, MESSAGE_KEY_pos);
	if(tmp_tuple){
		song_pos = tmp_tuple->value->int32;
		update_song_pos();
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
	if(state != STATE_PLAY && state != STATE_PAUSE){
		return;
	}
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
		bitmap_layer_set_bitmap(s_state_layer, gbitmap_create_with_resource(RESOURCE_ID_VOL_UP_STATE));
		sendAction(ACTION_VOL_UP);
	}
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
	if(menu_state == MENUSTATE_OUTER){
		sendAction(ACTION_NEXT);
	}else{
		bitmap_layer_set_bitmap(s_state_layer, gbitmap_create_with_resource(RESOURCE_ID_VOL_DOWN_STATE));
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
	
	time_t time_value = time(NULL);
	struct tm* mTime = localtime(&time_value);

	window_set_background_color(window,GColorLightGray);

	s_action_bar_layer = action_bar_layer_create();
	action_bar_layer_set_click_config_provider(s_action_bar_layer, prv_click_config_provider);
	update_action_buttons();
	action_bar_layer_add_to_window(s_action_bar_layer,window);
	
	s_curtime_layer = text_layer_create(GRect(0,0,PBL_IF_RECT_ELSE(114,180),14));
	text_layer_set_background_color(s_curtime_layer,GColorLightGray);
	text_layer_set_text_color(s_curtime_layer,GColorBlack);
	update_curtime(mTime);
	text_layer_set_font(s_curtime_layer,fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_curtime_layer,GTextAlignmentCenter);
	layer_add_child(window_layer,text_layer_get_layer(s_curtime_layer));
	
	s_title_layer = text_layer_create(GRect(PBL_IF_RECT_ELSE(12,30),43,95,52));
	text_layer_set_background_color(s_title_layer,GColorLightGray);
	text_layer_set_text_color(s_title_layer,GColorBlack);
	text_layer_set_text(s_title_layer,"Loading...");
	text_layer_set_font(s_title_layer,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_title_layer,GTextAlignmentLeft);
	layer_add_child(window_layer,text_layer_get_layer(s_title_layer));
	
	s_author_layer = text_layer_create(GRect(PBL_IF_RECT_ELSE(12,30),25,95,22));
	text_layer_set_background_color(s_author_layer,GColorLightGray);
	text_layer_set_text_color(s_author_layer,GColorBlack);
	text_layer_set_text(s_author_layer,"");
	text_layer_set_font(s_author_layer,fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_author_layer,GTextAlignmentLeft);
	layer_add_child(window_layer,text_layer_get_layer(s_author_layer));
	
	s_pos_layer = text_layer_create(GRect(PBL_IF_RECT_ELSE(12,30),102,49,14));
	text_layer_set_background_color(s_pos_layer,GColorLightGray);
	text_layer_set_text_color(s_pos_layer,GColorBlack);
	text_layer_set_text(s_pos_layer,"");
	text_layer_set_font(s_pos_layer,fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_pos_layer,GTextAlignmentLeft);
	layer_add_child(window_layer,text_layer_get_layer(s_pos_layer));
	
	s_time_layer = text_layer_create(GRect(PBL_IF_RECT_ELSE(53,71),102,49,14));
	text_layer_set_background_color(s_time_layer,GColorLightGray);
	text_layer_set_text_color(s_time_layer,GColorBlack);
	text_layer_set_text(s_time_layer,"");
	text_layer_set_font(s_time_layer,fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_time_layer,GTextAlignmentRight);
	layer_add_child(window_layer,text_layer_get_layer(s_time_layer));
	
	s_progress_layer = layer_create(GRect(PBL_IF_RECT_ELSE(12,30),120,90,4));
	layer_set_update_proc(s_progress_layer, progress_layer_update_proc);
	layer_add_child(window_layer,s_progress_layer);
	
	s_state_layer = bitmap_layer_create(GRect(PBL_IF_RECT_ELSE(12,30),131,42,24));
	bitmap_layer_set_alignment(s_state_layer,GAlignTopLeft);
	bitmap_layer_set_background_color(s_state_layer,GColorLightGray);
	bitmap_layer_set_compositing_mode(s_state_layer,GCompOpSet);
	update_state_image();
	layer_add_child(window_layer,bitmap_layer_get_layer(s_state_layer));
}

static void prv_window_unload(Window *window) {
	layer_destroy(s_progress_layer);
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_pos_layer);
	text_layer_destroy(s_author_layer);
	text_layer_destroy(s_title_layer);
	text_layer_destroy(s_curtime_layer);
	bitmap_layer_destroy(s_state_layer);
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
