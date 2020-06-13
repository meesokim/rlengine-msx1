/*
 * RetroDeLuxe Engine for MSX
 *
 * Copyright (C) 2019 Enric Martin Geijo (retrodeluxemsx@gmail.com)
 *
 * RDLEngine is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MSX_FONT_H_
#define _MSX_FONT_H

#include "tile.h"

enum font_type {
	FONT_UPPERCASE,
	FONT_LOWERCASE,
	FONT_NUMERIC,
	FONT_SYMBOLS,
};

struct font {
	struct tile_set tiles;
	uint8_t type;
	uint8_t num_glyphs;
	uint8_t glyph_w;
	uint8_t glyph_h;
	uint8_t idx;
};

/**
 * A font set may contain four fonths each providing different types of glyphs
 *  symbols: ascii range 33 to 47
 *  numeric: ascii range 48 to 57
 *  upper:   ascii range 65 to 90
 *  lower:   ascii range 97 to 122
 */
struct font_set {
	struct font *upper;
	struct font *lower;
	struct font *numeric;
	struct font *symbols;
};

#define CHR_SPC 32
#define CHR_0 48
#define CHR_9 57
#define CHR_A 65
#define CHR_Z 90
#define CHR_a 97
#define CHR_z 122
#define CHR_EXCL 33
#define CHR_SLASH 47

#define INIT_FONT(FONT, TILES, TYPE, GLYPHS, W, H)	(FONT).tiles.w = TILES ## _tile_w;\
							(FONT).tiles.h = TILES ## _tile_h;\
							(FONT).tiles.pattern = TILES ## _tile;\
							(FONT).tiles.color = TILES ## _tile_color; \
							(FONT).tiles.allocated = false; \
							(FONT).type = (TYPE); \
							(FONT).num_glyphs = (GLYPHS); \
							(FONT).glyph_w = (W); \
							(FONT).glyph_h = (H);

void init_font(struct font *f, uint8_t *tile_pattern, uint8_t *tile_color,
	uint8_t tile_w, uint8_t tile_h, enum font_type type,
	uint8_t num_glyphs, uint8_t glyph_w, uint8_t glyph_h);
void font_to_vram(struct font *f, uint8_t pos);
void font_to_vram_bank(struct font *f, uint8_t bank, uint8_t pos);
void font_vprint(struct font *f, uint8_t x, uint8_t y, char *text);
void font_printf(struct font_set *fs, uint8_t x, uint8_t y, uint8_t *buffer, char *text);

#endif
