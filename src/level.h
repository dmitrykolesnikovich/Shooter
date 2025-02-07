/* Copyright (c) 2017-2019 Dmitry Stepanov a.k.a mr.DIMAS
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
* LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
* OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

typedef struct jump_pad_t {
	level_t* level;
	de_node_t* model;
	de_vec3_t force;
	de_static_geometry_t* bounds;
} jump_pad_t;

struct level_t {
	game_t* game;
	de_scene_t* scene;
	actor_t* player;
	footstep_sound_map_t footstep_sound_map;
	DE_ARRAY_DECLARE(jump_pad_t*, jump_pads);
	DE_ARRAY_DECLARE(item_t*, items);
	DE_LINKED_LIST_DECLARE(struct projectile_t, projectiles);
	DE_LINKED_LIST_DECLARE(struct actor_t, actors);
};

void level_create_collider(level_t* level);

bool level_visit(de_object_visitor_t* visitor, level_t* level);

level_t* level_create_test(game_t* game);

void level_update(level_t* level, float dt);

void level_free(level_t* level);