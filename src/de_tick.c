//
// Physic tick update
// @todo this should run on a separate thread
//
static TickInput *Tick_PollInput(AppState *app)
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

    TickInput *input = app->tick_input_buf + current;

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
    TickInput *input = Tick_PollInput(app);

    // update prev_p
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        obj->prev_p = obj->p;
    }

    // animate special wall
    {
        Object *obj = Object_Get(app, app->special_wall);
        obj->collision_rotation = WrapF(0.f, 1.f, obj->collision_rotation + TIME_STEP);
        obj->sprite_rotation = obj->collision_rotation;
        Object_UpdateCollisionVerticesAndNormals(obj);
    }

    // player input
    ForArray(player_id_index, app->player_ids)
    {
        Object *player = Object_PlayerFromIndex(app, player_id_index);

        bool ice_skating_dlc = (player_id_index == 1);
        if (ice_skating_dlc)
        {
            // @todo(mg): this is bad, we need a fixed timestep
            float player_speed = 0.005f * TIME_STEP;
            V2 player_ddp = V2_Scale(input->move_dir, player_speed);
            player->dp = V2_Add(player->dp, player_ddp);

            float drag = -0.999f * TIME_STEP;
            V2 player_drag = V2_Scale(player->dp, drag);
            player->dp = V2_Add(player->dp, player_drag);
        }
        else
        {
            float player_speed = 5.f * TIME_STEP;
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
        Object_UpdateCollisionVerticesAndNormals(obj);

        ForU32(collision_iteration, 8) // support up to 8 overlapping wall collisions
        {
            float closest_obstacle_separation_dist = FLT_MAX;
            V2 closest_obstacle_wall_normal = {0};

            ForU32(obstacle_id, app->object_count)
            {
                Object *obstacle = app->object_pool + obstacle_id;
                if (!(obstacle->flags & ObjectFlag_Collide)) continue;
                if (obj == obstacle) continue;

                float biggest_dist = -FLT_MAX;
                V2 wall_normal = {0};

                // @info(mg) SAT algorithm needs 2 iterations
                // from the perspective of the obj
                // and from the perspective of the obstacle.
                ForU32(sat_iteration, 2)
                {
                    Object *normal_obj;
                    Object *projected_obstacle;
                    CollisionProjectionResult a;
                    CollisionProjectionResult b;
                    if (sat_iteration == 0)
                    {
                        normal_obj = obj;
                        projected_obstacle = obstacle;
                    }
                    else
                    {
                        normal_obj = obstacle;
                        projected_obstacle = obj;
                    }

                    a = Object_CollisionProjection(normal_obj->collision_normals, normal_obj->collision_vertices); // @speed(mg) we could cache these per object
                    b = Object_CollisionProjection(normal_obj->collision_normals, projected_obstacle->collision_vertices);

                    ForArray(i, a.arr)
                    {
                        static_assert(ArrayCount(a.arr) == ArrayCount(obj->collision_normals.arr));
                        V2 normal = normal_obj->collision_normals.arr[i];

                        V2 obj_col = Object_GetAvgCollisionPos(normal_obj);
                        V2 obstacle_col = Object_GetAvgCollisionPos(projected_obstacle);
                        V2 obstacle_dir = V2_Sub(obstacle_col, obj_col);
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
                            if (normal_obj == obj)
                            {
                                wall_normal = V2_Reverse(normal);
                            }
                            else
                            {
                                wall_normal = normal;
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

                Object_UpdateCollisionVerticesAndNormals(obj);
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
        if (Sprite_Get(app, obj->sprite_id)->frame_count <= 1) continue;

        Uint32 frame_index_map[8] =
        {
            0, 1, 2, 1,
            0, 3, 4, 3
        };

        bool in_idle_frame = (0 == frame_index_map[obj->sprite_animation_index]);

        float distance = V2_Length(V2_Sub(obj->p, obj->prev_p));
        float anim_speed = (16.f * TIME_STEP);
        anim_speed += (3200.f * distance * TIME_STEP);

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
        Object *player = Object_Get(app, app->player_ids[0]);
        app->camera_p = player->p;
    }
}
