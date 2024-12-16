//
// Physics update tick
// @todo this should run on a separate thread
//
static Tick_Input *Tick_PollInput(AppState *app)
{
    // select slot from circular buffer
    Uint64 current = app->tick_input_max % ArrayCount(app->tick_input_buf);
    {
        app->tick_input_max += 1;
        Uint64 max = app->tick_input_max % ArrayCount(app->tick_input_buf);
        Uint64 min = app->tick_input_min % ArrayCount(app->tick_input_buf);
        if (min == max)
            app->tick_input_min += 1;
    }

    Tick_Input *input = app->tick_input_buf + current;

    V2 dir = {0};
    if (app->keyboard[SDL_SCANCODE_W] || app->keyboard[SDL_SCANCODE_UP])    dir.y += 1;
    if (app->keyboard[SDL_SCANCODE_S] || app->keyboard[SDL_SCANCODE_DOWN])  dir.y -= 1;
    if (app->keyboard[SDL_SCANCODE_A] || app->keyboard[SDL_SCANCODE_LEFT])  dir.x -= 1;
    if (app->keyboard[SDL_SCANCODE_D] || app->keyboard[SDL_SCANCODE_RIGHT]) dir.x += 1;
    input->move_dir = V2_Normalize(dir);

    return input;
}

static void Tick_Iterate(AppState *app)
{
    Tick_Input *input = Tick_PollInput(app);

    // update prev_p
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        obj->prev_p = obj->p;
    }

    // player input
    {
        Object *player = Object_Network(app, app->player_network_slot);
        if (!Object_IsZero(app, player))
        {
            float player_speed = 200.f * TIME_STEP;
            player->dp = V2_Scale(input->move_dir, player_speed);
        }
    }

    // movement & collision
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        obj->has_collision = false;
    }
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        if (!(obj->flags & ObjectFlag_Move)) continue;

        obj->p = V2_Add(obj->p, obj->dp);

        Sprite *obj_sprite = Sprite_Get(app, obj->sprite_id);
        Col_Vertices obj_verts = obj_sprite->collision_vertices;
        V2_VerticesOffset(obj_verts.arr, ArrayCount(obj_verts.arr), obj->p);
        V2 obj_center = V2_VerticesAverage(obj_verts.arr, ArrayCount(obj_verts.arr));

        ForU32(collision_iteration, 8) // support up to 8 overlapping wall collisions
        {
            float closest_obstacle_separation_dist = FLT_MAX;
            V2 closest_obstacle_wall_normal = {0};

            ForU32(obstacle_id, app->object_count)
            {
                Object *obstacle = app->object_pool + obstacle_id;
                if (!(obstacle->flags & ObjectFlag_Collide)) continue;
                if (obj == obstacle) continue;

                Sprite *obstacle_sprite = Sprite_Get(app, obstacle->sprite_id);
                Col_Vertices obstacle_verts = obstacle_sprite->collision_vertices;
                V2_VerticesOffset(obstacle_verts.arr, ArrayCount(obstacle_verts.arr), obstacle->p);
                V2 obstacle_center = V2_VerticesAverage(obstacle_verts.arr, ArrayCount(obstacle_verts.arr));

                float biggest_dist = -FLT_MAX;
                V2 wall_normal = {0};

                // @info(mg) SAT algorithm needs 2 iterations
                // from the perspective of the obj
                // and from the perspective of the obstacle.
                ForU32(sat_iteration, 2)
                {
                    Col_Normals normals = (sat_iteration ?
                                              obj_sprite->collision_normals :
                                              obstacle_sprite->collision_normals);

                    Col_Projection a = CollisionProjection(normals, obstacle_verts);
                    Col_Projection b = CollisionProjection(normals, obj_verts);

                    ForArray(i, a.arr)
                    {
                        static_assert(ArrayCount(a.arr) == ArrayCount(normals.arr));
                        V2 normal = normals.arr[i];

                        V2 obstacle_dir = V2_Sub(obstacle_center, obj_center);
                        if (V2_Inner(normal, obstacle_dir) < 0)
                        {
                            continue;
                        }

                        float d = RngF_MaxDistance(a.arr[i], b.arr[i]);
                        if (d > 0.f)
                        {
                            // @info(mg) We can exit early from checking this
                            //     obstacle since we found an axis that has
                            //     a separation between obj and obstacle.
                            goto skip_this_obstacle;
                        }

                        if (d > biggest_dist)
                        {
                            biggest_dist = d;

                            wall_normal = normal;
                            if (!sat_iteration)
                            {
                                wall_normal = V2_Reverse(wall_normal);
                            }
                        }
                    }
                }

                if (closest_obstacle_separation_dist > biggest_dist)
                {
                    closest_obstacle_separation_dist = biggest_dist;
                    closest_obstacle_wall_normal = wall_normal;
                }

                obj->has_collision |= (biggest_dist < 0.f);
                obstacle->has_collision |= (biggest_dist < 0.f);

                skip_this_obstacle:;
            }

            if (closest_obstacle_separation_dist < 0.f)
            {
                V2 move_out_dir = closest_obstacle_wall_normal;
                float move_out_magnitude = -closest_obstacle_separation_dist;

                V2 move_out = V2_Scale(move_out_dir, move_out_magnitude);
                obj->p = V2_Add(obj->p, move_out);

                // remove all velocity on collision axis
                // we might want to do something different here!
                if (move_out.x) obj->dp.x = 0;
                if (move_out.y) obj->dp.y = 0;
            }
            else
            {
                // Collision not found, stop iterating
                break;
            }
        } // collision_iteration
    } // obj_id


    // animate textures
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        if (Sprite_Get(app, obj->sprite_id)->tex_frames <= 1) continue;

        Uint32 frame_index_map[8] =
        {
            0, 1, 2, 1,
            0, 3, 4, 3
        };

        bool in_idle_frame = (0 == frame_index_map[obj->sprite_animation_index]);

        float distance = V2_Length(V2_Sub(obj->p, obj->prev_p));
        float anim_speed = (18.f * TIME_STEP);
        anim_speed += (400.f * distance * TIME_STEP);

        if (!distance && in_idle_frame)
        {
            anim_speed = 0.f;
        }
        obj->sprite_animation_t += anim_speed;

        float period = 1.f;
        while (obj->sprite_animation_t > period)
        {
            obj->sprite_animation_t -= period;
            obj->sprite_animation_index += 1;
        }

        obj->sprite_animation_index %= ArrayCount(frame_index_map);
        obj->sprite_frame_index = frame_index_map[obj->sprite_animation_index];
    }

    // move camera
    {
        Object *player = Object_Network(app, app->player_network_slot);
        app->camera_p = player->p;
    }
}
