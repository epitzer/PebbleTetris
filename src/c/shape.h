#pragma once

typedef struct Shape {
  int16_t x, y, w, h;
  GColor fill, stroke;
} Shape;

Shape* create_random_shape(GRect bounds);
void draw_shape(Shape *shape, Layer *layer, GContext *ctx);
bool is_point_inside_shape(int16_t x, int16_t y, Shape *s);
bool is_shape_inside_shape(Shape *a, Shape *b);
bool are_shapes_overlapping(Shape *a, Shape *b);
bool shape_bounded_move_and_scale(Shape *s, GRect new_shape_bounds, GRect bounds);