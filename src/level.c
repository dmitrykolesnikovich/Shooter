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

#define JUMP_PAD_TAG "JumpPad"
#define JUMP_PAD_BEGIN_TAG "_Begin"
#define JUMP_PAD_END_TAG "_End"

void level_create_collider(level_t* level)
{
	de_node_t* polygon = de_scene_find_node(level->scene, "Polygon");
	if (polygon) {
		de_static_geometry_t* map_collider;
		assert(polygon->type == DE_NODE_TYPE_MESH);
		map_collider = de_scene_create_static_geometry(level->scene);
		de_node_calculate_transforms_ascending(polygon);
		de_static_geometry_fill(map_collider, de_node_to_mesh(polygon), &polygon->global_matrix);
	}
}

jump_pad_t* jump_pad_create(level_t* level, de_node_t* model, de_vec3_t force)
{
	jump_pad_t* pad = DE_NEW(jump_pad_t);
	pad->level = level;
	pad->model = model;
	pad->force = force;
	pad->bounds = de_scene_create_static_geometry(level->scene);

	de_mat4_t transform;
	de_node_get_global_transform(model, &transform);
	de_static_geometry_fill(pad->bounds, de_node_to_mesh(model), &transform);

	DE_ARRAY_APPEND(level->jump_pads, pad);
	return pad;
}

void jump_pad_free(jump_pad_t* pad)
{
	DE_ARRAY_REMOVE(pad->level->jump_pads, pad);
	de_free(pad);
}

void jump_pad_update(jump_pad_t* pad)
{
	level_t* level = pad->level;
	for (actor_t* actor = level->actors.head; actor; actor = actor->next) {
		de_body_t* body = actor->body;
		for (size_t i = 0; i < de_body_get_contact_count(body); ++i) {
			const de_contact_t* contact = de_body_get_contact(body, i);
			if (contact->geometry == pad->bounds) {
				de_body_set_velocity(body, &pad->force);
				break;
			}
		}
	}
}

/**
 * @brief Until we have built-in editor we'll prepare level by scanning level's scene
 * and search for nodes with special names and creating appropriate game objects
 * from them.
 */
void level_scan_scene(level_t* level)
{
	char fmt_buf[1024];
	for (de_node_t* node = de_scene_get_first_node(level->scene); node; node = de_node_get_next(node)) {
		const char* name = de_node_get_name(node);
		if (name && strstr(name, JUMP_PAD_TAG)) {
			const char* separator = strchr(name, '_');
			if (!separator) {
				snprintf(fmt_buf, sizeof(fmt_buf), "%s%s", name, JUMP_PAD_BEGIN_TAG);
				de_node_t* begin = de_scene_find_node(level->scene, fmt_buf);

				snprintf(fmt_buf, sizeof(fmt_buf), "%s%s", name, JUMP_PAD_END_TAG);
				de_node_t* end = de_scene_find_node(level->scene, fmt_buf);

				if (begin && end) {
					de_vec3_t begin_pos;
					de_node_get_global_position(begin, &begin_pos);

					de_vec3_t end_pos;
					de_node_get_global_position(end, &end_pos);

					de_vec3_t force;
					de_vec3_sub(&force, &end_pos, &begin_pos);

					float length;
					de_vec3_normalize_ex(&force, &force, &length);
					
					de_vec3_scale(&force, &force, length / 20.0f);

					jump_pad_create(level, node, force);
				}
			}
		}
	}
}

bool level_visit(de_object_visitor_t* visitor, level_t* level)
{
	bool result = true;
	if (visitor->is_reading) {
		level->game = de_core_get_user_pointer(visitor->core);
	}
	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "Scene", &level->scene, de_scene_visit);
	result &= DE_OBJECT_VISITOR_VISIT_INTRUSIVE_LINKED_LIST(visitor, "Actors", level->actors, actor_t, actor_visit);

	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "Player", &level->player, actor_visit);
	if (visitor->is_reading) {
		level_create_collider(level);
		level_scan_scene(level);
	}
	return result;
}

level_t* level_create_test(game_t* game)
{
	level_t* level = DE_NEW(level_t);
	level->game = game;
	level->scene = de_scene_create(game->core);
	footstep_sound_map_read(game->core, &level->footstep_sound_map);

	de_path_t res_path;
	de_path_init(&res_path);

	actor_t* bot = actor_create(level, ACTOR_TYPE_BOT);
	actor_set_position(bot, &(de_vec3_t){-1.0, 0.0, -1.0});

	item_t* medkit = item_create(level, ITEM_TYPE_MEDKIT);
	item_set_position(medkit, &(de_vec3_t){0.0f, 0.1f, 0.0f});

	/* Ripper */
	de_path_append_cstr(&res_path, "data/models/ripper.fbx");
	de_resource_t* res = de_core_request_resource(game->core, DE_RESOURCE_TYPE_MODEL, &res_path);
	de_model_t* mdl = de_resource_to_model(res);
	de_node_t* ripper1 = de_model_instantiate(mdl, level->scene);
	de_node_set_local_position(ripper1, &(de_vec3_t){ -1, 0, -1 });
	de_node_t* ripper2 = de_model_instantiate(mdl, level->scene);
	de_node_set_local_position(ripper2, &(de_vec3_t){ 1, 0, -1 });
	de_node_t* ripper3 = de_model_instantiate(mdl, level->scene);
	de_node_set_local_position(ripper3, &(de_vec3_t){ 1, 0, 1 });
	de_node_t* ripper4 = de_model_instantiate(mdl, level->scene);
	de_node_set_local_position(ripper4, &(de_vec3_t){ -1, 0, 1 });

	/* Level */
	de_path_clear(&res_path);
	de_path_append_cstr(&res_path, "data/models/dm6.fbx");
	res = de_core_request_resource(game->core, DE_RESOURCE_TYPE_MODEL, &res_path);
	de_model_instantiate(de_resource_to_model(res), level->scene);

	level_create_collider(level);

	level_scan_scene(level);

	de_node_t* particle_system_node = de_node_create(level->scene, DE_NODE_TYPE_PARTICLE_SYSTEM);
	de_particle_system_t* particle_system = de_node_to_particle_system(particle_system_node);
	de_particle_system_emitter_t* emitter = de_particle_system_emitter_create(particle_system, DE_PARTICLE_SYSTEM_EMITTER_TYPE_SPHERE);
	emitter->max_particles = 1000;
	emitter->particle_spawn_rate = 50;
	particle_system->acceleration.y = -0.1f;
	de_color_gradient_t* gradient = de_particle_system_get_color_gradient_over_lifetime(particle_system);
	de_color_gradient_add_point(gradient, 0.00f, &(de_color_t) { 150, 150, 150, 0 });
	de_color_gradient_add_point(gradient, 0.05f, &(de_color_t) { 150, 150, 150, 220 });
	de_color_gradient_add_point(gradient, 0.85f, &(de_color_t) { 255, 255, 255, 180 });
	de_color_gradient_add_point(gradient, 1.00f, &(de_color_t) { 255, 255, 255, 0 });
	de_path_clear(&res_path);
	de_path_append_cstr(&res_path, "data/particles/smoke_04.tga");
	res = de_core_request_resource(game->core, DE_RESOURCE_TYPE_TEXTURE, &res_path);
	de_particle_system_set_texture(particle_system, de_resource_to_texture(res));

	de_path_free(&res_path);

	level->player = actor_create(level, ACTOR_TYPE_PLAYER);
	de_node_t* pp = de_scene_find_node(level->scene, "PlayerPosition");
	if (pp) {
		de_vec3_t pos;
		de_node_get_global_position(pp, &pos);
		actor_set_position(level->player, &pos);
	}

	return level;
}

void level_update(level_t* level, float dt)
{
	for(size_t i = 0; i < level->items.size; ++i) {
		item_update(level->items.data[i], dt);
	}

	for (actor_t* actor = level->actors.head; actor; actor = actor->next) {
		actor_update(actor);
	}

	for (projectile_t* projectile = level->projectiles.head; projectile; projectile = projectile->next) {
		projectile_update(projectile);
	}

	for (size_t i = 0; i < level->jump_pads.size; ++i) {
		jump_pad_update(level->jump_pads.data[i]);
	}
}

void level_free(level_t* level)
{
	/* free jump pads */
	while (level->jump_pads.size) {
		jump_pad_free(DE_ARRAY_LAST(level->jump_pads));
	}
	DE_ARRAY_FREE(level->jump_pads);

	/* free actors */
	while (level->actors.head) {
		actor_free(level->actors.head);
	}

	/* free items */
	while(level->items.size) {
		item_free(DE_ARRAY_LAST(level->items));
	}
	DE_ARRAY_FREE(level->items);

	footstep_sound_map_free(&level->footstep_sound_map);	
	de_scene_free(level->scene);
	de_free(level);
}