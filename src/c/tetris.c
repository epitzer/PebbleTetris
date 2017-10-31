#include <pebble.h>
#include "shape.h"

#define MAX_N_SHAPES 64
#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))

static Window *window;
static TextLayer *text_layer;
static Layer *canvas_layer;

static Shape shapes[MAX_N_SHAPES];
static Shape *current_shape = NULL;
static uint16_t n_shapes = 0;

static const uint32_t tick_ms = 600;
static AppTimer *timer = NULL;

char score_text[128];
static uint64_t score = 0;
static bool game_is_over = false;

static void timer_tick(void *data);

static void start() {
  n_shapes = 0;
  game_is_over = false;
  score = 0;
  timer = app_timer_register(tick_ms, timer_tick, NULL);
  layer_mark_dirty(canvas_layer);
  snprintf(score_text, sizeof(score_text), "%11llu", score);
  text_layer_set_text(text_layer, score_text);
}

static void game_over() {
  game_is_over = true;
  if (current_shape != NULL) {
    free(current_shape);
    current_shape = NULL;
    layer_mark_dirty(canvas_layer);
  }
  snprintf(score_text, sizeof(score_text), "%11llu\nGAME OVER", score);
  text_layer_set_text(text_layer, score_text);
}

bool is_game_over() {
  for (int16_t i = 0; i<n_shapes; i++) {
    if (current_shape != NULL && are_shapes_overlapping(current_shape, &shapes[i]))
      return true;
    for (int j = 0; j<i; j++) {
      if (are_shapes_overlapping(&shapes[i], &shapes[j]))
        return true;
    }
  }
  return false;
}

static bool try_move_and_scale_shape(Shape *s, GRect new_shape_bounds) {
  GRect bounds = layer_get_bounds(canvas_layer);
  Shape moved_shape = *s;
  
  if (!shape_bounded_move_and_scale(&moved_shape, new_shape_bounds, bounds))
    return false;
  
  for (uint8_t i = 0; i<n_shapes; i++) {
    if (s != &shapes[i]) {
      if (are_shapes_overlapping(&moved_shape, &shapes[i]))
        return false;
    }
  }
  
  *s = moved_shape;
  return true;
}

static bool try_move_shape(Shape *s, int16_t dx, int16_t dy) {
  return try_move_and_scale_shape(s, GRect(s->x + dx, s->y + dy, s->w, s->h));
}

static bool try_sink_shape(Shape *s) {
  return try_move_and_scale_shape(s, GRect(s->x + 10, s->y, s->w, s->h));
}

static bool try_flip_shape(Shape *s) {
  return try_move_and_scale_shape(s, GRect(s->x + (s->w/20)*10, s->y + (s->h/20)*10, s->h, s->w));
}

static int compare_shape_by_x_rev(const void *ap, const void *bp) {
  Shape *a = (Shape*)ap;
  Shape *b = (Shape*)bp;
  return b->x - a->x;
}

static void sink_all_shapes() {
  bool something_moved;
  qsort(shapes, n_shapes, sizeof(Shape), compare_shape_by_x_rev);
  do {
    something_moved = false;
    for (uint16_t i = 0; i<n_shapes; i++) {
      if (try_sink_shape(&shapes[i])) {
        something_moved = true;
      }
    }
  } while (something_moved);
}

static uint8_t remove_full_columns(uint8_t n_rows, uint8_t n_cols) {
  uint8_t n_full_columns = 0;
  uint16_t *height_sums = (uint16_t*) calloc(sizeof(uint16_t), n_cols);
  for (uint16_t i = 0; i<n_shapes; i++) {
    for (uint16_t j = 0; j<shapes[i].w/10; j++) {
      height_sums[shapes[i].x/10 + j] += shapes[i].h/10;
    }
  }
  for (uint16_t i = 0; i<n_cols; i++) {
    if (height_sums[i] >= n_rows)
      n_full_columns++;
  }
  if (n_full_columns > 0) {
    Shape new_shapes[MAX_N_SHAPES];
    uint16_t n_new_shapes = 0;
    for (uint16_t i = 0; i<n_shapes; i++) {
      uint16_t start_x = shapes[i].x/10;
      uint16_t len = shapes[i].w/10;
      uint16_t end_x = shapes[i].x/10 + len;
      for (uint16_t x = start_x; x < end_x; x++) {
        if (height_sums[x] >= n_rows) {
          if (x - start_x > 0) {
            new_shapes[n_new_shapes] = shapes[i];
            new_shapes[n_new_shapes].x = start_x * 10 + 2;
            new_shapes[n_new_shapes].w = (x-start_x) * 10;
            n_new_shapes++;
          }
          start_x = x + 1;
        }
      }
      if (start_x < end_x) {
        new_shapes[n_new_shapes] = shapes[i];
        new_shapes[n_new_shapes].x = start_x * 10 + 2;
        new_shapes[n_new_shapes].w = (end_x - start_x) * 10;
        n_new_shapes++;
      }
    }
    memcpy(shapes, new_shapes, sizeof(Shape) * n_new_shapes);
    n_shapes = n_new_shapes;
    sink_all_shapes();
  }
  free(height_sums);
  return n_full_columns;
}

static void timer_tick(void *data) {
  if (game_is_over || is_game_over()) {
    game_over();
    return;
  }
  
  timer = app_timer_register(tick_ms, timer_tick, NULL);
  
  GRect bounds = layer_get_bounds(canvas_layer);
  uint64_t new_score = 0;
  if (current_shape != NULL) {
    if(!try_sink_shape(current_shape)) {
      shapes[n_shapes] = *current_shape; 
      n_shapes++;
      free(current_shape);
      current_shape = NULL;
      new_score = 1 + 4 * remove_full_columns(bounds.size.h/10, bounds.size.w/10);
    }
  } else if (n_shapes < MAX_N_SHAPES) {
    current_shape = create_random_shape(bounds);
  } else {
    game_over();
  }
  layer_mark_dirty(canvas_layer);
  
  if (new_score > 0) {
    snprintf(score_text, sizeof(score_text), "%11llu + %11llu", score, new_score);
  } else {
    snprintf(score_text, sizeof(score_text), "%11llu", score);
  }
  text_layer_set_text(text_layer, score_text);
  score += new_score;
}

static void canvas_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  graphics_context_set_fill_color(ctx, GColorBrass);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx,
                    bounds.origin,
                    GPoint(bounds.origin.x, bounds.origin.y + bounds.size.h));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_line(ctx,
                     GPoint(bounds.origin.x + bounds.size.w - 1, bounds.origin.y),
                     GPoint(bounds.origin.x + bounds.size.w - 1, bounds.origin.y + bounds.size.h));
  
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  for (uint16_t i = 1; i<14; i++) {
    graphics_draw_line(ctx,
                      GPoint(bounds.origin.x + i*10, bounds.origin.y + 2),
                      GPoint(bounds.origin.x + i*10, bounds.origin.y + bounds.size.h));
    graphics_draw_line(ctx,
                      GPoint(bounds.origin.x + 2,               bounds.origin.y + i*10),
                      GPoint(bounds.origin.x + bounds.size.w-2, bounds.origin.y + i*10));
  }
  for (uint8_t i = 0; i < n_shapes; i++) {
     draw_shape(&shapes[i], layer, ctx);
  }
  
  if (current_shape != NULL) {
    draw_shape(current_shape, layer, ctx);
  }
                     
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (game_is_over) {
    start();
  } else  if (current_shape != NULL && try_move_shape(current_shape, 10, 0))
    layer_mark_dirty(canvas_layer);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (current_shape != NULL && try_move_shape(current_shape, 0, -10))
    layer_mark_dirty(canvas_layer);
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (current_shape != NULL && try_move_shape(current_shape, 0, 10))
    layer_mark_dirty(canvas_layer);
}

static void back_click_handler(ClickRecognizerRef recognizer, void* context) {
  if (current_shape != NULL && try_flip_shape(current_shape))
    layer_mark_dirty(canvas_layer);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

static void save() {
  uint16_t shapes_per_chunk = PERSIST_DATA_MAX_LENGTH / sizeof(Shape);
  uint16_t n_chunks = (n_shapes-1) / shapes_per_chunk + 1;
  persist_write_int(1, n_shapes);
  for (uint16_t i = 0; i<n_chunks; i++) {
    uint16_t n = MIN(shapes_per_chunk, n_shapes - i * shapes_per_chunk);
    persist_write_data(100 + i, &shapes[i*shapes_per_chunk], sizeof(Shape) * n);
  }
  if (current_shape != NULL) {
    persist_write_data(2, current_shape, sizeof(Shape));
  }
  persist_write_int(3, score);
  persist_write_bool(4, game_is_over);
}

static void load() {
  if (persist_exists(1)) {
    n_shapes = persist_read_int(1);
    uint16_t shapes_per_chunk = PERSIST_DATA_MAX_LENGTH / sizeof(Shape);
    uint16_t n_chunks = (n_shapes-1) / shapes_per_chunk + 1;
    for (uint16_t i = 0; i<n_chunks; i++) {
      uint16_t n = MIN(shapes_per_chunk, n_shapes - i * shapes_per_chunk);
      persist_read_data(100 + i, &shapes[i * shapes_per_chunk], sizeof(Shape) * n);
    }
  }
  if (persist_exists(2)) {
    if (current_shape == NULL)
      current_shape = (Shape*) malloc(sizeof(Shape));
    persist_read_data(2, current_shape, sizeof(Shape));
  }
  if (persist_exists(3)) score = persist_read_int(3);
  if (persist_exists(4)) game_is_over = persist_read_bool(4);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  canvas_layer = layer_create(GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, 140));
  layer_set_update_proc(canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, canvas_layer);

  text_layer = text_layer_create(GRect(bounds.origin.x, 140, bounds.size.w, bounds.size.h-140));
  text_layer_set_text(text_layer, "HAVE FUN!");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  
  current_shape = create_random_shape(bounds);
  
  load();
  timer = app_timer_register(tick_ms, timer_tick, NULL);
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  if (current_shape != NULL) {
    free(current_shape);
    current_shape = NULL;
  }
  
  if (timer != NULL) {
    app_timer_cancel(timer);
    timer = NULL;
  }
  save();
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
  light_enable(true);
}

static void deinit(void) {
  window_destroy(window);
  light_enable(false);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}