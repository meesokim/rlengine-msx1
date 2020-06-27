#ifndef _ANIM_H_
#define _ANIM_H_

enum anim_t {
	ANIM_LEFT_RIGHT,
	ANIM_LEFT_RIGHT_FLOOR,
	ANIM_UP_DOWN,
	ANIM_GRAVITY,
	ANIM_STATIC,
	ANIM_JEAN,
	ANIM_JUMP,
	ANIM_CYCLE_TILE,
	ANIM_CHASE,
	ANIM_CLOSE_DOOR,
	ANIM_SHOOTER_PLANT,
	ANIM_FALLING_BULLETS,
	ANIM_WATERDROP,
	ANIM_FISH_JUMP,
	ANIM_SPLASH,
	ANIM_GHOST,
	ANIM_FIREBALL,
	ANIM_ARCHER_SKELETON,
	ANIM_HORIZONTAL_PROJECTILE,
	ANIM_GARGOLYNE,
	ANIM_INTRO_CHASE,
	ANIM_INTRO_JEAN,
	ANIM_LEFT_RIGHT_BOUNDED,
	MAX_ANIMATORS
};

enum jane_anim_state_t {
	JANE_STATE_LEFT,
	JANE_STATE_LEFT_JUMP,
	JANE_STATE_LEFT_CROUCH,
	JANE_STATE_RIGHT,
	JANE_STATE_RIGHT_JUMP,
	JANE_STATE_RIGHT_CROUCH,
	JANE_STATE_DEATH,
};

enum obj_state {
	STATE_IDLE,
	STATE_MOVING_LEFT,
	STATE_MOVING_RIGHT,
	STATE_MOVING_DOWN,
	STATE_MOVING_UP,
	STATE_HOPPING_RIGHT,
	STATE_FALLING_RIGHT,
	STATE_JUMPING,
	STATE_CROUCHING,
	STATE_FALLING,	// 9
	STATE_COLLISION,
	STATE_DEATH,
	STATE_OFF_SCREEN,
	STATE_OFF_SCREEN_DELAY_1S,
	STATE_OFF_SCREEN_DELAY_2S,
};

extern struct animator animators[MAX_ANIMATORS];

void add_animator(struct displ_object *dpo, enum anim_t animidx);

#endif
