#include <pebble.h>

#define ACTION_PAUSE 0
#define ACTION_PLAY 1
#define ACTION_STOP 2
#define ACTION_GETINFO 255

#define STATE_PLAY 0
#define STATE_PAUSE 1
#define STATE_STOP 2


static Window *s_window;
static TextLayer *s_text_layer;
static Layer *s_main_layer;
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

bool pause = true;
uint8_t state = STATE_PLAY;
uint8_t buffer[128];
static bool s_js_ready = false;

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

static void main_layer_update_proc(Layer *layer, GContext *ctx){
	graphics_context_set_antialiased(ctx, false);
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx,GRect(144 - 35,0,35,168),0,GCornersAll);
	
	graphics_context_set_fill_color(ctx, GColorWhite);
	GPath *path_ptr = gpath_create(&PLAY_BUTTON);
	gpath_move_to(path_ptr,GPoint(144-24,84-10));
	gpath_draw_filled(ctx,path_ptr);
	
	graphics_fill_rect(ctx,GRect(144 - 25,28,2,17),0,GCornersAll);
	path_ptr = gpath_create(&FOR_BUTTON);
	gpath_move_to(path_ptr,GPoint(144 - 21,28));
	gpath_draw_filled(ctx,path_ptr);

	graphics_fill_rect(ctx,GRect(144 - 14,168-28-16,2,17),0,GCornersAll);
	path_ptr = gpath_create(&BACK_BUTTON);
	gpath_move_to(path_ptr,GPoint(144 - 25,168-28-16));
	gpath_draw_filled(ctx,path_ptr);
}
static void hard_update(){
	APP_LOG(APP_LOG_LEVEL_INFO, "Hard updating...");
	layer_mark_dirty(s_main_layer);
	sendAction(ACTION_GETINFO);
}
static void tick_handler(struct tm *tick_time, TimeUnits units_changed){
	if(tick_time->tm_sec == 0){
		hard_update();
	}else{
		layer_mark_dirty(s_main_layer); // we want to re-render
	}
}
static void inbox_recieved_callback(DictionaryIterator *iterator, void *context){
	APP_LOG(APP_LOG_LEVEL_INFO, "Recieved some message....");
	Tuple *ready_tuple = dict_find(iterator, MESSAGE_KEY_JSReady);
	APP_LOG(APP_LOG_LEVEL_INFO, "Ready tuple: %p", ready_tuple);
	if(ready_tuple){
		s_js_ready = true;
		APP_LOG(APP_LOG_LEVEL_INFO, "JS is ready!");
		hard_update();
	}
	Tuple *state_tuple = dict_find(iterator, MESSAGE_KEY_state);
	APP_LOG(APP_LOG_LEVEL_INFO, "Recieved inbox! %p",state_tuple);
	if(state_tuple){
		state = state_tuple->value->int8;
		APP_LOG(APP_LOG_LEVEL_INFO, "State: %d",state);
	}
}
static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
	switch(state){
		case STATE_PLAY:
			sendAction(ACTION_PAUSE);
			state = STATE_PAUSE;
			break;
		case STATE_PAUSE:
		case STATE_STOP:
			sendAction(ACTION_PLAY);
			state = STATE_PLAY;
			break;
	}
}

static void prv_select_long_click_handler(ClickRecognizerRef recognizer, void *context){
	sendAction(ACTION_STOP);
	state = STATE_STOP;
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
	hard_update();
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
	text_layer_set_text(s_text_layer, "Down");
}

static void prv_click_config_provider(void *context) {
	window_long_click_subscribe(BUTTON_ID_SELECT, 0, NULL, prv_select_long_click_handler);
	window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
	window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
	window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	
	s_main_layer = layer_create(bounds);

	window_set_background_color(window,GColorLightGray);

	layer_set_update_proc(s_main_layer, main_layer_update_proc);
	layer_add_child(window_layer, s_main_layer);

	

	//
	//s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
	//text_layer_set_text(s_text_layer, "Press a button");
	//text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
	//layer_add_child(window_layer, text_layer_get_layer(s_text_layer));
}

static void prv_window_unload(Window *window) {
	layer_destroy(s_main_layer);
	text_layer_destroy(s_text_layer);
}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void prv_init(void) {
	s_window = window_create();
	window_set_click_config_provider(s_window, prv_click_config_provider);
	window_set_window_handlers(s_window, (WindowHandlers) {
		.load = prv_window_load,
		.unload = prv_window_unload,
	});
	
	tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
	
	app_message_register_inbox_received(inbox_recieved_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);

	app_message_open(sizeof(buffer),sizeof(buffer));
	
	window_stack_push(s_window, true);
}

static void prv_deinit(void) {
	window_destroy(s_window);
}

int main(void) {
	prv_init();
	APP_LOG(APP_LOG_LEVEL_INFO, "Inited app");
	app_event_loop();
	prv_deinit();
}
