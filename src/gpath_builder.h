#include <pebble.h>

typedef struct {
  uint32_t max_points;
  uint32_t next_point_index;
  GPoint points[];
} GPathBuilder;


GPathBuilder *gpath_builder_create(uint32_t max_points);
void gpath_builder_destroy(GPathBuilder *builder);

bool gpath_builder_move_to_point(GPathBuilder *builder, GPoint to_point);
bool gpath_builder_line_to_point(GPathBuilder *builder, GPoint to_point);
bool gpath_builder_curve_to_point(GPathBuilder *builder, GPoint to_point, GPoint control_point_1, GPoint control_point_2);

// TODO: make gpath reuse the builder.points (note: requires realloc and change in gpath_builder_create to use two separate mallocs)
//! removes last point if it's the same as the first
GPath *gpath_builder_create_path(GPathBuilder *builder);
