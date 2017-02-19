#ifndef _MSX_H_PHYS
#define _MSX_H_PHYS

#define COLLISION_LEFT 	1
#define COLLISION_RIGHT	2
#define COLLISION_UP		4
#define COLLISION_DOWN		8
#define COLLISION_UP_LEFT	16
#define COLLISION_UP_RIGHT	32
#define COLLISION_DOWN_LEFT	64
#define COLLISION_DOWN_RIGHT	128

#define is_colliding_left(x)	(((x)->collision_state & COLLISION_LEFT) != 0)
#define is_colliding_right(x)	(((x)->collision_state & COLLISION_RIGHT) != 0)
#define is_colliding_down(x)	(((x)->collision_state & COLLISION_DOWN) != 0)

void phys_init();
void phys_set_colliding_tile(uint8_t tile);
void phys_detect_tile_collisions(struct display_object *, uint8_t *map);

#endif