#include <pebble.h>

#define NUMBER_OF_POINTS 12
#define MAX_POINTS 256
#define DRAW_LINE false

#define ROTATION_STEP 5
#define ROTATION_MAX 360
#define ROTATION_MIN 0

static Window *window;
static Layer *layer;
static GPoint points[NUMBER_OF_POINTS]; //points with vertices
static GPoint points_helper[MAX_POINTS];
unsigned int current_point = 0;
static AppTimer *refresh_timer;
static int rotation_helper;

static GPoint offset;
static int32_t rotation;

static void app_timer_callback(void *data) {
  rotation_helper += ROTATION_STEP;
  if (rotation_helper >= ROTATION_MAX) {
    rotation_helper = ROTATION_MIN;
  }
  rotation = (TRIG_MAX_ANGLE/360) * rotation_helper;
  layer_mark_dirty(layer);
  
  refresh_timer = app_timer_register(40, app_timer_callback, NULL);
}

static GPoint _rotate_offset_point(GPoint *orig, int32_t rotation, GPoint *offset) {
  int32_t cosine = cos_lookup(rotation);
  int32_t sine = sin_lookup(rotation);
  GPoint result;
  result.x = (int32_t)orig->x * cosine / TRIG_MAX_RATIO - (int32_t)orig->y * sine / TRIG_MAX_RATIO + offset->x;
  result.y = (int32_t)orig->y * cosine / TRIG_MAX_RATIO + (int32_t)orig->x * sine / TRIG_MAX_RATIO + offset->y;
  return result;
}

int MAX (int a, int b){
  return (a > b)? a : b;
}

int MIN (int a, int b){
  return (a < b)? a : b;
}

static int integer_divide_round_closest(const int n, const int d) {
    return ((n < 0) ^ (d < 0)) ? ((n - d/2)/d) : ((n + d/2)/d);
}

static void swap16(int16_t *a, int16_t *b) {
    int16_t tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sort16(int16_t *values, size_t length) {
    for (unsigned int i = 0; i < length; i++) {
        for (unsigned int j = i+1; j < length; j++) {
            if (values[i] > values[j]) {
                swap16(&values[i], &values[j]);
            }
        }
    }
}

static void _gpath_draw_filled(GContext* ctx, unsigned int num_pts, GPoint pts[]) {
    if (num_pts < 2) {
        return;
    }

    GPoint* rot_points = malloc(num_pts * sizeof(GPoint));
    int min_x, max_x, min_y, max_y;
    GPoint rot_start, rot_end;
    bool found_start_direction = false;
    bool start_is_down = false;

    rot_points[0] = rot_end = _rotate_offset_point(&pts[0], rotation, &offset);
    min_x = max_x = rot_points[0].x;
    min_y = max_y = rot_points[0].y;

    // begin finding the last path segment's direction going backwards through the path
    // we must go backwards because we find intersections going forwards
    for (int i = num_pts - 1; i > 0; --i) {
        rot_points[i] = rot_start = _rotate_offset_point(&pts[i], rotation, &offset);
        if (min_x > rot_points[i].x) { min_x = rot_points[i].x; }
        if (max_x < rot_points[i].x) { max_x = rot_points[i].x; }
        if (min_y > rot_points[i].y) { min_y = rot_points[i].y; }
        if (max_y < rot_points[i].y) { max_y = rot_points[i].y; }

        if (found_start_direction) {
            continue;
        }

        // use the first non-horizontal path segment's direction as the start direction
        if (rot_end.y != rot_start.y) {
            start_is_down = rot_end.y > rot_start.y;
            found_start_direction = true;
        }

        rot_end = rot_start;
    }

    // x-intersections of path segments whose direction is up
    int16_t* intersections_up = malloc(num_pts * sizeof(int16_t));
    // x-intersections of path segments whose direction is down
    int16_t* intersections_down = malloc(num_pts * sizeof(int16_t));
    size_t intersection_up_count;
    size_t intersection_down_count;

    // find all of the horizontal intersections and draw them
    min_y = MAX(min_y, 0);
    max_y = MIN(max_y, 167);
    for (int i = min_y; i <= max_y; ++i) {
        // initialize with 0 intersections
        intersection_down_count = 0;
        intersection_up_count = 0;

        // horizontal path segments don't have a direction and depend upon the last path segment's direction
        // keep track of the last path direction for horizontal path segments to use
        bool last_is_down = start_is_down;
        rot_end = rot_points[0];

        // find the intersections
        for (unsigned int j = 0; j < num_pts; ++j) {
            rot_start = rot_points[j];
            if (j + 1 < num_pts) {
                rot_end = rot_points[j + 1];
            } else {
                // wrap to the first point
                rot_end = rot_points[0];
            }

            // if the line is on/crosses this height
            if ((rot_start.y - i) * (rot_end.y - i) <= 0) {
                bool is_down = rot_end.y != rot_start.y ? rot_end.y > rot_start.y : last_is_down;
                // don't count end points in the same direction to avoid double intersections
                if (!(rot_start.y == i && last_is_down == is_down)) {
                    // linear interpolation of the line intersection
                    int16_t x = (int16_t)(rot_start.x + integer_divide_round_closest((rot_end.x - rot_start.x) * (i - rot_start.y), (rot_end.y - rot_start.y)));
                    if (is_down) {
                        intersections_down[intersection_down_count] = x;
                        intersection_down_count++;
                    } else {
                        intersections_up[intersection_up_count] = x;
                        intersection_up_count++;
                    }
                }
                last_is_down = is_down;
            }
        }

        // sort the intersections
        sort16(intersections_up, intersection_up_count);
        sort16(intersections_down, intersection_down_count);

        // draw the line segments
        for (int j = 0; j < (int)MIN(intersection_up_count, intersection_down_count); j++) {
            int16_t x_a = intersections_up[j];
            int16_t x_b = intersections_down[j];
            if (x_a > x_b) {
                swap16(&x_a, &x_b);
            }
            graphics_fill_rect(ctx, GRect(x_a, (int16_t)i, (int16_t)(x_b - x_a + 1), 1), 0, GCornerNone);
        }
    }
    free(rot_points);
    free(intersections_up);
    free(intersections_down);
}

void clean_points(void) {
  current_point = 0;
}

void add_point(int x, int y)
{
  if (current_point+1 >= MAX_POINTS) {
    return;
  }
  points_helper[current_point] = GPoint(x, y);
  current_point++;
}

double m_distance_tolerance = 0.25;

const int fixedpoint_base = 16;
int m_distance_tolerance_fixed = 4;
int32_t m_angle_tolerance = (TRIG_MAX_ANGLE / 360) * 10;

void recursive_bezier_fixed(int x1, int y1, 
                            int x2, int y2, 
                            int x3, int y3, 
                            int x4, int y4){

  // Calculate all the mid-points of the line segments
  int x12   = (x1 + x2) / 2;
  int y12   = (y1 + y2) / 2;
  int x23   = (x2 + x3) / 2;
  int y23   = (y2 + y3) / 2;
  int x34   = (x3 + x4) / 2;
  int y34   = (y3 + y4) / 2;
  int x123  = (x12 + x23) / 2;
  int y123  = (y12 + y23) / 2;
  int x234  = (x23 + x34) / 2;
  int y234  = (y23 + y34) / 2;
  int x1234 = (x123 + x234) / 2;
  int y1234 = (y123 + y234) / 2;
/*
  //this algorithm is a bit more computational heavy, but interesting approach nonetheless
  //also tends to overflow an int when calculating a distance between points

  // Try to approximate the full cubic curve by a single straight line
  int dx = (x4-x1);
  int dy = (y4-y1);

  int d2 = abs(((x2-x4) * dy) / fixedpoint_base - ((y2 - y4) * dx) / fixedpoint_base);
  int d3 = abs(((x3-x4) * dy) / fixedpoint_base - ((y3 - y4) * dx) / fixedpoint_base);
  
  //first part should be divided after mutliplication but that often causes int to overflow and causes incorrect results
  if(((d2 + d3) * ((d2 + d3) / fixedpoint_base)) < (m_distance_tolerance_fixed * ((dx * dx) / fixedpoint_base + (dy * dy) / fixedpoint_base)) / fixedpoint_base)
  {
    add_point(x1234 / fixedpoint_base, y1234 / fixedpoint_base);
    return;
  }
*/

  // Angle Condition
  int32_t a23 = atan2_lookup((y3 - y2) / fixedpoint_base, (x3 - x2) / fixedpoint_base);
  int32_t da1 = abs(a23 - atan2_lookup((y2 - y1) / fixedpoint_base, (x2 - x1) / fixedpoint_base));
  int32_t da2 = abs(atan2_lookup((y4 - y3) / fixedpoint_base, (x4 - x3) / fixedpoint_base) - a23);
  if(da1 >= TRIG_MAX_ANGLE) da1 = TRIG_MAX_ANGLE - da1;
  if(da2 >= TRIG_MAX_ANGLE) da2 = TRIG_MAX_ANGLE - da2;

  if(da1 + da2 < m_angle_tolerance){
    // Finally we can stop the recursion
    add_point(x1234 / fixedpoint_base, y1234 / fixedpoint_base);
    return;
  }

  // Continue subdivision
  recursive_bezier_fixed(x1, y1, x12, y12, x123, y123, x1234, y1234); 
  recursive_bezier_fixed(x1234, y1234, x234, y234, x34, y34, x4, y4); 
}

void bezier_fixed(GPoint p1, GPoint p2, GPoint p3, GPoint p4){

  //translate points to fixedpoint realms
  int32_t x1 = p1.x * fixedpoint_base;
  int32_t x2 = p2.x * fixedpoint_base;
  int32_t x3 = p3.x * fixedpoint_base;
  int32_t x4 = p4.x * fixedpoint_base;
  int32_t y1 = p1.y * fixedpoint_base;
  int32_t y2 = p2.y * fixedpoint_base;
  int32_t y3 = p3.y * fixedpoint_base;
  int32_t y4 = p4.y * fixedpoint_base;

  recursive_bezier_fixed(x1, y1, x2, y2, x3, y3, x4, y4);
}

static void update_layer(struct Layer *layer, GContext *ctx) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "starting render");

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  
  time_t start = time(NULL);
  uint16_t start_ms = time_ms(NULL, NULL);
  
  //adaptive bezier base ( w/o doubles) calculations
  
  clean_points();
  
  add_point(points[0].x, points[0].y);
  for (int i=0; i<NUMBER_OF_POINTS; i+=3){
    if (i+3<NUMBER_OF_POINTS) {
      bezier_fixed(points[i], points[i+1], points[i+2], points[i+3]);
      add_point(points[i+3].x, points[i+3].y);
    } else {
      //we're at last curve
      bezier_fixed(points[i], points[i+1], points[i+2], points[0]);
      add_point(points[0].x, points[0].y);
    }
  }
  
  //drawing actual bezier curves
#if DRAW_LINE
  for (unsigned int i=0; i<current_point-1; i++){
    graphics_draw_line(ctx, points_helper[i], points_helper[i+1]);
  }
#else
  _gpath_draw_filled(ctx, current_point, points_helper);
#endif
  
  time_t end = time(NULL);
  uint16_t end_ms = time_ms(NULL, NULL);
  
  uint16_t total = (end - start) * 1000 + end_ms - start_ms;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "render took %d ms (%d points)", total, current_point);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  //text_layer_set_text(text_layer, "Select");
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
  
  points[0] = GPoint(-15, -15);   //point
  points[1] = GPoint(-15, -60);
  points[2] = GPoint(15, -60);
  points[3] = GPoint(15, -15);  //point
  points[4] = GPoint(60, -15);
  points[5] = GPoint(60, 15);
  points[6] = GPoint(15, 15);  //point
  points[7] = GPoint(15, 60);
  points[8] = GPoint(-15, 60);
  points[9] = GPoint(-15, 15);   //point
  points[10] = GPoint(-60, 15);
  points[11] = GPoint(-60, -15);
  
  offset = GPoint(bounds.size.w/2, bounds.size.h/2);
  rotation = 0;
  
  refresh_timer = app_timer_register(250, app_timer_callback, NULL);
}

static void window_unload(Window *window) {
  //nothing here yet
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
