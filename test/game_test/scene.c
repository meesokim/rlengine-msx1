#include "msx.h"
#include "sys.h"
#include "vdp.h"
#include "sprite.h"
#include "wq.h"
#include "tile.h"
#include "map.h"
#include "log.h"
#include "dpo.h"
#include "phys.h"
#include "list.h"
#include "pt3.h"

#include "anim.h"
#include "logic.h"
#include "scene.h"
#include "banks.h"

#include "gen/game_test_tiles_ext.h"
#include "gen/game_test_sprites_ext.h"
#include "gen/map_defs.h"

struct tile_set tileset_map1;
struct tile_set tileset_map2;
struct tile_set tileset_map3;
struct tile_set tileset_map3b;
struct tile_set tileset_map4;
struct tile_set tileset_map5;
struct tile_set tileset_map6;
struct tile_set tileset_map7;
struct tile_set tileset[TILE_MAX];
struct tile_object tileobject[31];
struct spr_sprite_def enemy_sprites[31];
struct spr_sprite_def jean_sprite;
struct displ_object display_object[32];
struct displ_object dpo_arrow;
struct displ_object dpo_bullet[2];
struct displ_object dpo_jean;
struct list_head display_list;
struct map_object_item *map_object;

uint8_t spr_ct, tob_ct;
uint8_t *room_objs;

extern unsigned char *map_map_segment_dict[25];
extern unsigned char *map_map_segment[25];
extern struct list_head *elem;
extern struct displ_object *dpo;
extern unsigned char *map_object_layer[25];
extern void sys_set_ascii_page3(char page);
extern const unsigned char huntloop_song_pt3[];
extern const unsigned char church_song_pt3[];
extern const unsigned char prayerofhope_song_pt3[];

void play_room_music()
{
	pt3_decode();
	pt3_play();
}

void stop_music()
{
	sys_irq_unregister(play_room_music);
	pt3_mute();
}

void start_music(uint8_t room)
{
	sys_set_ascii_page3(PAGE_MUSIC);

	switch (room) {
		case ROOM_FOREST:
		case ROOM_GRAVEYARD:
			pt3_init(huntloop_song_pt3, 1); // loop
			sys_irq_register(play_room_music);
			break;
		case ROOM_CHURCH_ENTRANCE:
		case ROOM_CHURCH:
		case ROOM_CHURCH_TOWER:
		case ROOM_CHURCH_UPPER_FLOOR:
			pt3_init(church_song_pt3, 1);
			sys_irq_register(play_room_music);
			break;
		case ROOM_MOON_SIGHT:
			pt3_init(prayerofhope_song_pt3, 1);
			sys_irq_register(play_room_music);
			break;
		default:
	}
}

static void add_tileobject(struct displ_object *dpo, uint8_t objidx, enum tile_sets_t tileidx)
{
	bool success;

	sys_set_ascii_page3(PAGE_DYNTILES);

	success = tile_set_valloc(&tileset[tileidx]);
	if (!success) {
		log_e("could not allocate tileobject\n");
		return;
	}
	sys_set_ascii_page3(PAGE_MAPOBJECTS);

	tileobject[objidx].x = map_object->x;
	tileobject[objidx].y = map_object->y;
	tileobject[objidx].cur_dir = 1;
	tileobject[objidx].cur_anim_step = 0;
	tileobject[objidx].ts = &tileset[tileidx];
	tileobject[objidx].idx = 0;

	dpo->type = DISP_OBJECT_TILE;
	dpo->tob = &tileobject[objidx];
	dpo->xpos = map_object->x;
	dpo->ypos = map_object->y;
	dpo->state = 0;

	INIT_LIST_HEAD(&dpo->list);
	list_add(&dpo->list, &display_list);
	INIT_LIST_HEAD(&dpo->animator_list);
	tob_ct++;
}

void remove_tileobject(struct displ_object *dpo)
{
	list_del(&dpo->list);
	tile_object_hide(dpo->tob, scr_tile_buffer, true);
}

void update_tileobject(struct displ_object *dpo)
{
	tile_object_show(dpo->tob, scr_tile_buffer, true);
}

static void add_sprite(struct displ_object *dpo, uint8_t objidx, enum spr_patterns_t pattidx)
{
	sys_set_ascii_page3(PAGE_SPRITES);

	spr_valloc_pattern_set(pattidx);
	spr_init_sprite(&enemy_sprites[objidx], pattidx);
	INIT_LIST_HEAD(&dpo->animator_list);

	sys_set_ascii_page3(PAGE_MAPOBJECTS);

	spr_set_pos(&enemy_sprites[objidx], map_object->x, map_object->y);
	dpo->type = DISP_OBJECT_SPRITE;
	dpo->spr = &enemy_sprites[objidx];
	dpo->xpos = map_object->x;
	dpo->ypos = map_object->y;
	dpo->state = 0;
	dpo->collision_state = 0;
	INIT_LIST_HEAD(&dpo->list);
	list_add(&dpo->list, &display_list);
	INIT_LIST_HEAD(&dpo->animator_list);
	spr_ct++;
}

void add_jean()
{
	sys_set_ascii_page3(PAGE_SPRITES);
	spr_valloc_pattern_set(PATRN_JEAN);

	spr_init_sprite(&jean_sprite, PATRN_JEAN);
	INIT_LIST_HEAD(&dpo_jean.animator_list);

	// FIXME: initial position will depend on how jean enters the room
	dpo_jean.xpos = 100;
	dpo_jean.ypos = 192 - 64;
	dpo_jean.type = DISP_OBJECT_SPRITE;
	dpo_jean.state = 0;
	dpo_jean.spr = &jean_sprite;
	dpo_jean.collision_state = 0;
	spr_set_pos(&jean_sprite, dpo_jean.xpos, dpo_jean.ypos);
	INIT_LIST_HEAD(&dpo_jean.list);
	list_add(&dpo_jean.list, &display_list);
	INIT_LIST_HEAD(&dpo_jean.animator_list);
	add_animator(&dpo_jean, ANIM_JEAN);
}

void jean_collision_handler()
{
	dpo_jean.state = STATE_COLLISION;
}


void clear_room() {
	uint8_t i;

	/* clear all sprite attributes */
	spr_clear();

	/* free dynamic tiles */
	for (i = 0; i < tob_ct; i++) {
		tile_set_vfree(tileobject[i].ts);
	}

	spr_ct = 0;
	tob_ct = 0;
}

/**
 * clean room ephemeral state
 */
void clean_state()
{
	game_state.templar_delay = 0;
	game_state.templar_ct = 0;
}

void load_room(uint8_t room)
{
	uint8_t i, id, type;
	bool add_dpo;

	stop_music();

	clear_room();
	clean_state();
	vdp_screen_disable();

	sys_set_ascii_page3(PAGE_MAP);
	map_inflate(map_map_segment_dict[room], map_map_segment[room], scr_tile_buffer, 192, 32);

	phys_init();
	init_tile_collisions();

	INIT_LIST_HEAD(&display_list);

	sys_set_ascii_page3(PAGE_MAPOBJECTS);

	//log_e("room : %d\n",room);

	type = 0;
	room_objs = map_object_layer[room];
	for (dpo = display_object, i = 0; type != 255 ; i++, dpo++) {
		sys_set_ascii_page3(PAGE_MAPOBJECTS);
		map_object = (struct map_object_item *) room_objs;
		type = map_object->type;
		// log_e("type %d\n", type);
		// log_e("room_objs %x\n", room_objs);
		if (type == ACTIONITEM) {
			uint8_t action_item_type = map_object->object.actionitem.type;
			// log_e("action_item_type %d\n", action_item_type);
			if (action_item_type == TYPE_SCROLL) {
				id = map_object->object.actionitem.action_id;
				if (game_state.scroll[id] == 0) {
					add_tileobject(dpo, tob_ct, TILE_SCROLL);
					phys_set_tile_collision_handler(dpo, pickup_scroll, id);
				}
			} else if (map_object->object.actionitem.type == TYPE_TOGGLE) {
				id = map_object->object.actionitem.action_id;
				if (game_state.toggle[id] == 0) {
					add_tileobject(dpo, tob_ct, TILE_TOGGLE);
					phys_set_tile_collision_handler(dpo, toggle_handler, id);
				} else {
					add_tileobject(dpo, tob_ct, TILE_TOGGLE);
					dpo->tob->cur_anim_step = 1;
				}
			} else if (map_object->object.actionitem.type == TYPE_CROSS) {
				id = map_object->object.actionitem.action_id;
				if (game_state.cross[id] == 0) {
					add_tileobject(dpo, tob_ct, TILE_CROSS);
					add_animator(dpo, ANIM_CYCLE_TILE);
					phys_set_tile_collision_handler(dpo, pickup_cross, id);
				}
			} else if (map_object->object.actionitem.type == TYPE_TELETRANSPORT) {
				// TODO: just go to the other one
				add_tileobject(dpo, tob_ct, TILE_TELETRANSPORT);
			} else if (map_object->object.actionitem.type == TYPE_HEART) {
				id = map_object->object.actionitem.action_id;
				if (game_state.hearth[id] == 0) {
					add_tileobject(dpo, tob_ct, TILE_HEART);
					add_animator(dpo, ANIM_CYCLE_TILE);
					phys_set_tile_collision_handler(dpo, pickup_heart, id);
				}
			} else if (map_object->object.actionitem.type == TYPE_CHECKPOINT) {
				id = map_object->object.actionitem.action_id;
				if (game_state.checkpoint[id] == 0) {
					add_tileobject(dpo, tob_ct, TILE_CHECKPOINT);
					phys_set_tile_collision_handler(dpo, checkpoint_handler, id);
				} else {
					add_tileobject(dpo, tob_ct, TILE_CHECKPOINT);
					dpo->tob->cur_anim_step = 1;
				}
			} else if (map_object->object.actionitem.type == TYPE_SWITCH) {
				game_state.cross_switch_enable = true;
				add_tileobject(dpo, tob_ct, TILE_SWITCH);
				phys_set_tile_collision_handler(dpo, crosswitch_handler, 0);
				if(game_state.cross_switch) {
					dpo->tob->cur_anim_step = 1;
				}
			} else if (map_object->object.actionitem.type == TYPE_CUP) {
				// TODO: end game sequence
				add_tileobject(dpo, tob_ct, TILE_CUP);
			} else if (map_object->object.actionitem.type == TYPE_TRIGGER) {
				add_tileobject(dpo, tob_ct, TILE_INVISIBLE_TRIGGER);
				phys_set_tile_collision_handler(dpo, trigger_handler, id);
			} else if (map_object->object.actionitem.type == TYPE_BELL) {
				if (!game_state.bell) {
					add_tileobject(dpo, tob_ct, TILE_BELL);
					phys_set_tile_collision_handler(dpo, bell_handler, id);
				} else {
					add_tileobject(dpo, tob_ct, TILE_BELL);
					dpo->tob->cur_anim_step = 1;
				}
			} else {
				room_objs += NEXT_OBJECT(struct map_object_actionitem);
				continue;
			}
			room_objs += NEXT_OBJECT(struct map_object_actionitem);
		} else if (map_object->type == STATIC) {
			if (map_object->object.static_.type == TYPE_DRAGON) {
 				// this is crashing, ignore
				//add_tileobject(dpo, tob_ct, TILE_DRAGON);
				// here there is some nice animation to do
			} else if (map_object->object.static_.type == TYPE_LAVA) {
				add_tileobject(dpo, tob_ct, TILE_LAVA);
				// also nice animation to do here
			} else if (map_object->object.static_.type == TYPE_SPEAR) {
				add_tileobject(dpo, tob_ct, TILE_SPEAR);
			} else if (map_object->object.static_.type == TYPE_WATER) {
				// this one also crashing?
				add_tileobject(dpo, tob_ct, TILE_WATER);
				//add_animator(dpo, ANIM_CYCLE_TILE);
			}
			room_objs += NEXT_OBJECT(struct map_object_static);
		} else if (map_object->type == GHOST) {
			add_sprite(dpo, spr_ct, PATRN_GHOST);
			add_animator(dpo, ANIM_STATIC);
			room_objs += NEXT_OBJECT(struct map_object_ghost);
		} else if (map_object->type == ROPE) {
			room_objs += NEXT_OBJECT(struct map_object_rope);
		} else if (map_object->type == DOOR) {
			type = map_object->object.door.type;
			id = map_object->object.door.action_id;
			add_dpo = false;
			if (id == 0) {
				if (game_state.door_trigger)
					add_dpo = true;
			} else if (id == 1 && game_state.toggle[0] == 0) {
				add_dpo = true;
			} else if (id == 2 && !game_state.bell) {
				add_tileobject(dpo, tob_ct, TILE_TRAPDOOR);
				phys_set_colliding_tile_object(dpo, true);
			} else if (id == 3 && game_state.toggle[1] == 0) {
				add_dpo = true;
			} else if (id == 4 && game_state.toggle[2] == 0) {
				add_dpo = true;
			}
			if (add_dpo) {
				add_tileobject(dpo, tob_ct, TILE_DOOR);
				if (type == 0) {
					dpo->tob->cur_anim_step = 1;
				}
				phys_set_colliding_tile_object(dpo, false);
			}
			room_objs += NEXT_OBJECT(struct map_object_door);
		} else if (map_object->type == SHOOTER) {
			if (map_object->object.shooter.type == TYPE_FLUSH) {
				add_sprite(dpo, spr_ct, PATRN_FISH);
				add_animator(dpo, ANIM_STATIC);
			} else if (map_object->object.shooter.type == TYPE_LEAK) {
				add_sprite(dpo, spr_ct, PATRN_WATERDROP);
				add_animator(dpo, ANIM_STATIC);
			} else if (map_object->object.shooter.type == TYPE_GARGOYLE) {
				add_tileobject(dpo, tob_ct, TILE_GARGOLYNE);
			} else if (map_object->object.shooter.type == TYPE_ARCHER) {
				add_tileobject(dpo, tob_ct, TILE_ARCHER_SKELETON);
			} else if (map_object->object.shooter.type == TYPE_PLANT) {
				add_tileobject(dpo, tob_ct, TILE_PLANT);

			}
			room_objs += NEXT_OBJECT(struct map_object_shooter);
		} else if (map_object->type == BLOCK) {
			add_tileobject(dpo, tob_ct, TILE_CROSS);
			add_animator(dpo, ANIM_CYCLE_TILE);
			room_objs += NEXT_OBJECT(struct map_object_block);
		} else if (map_object->type == STEP) {
			// TODO: Add special collisions
			room_objs += NEXT_OBJECT(struct map_object_step);
		} else if (map_object->type == MOVABLE) {
			if (map_object->object.movable.type == TYPE_TEMPLAR) {
				add_sprite(dpo, spr_ct, PATRN_TEMPLAR);
				if (room == ROOM_FOREST ||
					room == ROOM_GRAVEYARD) {
					add_animator(dpo, ANIM_CHASE);
					dpo->state = STATE_OFF_SCREEN;
					dpo->visible = false;
					dpo->spr->cur_state = SPR_STATE_RIGHT;
					if (game_state.templar_ct == 1) {
						dpo->state = STATE_OFF_SCREEN_DELAY_1S;
					} else if (game_state.templar_ct == 2) {
						dpo->state = STATE_OFF_SCREEN_DELAY_2S;
					}
					game_state.templar_ct++;
				}
			} else if (map_object->object.movable.type == TYPE_BAT) {
				add_sprite(dpo, spr_ct, PATRN_BAT);
				add_animator(dpo, ANIM_LEFT_RIGHT);
			} else if (map_object->object.movable.type == TYPE_SPIDER) {
				add_sprite(dpo, spr_ct, PATRN_SPIDER);
				add_animator(dpo, ANIM_UP_DOWN);
				dpo->state = STATE_MOVING_DOWN;
			} else if (map_object->object.movable.type == TYPE_RAT) {
				add_sprite(dpo, spr_ct, PATRN_RAT);
				add_animator(dpo, ANIM_LEFT_RIGHT_FLOOR);
			} else if (map_object->object.movable.type == TYPE_WORM) {
				add_sprite(dpo, spr_ct, PATRN_WORM);
				add_animator(dpo, ANIM_LEFT_RIGHT_FLOOR);
			} else if (map_object->object.movable.type == TYPE_PRIEST) {
				add_tileobject(dpo, tob_ct, TILE_PRIEST);
			} else if (map_object->object.movable.type == TYPE_FLY) {
				add_sprite(dpo, spr_ct, PATRN_FLY);
				add_animator(dpo, ANIM_UP_DOWN);
				dpo->state = STATE_MOVING_DOWN;
			} else if (map_object->object.movable.type == TYPE_SKELETON) {
				add_sprite(dpo, spr_ct, PATRN_SKELETON);
				add_animator(dpo, ANIM_LEFT_RIGHT_FLOOR);
			} else if (map_object->object.movable.type == TYPE_PALADIN) {
				add_sprite(dpo, spr_ct, PATRN_PALADIN);
				add_animator(dpo, ANIM_LEFT_RIGHT_FLOOR);
			// } else if (map_object->object.movable.type == TYPE_DEATH) {
			// 	// this is a big sprite 32x32 not supported yet
			// 	map_object++;
			// 	continue;
			} else if (map_object->object.movable.type == TYPE_DARK_BAT) {
				add_sprite(dpo, spr_ct, PATRN_DARKBAT);
				add_animator(dpo, ANIM_LEFT_RIGHT);
			} else if (map_object->object.movable.type == TYPE_DEMON) {
				add_sprite(dpo, spr_ct, PATRN_DEMON);
				add_animator(dpo, ANIM_LEFT_RIGHT_FLOOR);
			} else if (map_object->object.movable.type == TYPE_SKELETON_CEIL) {
				add_sprite(dpo, spr_ct, PATRN_SKELETON_CEILING);
				add_animator(dpo, ANIM_STATIC);
			} else if (map_object->object.movable.type == TYPE_LAVA) {
				add_sprite(dpo, spr_ct, PATRN_FIREBALL);
				add_animator(dpo, ANIM_STATIC);
			} else if (map_object->object.movable.type == TYPE_SATAN) {
				add_tileobject(dpo, tob_ct, TILE_SATAN);
			} else {
				room_objs += NEXT_OBJECT(struct map_object_movable);
				continue;
			}
			room_objs += NEXT_OBJECT(struct map_object_movable);
		}
	}

	add_jean();
	phys_set_sprite_collision_handler(jean_collision_handler);

	// show all elements
	list_for_each(elem, &display_list) {
		dpo = list_entry(elem, struct displ_object, list);
		if (dpo->type == DISP_OBJECT_SPRITE && dpo->visible) {
			spr_show(dpo->spr);
		} else if (dpo->type == DISP_OBJECT_TILE) {
			tile_object_show(dpo->tob, scr_tile_buffer, false);
		}
	}


	vdp_copy_to_vram(scr_tile_buffer, vdp_base_names_grp1, 704);
	vdp_screen_enable();
	start_music(room);
}

void init_tile_collisions()
{
	uint8_t i;
	for (i = 1; i < 86; i++)
		phys_set_colliding_tile(i);

	phys_clear_colliding_tile(16); // step brown
	phys_clear_colliding_tile(38); // step white
	phys_set_down_colliding_tile(16);
	phys_set_down_colliding_tile(38);
}

void init_map_tilesets() {
	uint8_t room;

	tile_init();

	room = 0;
	if (room == 8) {
		sys_set_ascii_page3(PAGE_MAPTILES);
		INIT_TILE_SET(tileset_map1, maptiles1);
		INIT_TILE_SET(tileset_map2, maptiles2);
		INIT_TILE_SET(tileset_map3, maptiles3);
		INIT_TILE_SET(tileset_map3b, maptiles3b);
		INIT_TILE_SET(tileset_map4, maptiles4);
		INIT_TILE_SET(tileset_map5, maptiles5);
		INIT_TILE_SET(tileset_map6, maptiles6);
		INIT_TILE_SET(tileset_map7, maptiles7);

		tile_set_valloc(&tileset_map1);
		tile_set_to_vram(&tileset_map4, 126);
		tile_set_to_vram(&tileset_map5, 126 + 32);
		tile_set_to_vram(&tileset_map3, 65);
		tile_set_to_vram(&tileset_map3b,97);
		tile_set_to_vram(&tileset_map6, 126 + 64);
		tile_set_to_vram(&tileset_map7, 126 + 96);
	} else {
		sys_set_ascii_page3(PAGE_MAPTILES);
		INIT_TILE_SET(tileset_map1, maptiles1);
		INIT_TILE_SET(tileset_map2, maptiles2);
		INIT_TILE_SET(tileset_map3, maptiles3)
		INIT_TILE_SET(tileset_map4, maptiles4);
		INIT_TILE_SET(tileset_map5, maptiles5);

		tile_set_valloc(&tileset_map1);
		tile_set_valloc(&tileset_map2);
		tile_set_to_vram(&tileset_map4, 126);
		tile_set_to_vram(&tileset_map5, 126 + 32);
		tile_set_valloc(&tileset_map3);
	}
}

void init_resources()
{
	uint8_t two_step_state[] = {2,2};
	uint8_t single_step_state[] = {1,1};
	uint8_t three_step_state[] = {3,3};
	uint8_t bullet_state[] = {1,1};
	uint8_t bat_state[] = {2};
	uint8_t waterdrop_state[] = {3};
	uint8_t single_four_state[] = {4};
	uint8_t spider_state[]= {2};
	uint8_t archer_state[]= {2,2};
	uint8_t jean_state[] = {2,1,2,2,1,2,2};

	init_map_tilesets();

	/** initialize dynamic tile sets */
	sys_set_ascii_page3(PAGE_DYNTILES);

	INIT_DYNAMIC_TILE_SET(tileset[TILE_SCROLL], scroll, 2, 2, 1, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_CHECKPOINT], checkpoint, 2, 3, 2, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_CROSS], cross, 2, 2, 4, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_HEART], hearth, 2, 2, 2, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_BELL], bell, 2, 2, 2, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_SWITCH], crosswitch, 2, 2, 2, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_TOGGLE], toggle, 2, 2, 2, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_TELETRANSPORT], portal, 2, 3, 1, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_CUP], cup, 2, 2, 1, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_DRAGON], dragon, 11, 5, 1, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_LAVA], lava, 1, 1, 1, 2);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_SPEAR], spear, 1, 1, 1, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_WATER], water, 2, 1, 1, 16);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_SATAN], satan, 4, 6, 1, 2);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_ARCHER_SKELETON], archer_skeleton, 2, 3, 1, 2);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_GARGOLYNE], gargolyne, 2, 2, 1, 2);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_PLANT], plant, 2, 2, 1, 2);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_PRIEST], priest, 2, 3, 1, 2);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_DOOR], door, 1, 4, 2, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_TRAPDOOR], trapdoor, 2, 2, 1, 1);
	INIT_DYNAMIC_TILE_SET(tileset[TILE_INVISIBLE_TRIGGER], invisible_trigger, 1, 4, 1, 1);

	/** initialize sprite pattern sets **/
	spr_init();
	sys_set_ascii_page3(PAGE_SPRITES);

	SPR_DEFINE_PATTERN_SET(PATRN_BAT, SPR_SIZE_16x16, 1, 1, bat_state, bat);
	SPR_DEFINE_PATTERN_SET(PATRN_RAT, SPR_SIZE_16x16, 1, 2, two_step_state, rat);
	SPR_DEFINE_PATTERN_SET(PATRN_SPIDER, SPR_SIZE_16x16, 1, 1, bat_state, spider);
	SPR_DEFINE_PATTERN_SET(PATRN_JEAN, SPR_SIZE_16x32, 1, 7, jean_state, monk1);
	SPR_DEFINE_PATTERN_SET(PATRN_TEMPLAR, SPR_SIZE_16x32, 1, 2, two_step_state, templar);
	SPR_DEFINE_PATTERN_SET(PATRN_WORM, SPR_SIZE_16x16, 1, 2, two_step_state, worm);
	SPR_DEFINE_PATTERN_SET(PATRN_SKELETON, SPR_SIZE_16x32, 1, 2, two_step_state, skeleton);
	SPR_DEFINE_PATTERN_SET(PATRN_PALADIN, SPR_SIZE_16x32, 1, 2, two_step_state, paladin);
	SPR_DEFINE_PATTERN_SET(PATRN_GUADANYA, SPR_SIZE_16x16, 1, 1, single_four_state, guadanya);
	SPR_DEFINE_PATTERN_SET(PATRN_GHOST, SPR_SIZE_16x16, 1, 2, two_step_state, ghost);
	SPR_DEFINE_PATTERN_SET(PATRN_DEMON, SPR_SIZE_16x32, 1, 2, two_step_state, demon);
//	SPR_DEFINE_PATTERN_SET(PATRN_DEATH, SPR_SIZE_32x32, 1, 2, 2, death);
	SPR_DEFINE_PATTERN_SET(PATRN_DARKBAT, SPR_SIZE_16x16, 1, 2, two_step_state, darkbat);
	SPR_DEFINE_PATTERN_SET(PATRN_FLY, SPR_SIZE_16x16, 1, 2, two_step_state, fly);
	SPR_DEFINE_PATTERN_SET(PATRN_SKELETON_CEILING, SPR_SIZE_16x32, 1, 2, two_step_state, skeleton_ceiling);
	SPR_DEFINE_PATTERN_SET(PATRN_FISH, SPR_SIZE_16x16, 1, 1, bat_state, fish);
	SPR_DEFINE_PATTERN_SET(PATRN_FIREBALL, SPR_SIZE_16x16, 1, 1, bat_state, fireball);
	SPR_DEFINE_PATTERN_SET(PATRN_WATERDROP, SPR_SIZE_16x16, 1, 1, waterdrop_state, waterdrop);
}
