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

static bool player_process_event(actor_t* actor, const de_event_t* evt)
{
	player_t* p = &actor->s.player;
	de_vec3_t position;
	de_node_get_global_position(p->camera, &position);
	de_vec3_t look;
	de_node_get_look_vector(p->camera, &look);
	bool processed = false;
	switch (evt->type) {
		case DE_EVENT_TYPE_MOUSE_MOVE:
			p->desired_yaw -= evt->s.mouse_move.vx;
			p->desired_pitch += evt->s.mouse_move.vy;
			if (p->desired_pitch > 90.0) {
				p->desired_pitch = 90.0;
			}
			if (p->desired_pitch < -90.0) {
				p->desired_pitch = -90.0;
			}
			break;
		case DE_EVENT_TYPE_KEY_DOWN:
			switch (evt->s.key.key) {
				case DE_KEY_W:
					p->controller.move_forward = true;
					break;
				case DE_KEY_S:
					p->controller.move_backward = true;
					break;
				case DE_KEY_A:
					p->controller.strafe_left = true;
					break;
				case DE_KEY_D:
					p->controller.strafe_right = true;
					break;
				case DE_KEY_C:
					p->controller.crouch = true;
					break;
				case DE_KEY_G:
					/* throw grenade */
					projectile_create(actor->parent_level, PROJECTILE_TYPE_GRENADE, &position, &look);
					break;
				case DE_KEY_Space:
					if (!p->controller.jumped) {
						if (actor_has_ground_contact(actor)) {
							de_body_set_y_velocity(actor->body, 0.085f);
						}
						p->controller.jumped = true;
					}
					break;
				case DE_KEY_LSHIFT:
					p->controller.run = true;
					break;
				default:
					break;
			}
			break;
		case DE_EVENT_TYPE_KEY_UP:
			switch (evt->s.key.key) {
				case DE_KEY_W:
					p->controller.move_forward = false;
					break;
				case DE_KEY_S:
					p->controller.move_backward = false;
					break;
				case DE_KEY_A:
					p->controller.strafe_left = false;
					break;
				case DE_KEY_D:
					p->controller.strafe_right = false;
					break;
				case DE_KEY_C:
					p->controller.crouch = false;
					break;
				case DE_KEY_Space:
					p->controller.jumped = false;
					break;
				case DE_KEY_LSHIFT:
					p->controller.run = false;
					break;
				default:
					break;
			}
			break;
		case DE_EVENT_TYPE_MOUSE_DOWN:
			p->controller.shoot = true;
			break;
		case DE_EVENT_TYPE_MOUSE_UP:
			p->controller.shoot = false;
			break;
		case DE_EVENT_TYPE_MOUSE_WHEEL: {
			int delta = evt->s.mouse_wheel.delta;
			if (delta > 0) {
				player_next_weapon(p);
			} else if (delta < 0) {
				player_prev_weapon(p);
			}
			break;
		}
		default:
			break;
	}
	return processed;
}

static weapon_t* player_get_current_weapon(player_t* player)
{
	if (player->current_weapon < (int)player->weapons.size) {
		return player->weapons.data[player->current_weapon];		
	}
	return NULL;
}

static void player_init(actor_t* actor)
{
	player_t* p = &actor->s.player;
	actor->move_speed = 0.06f;
	p->stand_body_height = 0.5f;
	p->crouch_body_height = 0.30f;
	p->sit_down_speed = 0.045f;
	p->stand_up_speed = 0.06f;
	p->run_speed_multiplier = 1.75f;
	p->camera_position = (de_vec3_t) { 0, p->stand_body_height, 0 };
	level_t* level = actor->parent_level;
	de_scene_t* scene = level->scene;

	de_capsule_shape_t* capsule_shape = de_convex_shape_to_capsule(de_body_get_shape(actor->body));
	de_capsule_shape_set_height(capsule_shape, p->stand_body_height);

	p->camera = de_node_create(scene, DE_NODE_TYPE_CAMERA);
	de_node_set_local_position(p->camera, &p->camera_position);
	de_node_attach(p->camera, actor->pivot);

	p->flash_light = de_node_create(scene, DE_NODE_TYPE_LIGHT);
	de_light_set_radius(de_node_to_light(p->flash_light), 0);
	de_node_attach(p->flash_light, p->camera);

	p->laser_dot = de_node_create(scene, DE_NODE_TYPE_LIGHT);
	de_light_t* laser_dot = de_node_to_light(p->laser_dot);
	de_light_set_radius(laser_dot, 0.5f);
	de_light_set_color(laser_dot, &(de_color_t) { 255, 0, 0, 255 });
	de_light_set_cast_shadows(laser_dot, false);

	p->weapon_pivot = de_node_create(scene, DE_NODE_TYPE_BASE);
	de_node_attach(p->weapon_pivot, p->camera);
	p->weapon_position = (de_vec3_t) { -0.065f, -0.052f, 0.02f };
	de_node_set_local_position(p->weapon_pivot, &p->weapon_position);

	player_add_weapon(p, weapon_create(level, WEAPON_TYPE_M4));
	player_add_weapon(p, weapon_create(level, WEAPON_TYPE_AK47));

	de_path_t path;
	de_path_from_cstr_as_view(&path, "data/models/sphere.fbx");
	de_resource_t* res = de_core_request_resource(actor->parent_level->game->core, DE_RESOURCE_TYPE_MODEL, &path);
	p->ray_cast_pick = de_model_instantiate(de_resource_to_model(res), actor->parent_level->scene);
}

static bool player_visit(de_object_visitor_t* visitor, actor_t* actor)
{
	player_t* player = &actor->s.player;
	bool result = true;
	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "Camera", &player->camera, de_node_visit);
	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "FlashLight", &player->flash_light, de_node_visit);
	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "WeaponPivot", &player->weapon_pivot, de_node_visit);
	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "LaserDot", &player->laser_dot, de_node_visit);
	result &= DE_OBJECT_VISITOR_VISIT_POINTER(visitor, "PickSphere", &player->ray_cast_pick, de_node_visit);
	result &= de_object_visitor_visit_float(visitor, "Pitch", &player->pitch);
	result &= de_object_visitor_visit_float(visitor, "DesiredPitch", &player->desired_pitch);
	result &= de_object_visitor_visit_float(visitor, "Yaw", &player->yaw);
	result &= de_object_visitor_visit_float(visitor, "DesiredYaw", &player->desired_yaw);
	result &= de_object_visitor_visit_float(visitor, "StandBodyRadius", &player->stand_body_height);
	result &= de_object_visitor_visit_float(visitor, "CrouchBodyRadius", &player->crouch_body_height);
	result &= de_object_visitor_visit_float(visitor, "StandUpSpeed", &player->stand_up_speed);
	result &= de_object_visitor_visit_float(visitor, "SitDownSpeed", &player->sit_down_speed);
	result &= de_object_visitor_visit_float(visitor, "RunSpeedMultiplier", &player->run_speed_multiplier);
	result &= de_object_visitor_visit_float(visitor, "CameraWobble", &player->camera_wobble);
	result &= de_object_visitor_visit_float(visitor, "PathLength", &player->path_len);
	result &= de_object_visitor_visit_vec3(visitor, "CameraOffset", &player->camera_offset);
	result &= de_object_visitor_visit_vec3(visitor, "CameraDestOffset", &player->camera_dest_offset);
	result &= de_object_visitor_visit_vec3(visitor, "CameraPosition", &player->camera_position);
	result &= de_object_visitor_visit_vec3(visitor, "WeaponOffset", &player->weapon_offset);
	result &= de_object_visitor_visit_vec3(visitor, "WeaponDestOffset", &player->weapon_dest_offset);
	result &= de_object_visitor_visit_vec3(visitor, "WeaponPosition", &player->weapon_position);
	result &= DE_OBJECT_VISITOR_VISIT_POINTER_ARRAY(visitor, "Weapons", player->weapons, weapon_visit);	
	return result;
}

static void player_deinit(actor_t* actor)
{
	player_t* p = &actor->s.player;
	for (size_t i = 0; i < p->weapons.size; ++i) {
		weapon_free(p->weapons.data[i]);
	}
	DE_ARRAY_FREE(p->weapons);
}

static void player_update(actor_t* actor)
{
	player_t* player = &actor->s.player;
	de_node_t* pivot = actor->pivot;
	de_node_t* camera = player->camera;
	de_body_t* body = actor->body;

	de_vec3_t look = (de_vec3_t) { 0 };
	de_node_get_look_vector(pivot, &look);
	de_vec3_normalize(&look, &look);

	de_vec3_t side = (de_vec3_t) { 0 };
	de_node_get_side_vector(pivot, &side);
	de_vec3_normalize(&side, &side);

	/* movement */
	bool is_moving = false;
	de_vec3_t direction = (de_vec3_t) { 0 };
	if (player->controller.move_forward) {
		de_vec3_add(&direction, &direction, &look);
		is_moving = true;
	}
	if (player->controller.move_backward) {
		de_vec3_sub(&direction, &direction, &look);
		is_moving = true;
	}
	if (player->controller.strafe_left) {
		de_vec3_add(&direction, &direction, &side);
		is_moving = true;
	}
	if (player->controller.strafe_right) {
		de_vec3_sub(&direction, &direction, &side);
		is_moving = true;
	}

	de_capsule_shape_t* capsule_shape = de_convex_shape_to_capsule(de_body_get_shape(body));
	const float actual_height = de_capsule_shape_get_height(capsule_shape);

	player->is_crouch = actual_height < (player->stand_body_height - 3 * FLT_EPSILON);

	/* crouch */
	if (player->controller.crouch) {
		float height = actual_height;
		height -= player->sit_down_speed;
		if (height < player->crouch_body_height) {
			height = player->crouch_body_height;
		}
		de_capsule_shape_set_height(capsule_shape, height);
	} else {
		/* make sure that we have enough space to stand up by casting ray up */
		de_ray_t probe_ray;
		de_vec3_t probe_ray_end;
		de_vec3_add(&probe_ray_end, &body->position, &(de_vec3_t) {0, player->stand_body_height, 0});
		de_ray_by_two_points(&probe_ray, &body->position, &probe_ray_end);
		if (!de_ray_cast(actor->parent_level->scene, &probe_ray, DE_RAY_CAST_FLAGS_IGNORE_BODY, &player->ray_cast_list)) {
			float height = actual_height;
			height += player->stand_up_speed;
			if (height > player->stand_body_height) {
				height = player->stand_body_height;
			}
			de_capsule_shape_set_height(capsule_shape, height);
		}
	}

	for (size_t i = 0; i < player->weapons.size; ++i) {
		weapon_update(player->weapons.data[i]);
	}

	if (player->controller.shoot) {
		weapon_t* wpn = player_get_current_weapon(player);
		if (wpn) {
			weapon_shoot(wpn);
		}
	}

	/* make sure that camera will be always at the top of the body */
	player->camera_position.y = actual_height;

	/* apply camera wobbling */
	if (is_moving && de_body_get_contact_count(body) > 0) {
		player->camera_dest_offset.x = 0.05f * (float)cos(player->camera_wobble * 0.5f);
		player->camera_dest_offset.y = 0.1f * (float)sin(player->camera_wobble);

		player->weapon_dest_offset.x = 0.0125f * (float)cos(player->camera_wobble * 0.5f);
		player->weapon_dest_offset.y = 0.0125f * (float)sin(player->camera_wobble);

		player->camera_wobble += 0.25f;
	} else {
		player->camera_dest_offset.x = 0;
		player->camera_dest_offset.y = 0;

		player->weapon_dest_offset.x = 0;
		player->weapon_dest_offset.y = 0;
	}

	/* camera offset will follow destination offset -> smooth movements */
	player->camera_offset.x += (player->camera_dest_offset.x - player->camera_offset.x) * 0.1f;
	player->camera_offset.y += (player->camera_dest_offset.y - player->camera_offset.y) * 0.1f;
	player->camera_offset.z += (player->camera_dest_offset.z - player->camera_offset.z) * 0.1f;

	player->weapon_offset.x += (player->weapon_dest_offset.x - player->weapon_offset.x) * 0.1f;
	player->weapon_offset.y += (player->weapon_dest_offset.y - player->weapon_offset.y) * 0.1f;
	player->weapon_offset.z += (player->weapon_dest_offset.z - player->weapon_offset.z) * 0.1f;

	/* set actual camera position */
	de_vec3_t combined_position;
	de_vec3_add(&combined_position, &player->camera_position, &player->camera_offset);
	de_node_set_local_position(player->camera, &combined_position);

	de_vec3_add(&combined_position, &player->weapon_position, &player->weapon_offset);
	de_node_set_local_position(player->weapon_pivot, &combined_position);

	/* run */
	float speed_multiplier = player->is_crouch ? 0.5f : 1.0f;
	if (player->controller.run && !player->is_crouch) {
		speed_multiplier = player->run_speed_multiplier;
	}

	if (de_vec3_sqr_len(&direction) > 0) {
		de_vec3_normalize(&direction, &direction);
		player->path_len += 0.05f;
		if (player->path_len >= 1) {
			level_t* level = actor->parent_level;
			for (size_t i = 0; i < de_body_get_contact_count(body); ++i) {
				de_contact_t* contact = de_body_get_contact(body, i);
				if (contact->triangle && contact->normal.y > 0.707) {
					de_resource_t* res = footstep_sound_map_probe(&level->footstep_sound_map, contact->triangle->material_hash);
					if (res) {
						de_sound_buffer_t* buffer = de_resource_to_sound_buffer(res);
						de_sound_context_t* ctx = de_core_get_sound_context(level->game->core);
						de_sound_source_t* src = de_sound_source_create(ctx, DE_SOUND_SOURCE_TYPE_3D);
						de_sound_source_set_buffer(src, buffer);
						de_sound_source_set_play_once(src, true);
						de_sound_source_play(src);
						de_vec3_t pos;
						de_node_get_global_position(pivot, &pos);
						de_sound_source_set_position(src, &pos);
						break;
					}
				}
			}
			player->path_len = 0;
		}
	}

	player->yaw += (player->desired_yaw - player->yaw) * 0.22f;
	player->pitch += (player->desired_pitch - player->pitch) * 0.22f;

	if (actor_has_ground_contact(actor)) {
		de_body_set_x_velocity(body, direction.x * speed_multiplier * actor->move_speed);
		de_body_set_z_velocity(body, direction.z * speed_multiplier * actor->move_speed);
	} else {
		/* a bit of air-control */
		de_body_add_acceleration(body, &(de_vec3_t) {
			.x = direction.x * 5.0f,
			.z = direction.z * 5.0f,
		});
	}

	de_quat_t yaw_rot;
	de_node_set_local_rotation(pivot, de_quat_from_axis_angle(&yaw_rot, &(de_vec3_t) { 0, 1, 0 }, de_deg_to_rad(player->yaw)));

	de_quat_t pitch_rot;
	de_node_set_local_rotation(camera, de_quat_from_axis_angle(&pitch_rot, &(de_vec3_t) { 1, 0, 0 }, de_deg_to_rad(player->pitch)));

	de_vec3_t camera_global_position;
	de_node_get_global_position(player->camera, &camera_global_position);

	de_vec3_t camera_look;
	de_node_get_look_vector(camera, &camera_look);

	const de_ray_t laser_sight_ray = {
		.origin = camera_global_position, .dir = { camera_look.x * 100, camera_look.y * 100, camera_look.z * 100 }
	};

	if (de_ray_cast(actor->parent_level->scene, &laser_sight_ray, DE_RAY_CAST_FLAGS_SORT_RESULTS, &player->ray_cast_list)) {

		de_ray_cast_result_t* closest = NULL;
		for(size_t i = 0; i < player->ray_cast_list.size; ++i) {
			if(player->ray_cast_list.data[i].body != body) {
				closest = &player->ray_cast_list.data[i];
				break;
			}
		}

		if (closest) {
			de_vec3_t dot_offset;
			de_vec3_normalize(&dot_offset, &closest->normal);
			de_vec3_scale(&dot_offset, &dot_offset, 0.2f);

			de_vec3_t laser_dot_position;
			de_vec3_add(&laser_dot_position, &closest->position, &dot_offset);
			de_node_set_local_position(player->laser_dot, &laser_dot_position);

			de_node_set_local_position(player->ray_cast_pick, &closest->position);
		}
	}

	/* listener */
	de_sound_context_t* ctx = de_core_get_sound_context(actor->parent_level->game->core);
	de_listener_t* lst = de_sound_context_get_listener(ctx);

	de_listener_set_orientation(lst, &look, &(de_vec3_t) { 0, 1, 0 });
	de_listener_set_position(lst, &camera_global_position);

	weapon_t* wpn = player_get_current_weapon(player);
	hud_update(actor->parent_level->game->hud, actor->health, wpn ? wpn->ammo : 0);
}

actor_dispatch_table_t* player_get_dispatch_table()
{
	static actor_dispatch_table_t table = {
		.init = player_init,
		.deinit = player_deinit,
		.visit = player_visit,
		.update = player_update,
		.process_event = player_process_event,
	};
	return &table;
}

void player_add_weapon(player_t* player, weapon_t* wpn)
{
	de_node_attach(wpn->model, player->weapon_pivot);
	DE_ARRAY_APPEND(player->weapons, wpn);

	player_next_weapon(player);
}

void player_remove_weapon(player_t* player, weapon_t* wpn)
{
	DE_ARRAY_REMOVE(player->weapons, wpn);
	player_prev_weapon(player);
}

void player_next_weapon(player_t* player)
{
	++player->current_weapon;
	if (player->current_weapon >= (int)player->weapons.size) {
		player->current_weapon = (int)player->weapons.size - 1;
	}
	for (size_t i = 0; i < player->weapons.size; ++i) {
		weapon_set_visible(player->weapons.data[i], i == (size_t)player->current_weapon ? true : false);
	}
}

void player_prev_weapon(player_t* player)
{
	--player->current_weapon;
	if (player->current_weapon < 0) {
		player->current_weapon = 0;
	}
	for (size_t i = 0; i < player->weapons.size; ++i) {
		weapon_set_visible(player->weapons.data[i], i == (size_t)player->current_weapon ? true : false);
	}
}