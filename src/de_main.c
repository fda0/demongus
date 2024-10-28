//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//
static void Game_Iterate(AppState *app)
{
    {
        Uint64 new_frame_time = SDL_GetTicks();
        Uint64 delta_time = new_frame_time - app->frame_time;
        app->frame_time = new_frame_time;
        app->dt = delta_time * (0.001f);
        app->dt = Min(app->dt, 1.f); // clamp dt to 1s
    }

    // animate special wall
    {
        Object *obj = Object_Get(app, app->special_wall);
        obj->rotation += app->dt;

        float period = 3.14f/2.f;
        while (obj->rotation > period)
        {
            obj->rotation -= period;
        }
    }

    // player input
    ForArray(player_id_index, app->player_ids)
    {
        Uint32 player_id = app->player_ids[player_id_index];

        V2 dir = {0};
        if (player_id_index == 0)
        {
            if (app->keyboard[SDL_SCANCODE_W] || app->keyboard[SDL_SCANCODE_UP])    dir.y += 1;
            if (app->keyboard[SDL_SCANCODE_S] || app->keyboard[SDL_SCANCODE_DOWN])  dir.y -= 1;
            if (app->keyboard[SDL_SCANCODE_A] || app->keyboard[SDL_SCANCODE_LEFT])  dir.x -= 1;
            if (app->keyboard[SDL_SCANCODE_D] || app->keyboard[SDL_SCANCODE_RIGHT]) dir.x += 1;
        }
        else if (player_id_index == 1)
        {
            // for vim fans
            if (app->keyboard[SDL_SCANCODE_K]) dir.y += 1;
            if (app->keyboard[SDL_SCANCODE_J]) dir.y -= 1;
            if (app->keyboard[SDL_SCANCODE_H]) dir.x -= 1;
            if (app->keyboard[SDL_SCANCODE_L]) dir.x += 1;
        }
        dir = V2_Normalize(dir);

        Object *player = Object_Get(app, player_id);

        bool ice_skating_dlc = (player_id_index == 0);
        if (ice_skating_dlc)
        {
            float player_speed = 0.01f * app->dt;
            V2 player_ddp = V2_Scale(dir, player_speed);
            player->dp = V2_Add(player->dp, player_ddp);

            // pro skater turns
            //if (dir.x && SignF(dir.x) != SignF(player->dp.x)) player->dp.x *= -0.9f;
            //if (dir.y && SignF(dir.y) != SignF(player->dp.y)) player->dp.y *= -0.9f;

            float drag = -15.f * app->dt;
            V2 player_drag = V2_Scale(player->dp, drag);
            player->dp = V2_Add(player->dp, player_drag);
        }
        else
        {
            float player_speed = 5.f * app->dt;
            player->dp = V2_Scale(dir, player_speed);
        }
    }

    // update vertices and normals
    {
        ForU32(obj_id, app->object_count)
        {
            Object *obj = app->object_pool + obj_id;

            float rotation = obj->rotation; // @todo turns -> radians
            float s = SinF(rotation);
            float c = CosF(rotation);

            obj->normals[0] = (V2){ c,  s}; // RIGHT
            obj->normals[1] = (V2){-s,  c}; // TOP
            obj->normals[2] = (V2){-c, -s}; // LEFT
            obj->normals[3] = (V2){ s, -c}; // BOTTOM

            V2 half = V2_Scale(obj->dim, 0.5f);
            obj->vertices[0] = (V2){-half.x, -half.y}; // BOTTOM-LEFT
            obj->vertices[1] = (V2){ half.x, -half.y}; // BOTTOM-RIGHT
            obj->vertices[2] = (V2){-half.x,  half.y}; // TOP-LEFT
            obj->vertices[3] = (V2){ half.x,  half.y}; // TOP-RIGHT

            ForArray(i, obj->vertices)
            {
                V2 vert = obj->vertices[i];

                obj->vertices[i].x = vert.x * c - vert.y * s;
                obj->vertices[i].y = vert.x * s + vert.y * c;

                obj->vertices[i].x += obj->p.x;
                obj->vertices[i].y += obj->p.y;
            }
        }
    }

    // movement
    if (1)
    {
        ForU32(obj_id, app->object_count)
        {
            Object *obj = app->object_pool + obj_id;
            obj->has_collision = false;
        }

        // @todo sat algorithm
        ForU32(obj_id, app->object_count)
        {
            Object *obj = app->object_pool + obj_id;
            if (!(obj->flags & ObjectFlag_Move)) continue;

            obj->p = V2_Add(obj->p, obj->dp);

            Arr4RngF minmax_obj_obj = Object_NormalsInnerVertices(obj, obj);

            ForU32(obstacle_id, app->object_count)
            {
                Object *obstacle = app->object_pool + obstacle_id;
                if (!(obstacle->flags & ObjectFlag_Collide)) continue;
                if (obj == obstacle) continue;

                Arr4RngF minmax_obj_obstacle = Object_NormalsInnerVertices(obj, obstacle);
                bool overlaps = Arr4RngF_Overlaps(minmax_obj_obj, minmax_obj_obstacle);

                Arr4RngF minmax_obstacle_obstacle = Object_NormalsInnerVertices(obstacle, obstacle);
                Arr4RngF minmax_obstacle_obj = Object_NormalsInnerVertices(obstacle, obj);
                overlaps &= Arr4RngF_Overlaps(minmax_obstacle_obstacle, minmax_obstacle_obj);

                obj->has_collision |= overlaps;
                obstacle->has_collision |= overlaps;
            }
        }

    }
    else
    {
        float wall_margin = 0.001f;
        // @speed(mg) This could be speed up in multiple ways.
        //            We could spatially partition the world into chunks
        //            to avoid n^2 scan through all possible world objects.
        //            We just don't care about this for now.
        ForU32(obj_id, app->object_count)
        {
            Object *obj = app->object_pool + obj_id;
            if (!(obj->flags & ObjectFlag_Move)) continue;
            if (!obj->dp.x && !obj->dp.y) continue;

            Uint32 iteration_count = 2;
            ForU32(iteration_index, iteration_count)
            {
                V2 obj_new_p = V2_Add(obj->p, obj->dp);

                struct {
                    Axis2 axis;
                    float t;
                    float near_wall_main;
                } closest_collision = {.t = FLT_MAX};

                ForU32(obstacle_id, app->object_count)
                {
                    Object *obstacle = app->object_pool + obstacle_id;
                    if (!(obstacle->flags & ObjectFlag_Collide)) continue;
                    if (obj == obstacle) continue;

                    V2 minkowski_half_dim = V2_Scale(V2_Add(obj->dim, obstacle->dim), 0.5f);

                    ForU32(main_axis, Axis2_COUNT)
                    {
                        Axis2 other_axis = Axis2_Other(main_axis);

                        if (obj->dp.E[main_axis])
                        {
                            float near_wall_other0 = obstacle->p.E[other_axis] - minkowski_half_dim.E[other_axis];
                            float near_wall_other1 = obstacle->p.E[other_axis] + minkowski_half_dim.E[other_axis];
                            if (obj_new_p.E[other_axis] >= near_wall_other0 &&
                                obj_new_p.E[other_axis] <= near_wall_other1)
                            {
                                float near_wall_main = obstacle->p.E[main_axis] -
                                    minkowski_half_dim.E[main_axis]*SignF(obj->dp.E[main_axis]);

                                float obj_wall_distance = near_wall_main - obj->p.E[main_axis];
                                float collision_t = obj_wall_distance / obj->dp.E[main_axis];

                                if (collision_t >= 0.f && collision_t < closest_collision.t)
                                {
                                    closest_collision.axis = main_axis;
                                    closest_collision.t = collision_t;
                                    closest_collision.near_wall_main = near_wall_main;
                                }
                            }
                        }
                    }
                }

                // @todo(mg) make sure we handle the case where player is in the wall
                if (closest_collision.t >= 0.f && closest_collision.t < 1.f)
                {
                    Axis2 main_axis = closest_collision.axis;

                    obj_new_p.E[main_axis] = closest_collision.near_wall_main -
                        wall_margin*SignF(obj->dp.E[main_axis]);

                    obj->p.E[main_axis] = obj_new_p.E[main_axis];
                    obj->dp.E[main_axis] = 0.f; // @todo(mg) this is not ideal, angled walls will be "sticky", we should calculate new dp vector here

                    if (iteration_index + 1 == iteration_count)
                    {
                        // we are in the last iteration, stop all reminding movement to prevent tunneling through walls
                        obj->dp = (V2){0};
                    }
                }
                else
                {
                    break;
                }
            }

            obj->p = V2_Add(obj->p, obj->dp);
        }
    }

    // move camera
    {
        Object *player = Object_Get(app, app->player_ids[0]);
        app->camera_p = player->p;
    }

    // draw objects
    {
        float camera_scale = 1.f;
        {
            float wh = Max(app->width, app->height); // pick bigger window dimension
            camera_scale = wh / app->camera_range;
        }
        V2 window_transform = (V2){app->width*0.5f, app->height*0.5f};

        ForU32(i, app->object_count)
        {
            Object *obj = app->object_pool + i;
            V2 half_dim = V2_Scale(obj->dim, 0.5f);


            V2 verts[4];
            static_assert(sizeof(verts) == sizeof(obj->vertices));
            memcpy(verts, obj->vertices, sizeof(obj->vertices));

            ForArray(i, verts)
            {
                // apply camera transform
                verts[i].x -= app->camera_p.x;
                verts[i].y -= app->camera_p.y;

                verts[i].x *= camera_scale;
                verts[i].y *= camera_scale;

                verts[i].x += window_transform.x;
                verts[i].y += window_transform.y;

                // fix y axis direction to +Y up (SDL uses +Y down, -Y up)
                verts[i].y = app->height - verts[i].y;
            }

            SDL_FColor fcolor = ColorF_To_SDL_FColor(obj->color);
            fcolor.a = 0.2f;
            if (obj->has_collision)
            {
                fcolor.r = SqrtF(fcolor.r);
                fcolor.g = SqrtF(fcolor.g);
                fcolor.b = SqrtF(fcolor.b);
                fcolor.a = 0.9f;
            }

            SDL_Vertex sdl_verts[4];
            SDL_zerop(sdl_verts);

            static_assert(ArrayCount(verts) == ArrayCount(sdl_verts));
            ForArray(i, sdl_verts)
            {
                sdl_verts[i].position = V2_To_SDL_FPoint(verts[i]);
                sdl_verts[i].color = fcolor;
            }

            int indices[] = { 0, 1, 2, 2, 3, 1 };
            SDL_RenderGeometry(app->renderer, 0,
                               sdl_verts, ArrayCount(sdl_verts),
                               indices, ArrayCount(indices));
        }
    }

    // draw mouse
    {
        static float r = 0;
        r += app->dt * 90.f;
        if (r > 255) r = 0;
        /* set the color to white */
        SDL_SetRenderDrawColor(app->renderer, (int)r, 255 - (int)r, 255, 255);

        float dim = 20;
        SDL_FRect rect = {
            app->mouse.x - 0.5f*dim, app->mouse.y - 0.5f*dim,
            dim, dim
        };
        SDL_RenderFillRect(app->renderer, &rect);
    }
}

static void Game_Init(AppState *app)
{
    app->frame_time = SDL_GetTicks();
    app->object_count += 1; // reserve object under index 0 as special 'nil' value
    app->camera_range = 20;

    // add player
    {
        Object *player = Object_Create(app, ObjectFlag_Draw|ObjectFlag_Move|ObjectFlag_Collide);
        player->p.x = -1.f;
        player->dim.x = 0.5f;
        player->dim.y = 0.7f;
        player->color = ColorF_RGB(.89f, .02f, 0);
        app->player_ids[0] = Object_IdFromPointer(app, player);
    }
    // add player2
    {
        Object *player = Object_Create(app, ObjectFlag_Draw|ObjectFlag_Move|ObjectFlag_Collide);
        player->p.x = 1.f;
        player->dim.x = 0.3f;
        player->dim.y = 0.9f;
        player->color = ColorF_RGB(0.4f, .4f, .94f);
        app->player_ids[1] = Object_IdFromPointer(app, player);
    }

    // add walls
    {
        float thickness = 0.5f;
        float length = 7.5f;
        float off = length*0.5f - thickness*0.5f;
        Object_Wall(app, (V2){off, 0}, (V2){thickness, length});
        Object_Wall(app, (V2){-off, 0}, (V2){thickness, length});
        Object_Wall(app, (V2){0, off}, (V2){length, thickness});
        Object_Wall(app, (V2){0,-off}, (V2){length*0.5f, thickness});

        {
            Object *rot_wall = Object_Wall(app, (V2){-off,-off*2.f}, (V2){length*0.5f, thickness});
            rot_wall->rotation = 0.125f;
        }

        {
            Object *rot_wall = Object_Wall(app, (V2){off,-off*2.f}, (V2){length*0.5f, thickness});
            rot_wall->rotation = 0.125f;
            app->special_wall = Object_IdFromPointer(app, rot_wall);
        }
    }
}
