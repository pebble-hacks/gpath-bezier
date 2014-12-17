#include <pebble.h>
#include "gpath_builder.h"

#define MAX_POINTS 256
#define DRAW_LINE false
#define BENCHMARK false
#define MAX_DEMO_PATHS 3

static const int rot_step = TRIG_MAX_ANGLE / 360 * 5;
static Window *window;
static Layer *layer;
static GPath *s_path;
static uint8_t path_switcher = 0;

static void prv_create_path(void);

static void app_timer_callback(void *data) {
  if (s_path) {
    s_path->rotation = (s_path->rotation + rot_step) % TRIG_MAX_ANGLE;
    layer_mark_dirty(layer);
  }
  
  app_timer_register(35, app_timer_callback, NULL);
}

static void update_layer(struct Layer *layer, GContext *ctx) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "starting render");

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  
  //drawing actual bezier curves
#if DRAW_LINE
  gpath_draw_outline(ctx, s_path);
#else
  gpath_draw_filled(ctx, s_path);
#endif
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Select");
  path_switcher = (path_switcher + 1) % MAX_DEMO_PATHS;
  prv_create_path();
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Select held");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Up");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Down");
}

static void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Up held");
}

static void up_long_released_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Up released");
}

static void down_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Down held");
}

static void down_long_released_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Down released");
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Back");
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  //window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
  window_long_click_subscribe(BUTTON_ID_UP, 250, up_long_click_handler, up_long_released_handler);
  window_long_click_subscribe(BUTTON_ID_DOWN, 250, down_long_click_handler, down_long_released_handler);
}

static void prv_create_path() {
  if (s_path) {
    gpath_destroy(s_path);
  }

#if BENCHMARK
  time_t start = time(NULL);
  uint16_t start_ms = time_ms(NULL, NULL);
#endif

  GPathBuilder *builder = gpath_builder_create(MAX_POINTS);
  
  switch (path_switcher) {
  case 0:
    gpath_builder_move_to_point(builder, GPoint(-15, -15));
    gpath_builder_curve_to_point(builder, GPoint(15, -15), GPoint(-15, -60), GPoint(15, -60));
    gpath_builder_curve_to_point(builder, GPoint(15, 15), GPoint(60, -15), GPoint(60, 15));
    gpath_builder_curve_to_point(builder, GPoint(-15, 15), GPoint(15, 60), GPoint(-15, 60));
    gpath_builder_curve_to_point(builder, GPoint(-15, -15), GPoint(-60, 15), GPoint(-60, -15));
    break;
  case 1:
    gpath_builder_move_to_point(builder, GPoint(-20, -50));
    gpath_builder_curve_to_point(builder, GPoint(20, -50), GPoint(-25, -60), GPoint(25, -60));
    gpath_builder_curve_to_point(builder, GPoint(20, 50), GPoint(0, 0), GPoint(0, 0));
    gpath_builder_curve_to_point(builder, GPoint(-20, 50), GPoint(25, 60), GPoint(-25, 60));
    gpath_builder_curve_to_point(builder, GPoint(-20, -50), GPoint(0, 0), GPoint(0, 0));
    break;
  case 2:
    gpath_builder_move_to_point(builder, GPoint(0, -60));
    gpath_builder_curve_to_point(builder, GPoint(60, 0), GPoint(35, -60), GPoint(60, -35));
    gpath_builder_curve_to_point(builder, GPoint(0, 60), GPoint(60, 35), GPoint(35, 60));
    gpath_builder_curve_to_point(builder, GPoint(0, 0), GPoint(-50, 60), GPoint(-50, 0));
    gpath_builder_curve_to_point(builder, GPoint(0, -60), GPoint(50, 0), GPoint(50, -60));
    break;
  default:
    APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid demo path id: %d", path_switcher);
  }


  s_path = gpath_builder_create_path(builder);
  gpath_builder_destroy(builder);

#if BENCHMARK
  time_t end = time(NULL);
  uint16_t end_ms = time_ms(NULL, NULL);
#endif
  
  GRect bounds = layer_get_bounds(window_get_root_layer(window));
  gpath_move_to(s_path, GPoint((int16_t)(bounds.size.w/2), (int16_t)(bounds.size.h/2)));

#if BENCHMARK
  int total = (end - start) * 1000 + end_ms - start_ms;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "building took %d ms (%d points)", total, (int)s_path->num_points);
#endif
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorBlack);
  GRect bounds = layer_get_bounds(window_layer);

  layer = layer_create(bounds);
  layer_set_update_proc(layer, update_layer);
  layer_add_child(window_layer, layer);
  
  /*
  srand(time(NULL));
  
  for (int i=0; i<NUMBER_OF_POINTS; i++) {
    points[i] = GPoint(rand() % bounds.size.w, rand() % (bounds.size.h - 30));
  }
  */
  
  prv_create_path();
  
  app_timer_register(250, app_timer_callback, NULL);
}

static void window_unload(Window *window) {
  gpath_destroy(s_path);
  layer_destroy(layer);
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
