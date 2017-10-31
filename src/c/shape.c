#include <pebble.h>
#include "shape.h"

#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))

Shape* create_random_shape(GRect bounds) {
  const GColor fill_colors[] = {GColorRed, GColorGreen, GColorBlue};
  const GColor stroke_colors[] = {GColorDarkCandyAppleRed, GColorDarkGreen, GColorDukeBlue};

  const int16_t n = bounds.size.h/10;
  const int16_t y = rand() % (n-2);
  const int16_t min_h = 2;
  const int16_t max_h = MIN(6, n-y) - min_h;
  const int16_t h = rand() % max_h + min_h;

  Shape *shape = (Shape*) malloc(sizeof(Shape));
  *shape = (Shape) {
    .x = 2,
    .y = 10 * y,
    .w = 10,
    .h = 10 * h,
    .fill = fill_colors[rand() % ARRAY_LENGTH(fill_colors)],
    .stroke = stroke_colors[rand() & ARRAY_LENGTH(stroke_colors)]
  };
  return shape;
}

void draw_shape(Shape *shape, Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, shape->stroke);
  graphics_context_set_fill_color(ctx, shape->fill);
  graphics_fill_rect(ctx, GRect(shape->x, shape->y, shape->w-1, shape->h-1), 0, GCornerNone);
  graphics_draw_rect(ctx, GRect(shape->x, shape->y, shape->w-1, shape->h-1));
}

bool is_point_inside_shape(int16_t x, int16_t y, Shape *s) {
  return
    (x >= s->x) && (x <= (s->x + s->w-1)) &&
    (y >= s->y) && (y <= (s->y + s->h-1));
}

bool is_shape_inside_shape(Shape *a, Shape *b) {
  return
    is_point_inside_shape(a->x,          a->y,          b) ||
    is_point_inside_shape(a->x + a->w-1, a->y,          b) ||
    is_point_inside_shape(a->x,          a->y + a->h-1, b) ||
    is_point_inside_shape(a->x + a->w-1, a->y + a->h-1, b);
}

bool are_shapes_overlapping(Shape *a, Shape *b) {
  return is_shape_inside_shape(a, b) || is_shape_inside_shape(b, a);
}

bool shape_bounded_move_and_scale(Shape *s, GRect new_shape_bounds, GRect bounds) {
  s->x = new_shape_bounds.origin.x;
  s->y = new_shape_bounds.origin.y;
  s->w = new_shape_bounds.size.w;
  s->h = new_shape_bounds.size.h;
  
  if (s->x + s->w > bounds.size.w ||
      s->y + s->h > bounds.size.h ||
      s->x < bounds.origin.x ||
      s->y < bounds.origin.y)
    return false;
  return true;
}