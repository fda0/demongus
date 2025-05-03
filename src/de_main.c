//
// @info(mg) The idea is that this file should contain game logic
//           and it should be isolated from platform specific
//           stuff when it's reasonable.
//

static void Game_VerticesCameraTransform(AppState *app, V2 verts[4], float camera_scale, V2 window_transform)
{
    ForU32(i, 4)
    {
        // apply camera transform
        verts[i].x -= app->camera_p.x;
        verts[i].y -= app->camera_p.y;

        verts[i].x *= camera_scale;
        verts[i].y *= camera_scale;

        verts[i].x += window_transform.x;
        verts[i].y += window_transform.y;

        // fix y axis direction to +Y up (SDL uses +Y down, -Y up)
        verts[i].y = app->window_height - verts[i].y;
    }
}

static void Game_IssueDrawCommands(AppState *app)
{
    // animate collision overlay texture
    if (app->debug.draw_collision_box)
    {
        Sprite *sprite_overlay = Sprite_Get(app, app->sprite_overlay_id);

        float overlay_speed = 12.f;
        app->debug.collision_sprite_animation_t += overlay_speed * app->dt;

        float period = 1.f;
        while (app->debug.collision_sprite_animation_t > period)
        {
            app->debug.collision_sprite_animation_t -= period;
            app->debug.collision_sprite_frame_index += 1;
        }

        app->debug.collision_sprite_frame_index %= sprite_overlay->frame_count;
    }

    // draw objects
    {
        float camera_scale = 1.f;
        {
            float wh = Max(app->window_width, app->window_height); // pick bigger window dimension
            camera_scale = wh / app->camera_range;
        }
        V2 window_transform = (V2){app->window_width*0.5f, app->window_height*0.5f};

        ForU32(object_index, app->object_count)
        {
            Object *obj = app->object_pool + object_index;

            if (obj->dirty_sprite_vertices)
            {
                Object_CalculateVerticesAndNormals(obj, true);
            }

            V2 verts[4];
            static_assert(sizeof(verts) == sizeof(obj->sprite_vertices.arr));
            memcpy(verts, obj->sprite_vertices.arr, sizeof(obj->sprite_vertices.arr));
            Game_VerticesCameraTransform(app, verts, camera_scale, window_transform);

            SDL_FColor fcolor = ColorF_To_SDL_FColor(obj->color);
            SDL_Vertex sdl_verts[4];
            SDL_zerop(sdl_verts);

            static_assert(ArrayCount(verts) == ArrayCount(sdl_verts));
            ForArray(i, sdl_verts)
            {
                sdl_verts[i].position = V2_To_SDL_FPoint(verts[i]);
                sdl_verts[i].color = fcolor;
            }

            Sprite *sprite = Sprite_Get(app, obj->sprite_id);
            {
                float tex_y0 = 0.f;
                float tex_y1 = 1.f;
                if (sprite->frame_count > 1)
                {
                    Uint32 frame_index = obj->sprite_frame_index;
                    float tex_height = 1.f / sprite->frame_count;
                    tex_y0 = frame_index * tex_height;
                    tex_y1 = tex_y0 + tex_height;
                }
                sdl_verts[0].tex_coord = (SDL_FPoint){1, tex_y1};
                sdl_verts[1].tex_coord = (SDL_FPoint){0, tex_y1};
                sdl_verts[2].tex_coord = (SDL_FPoint){0, tex_y0};
                sdl_verts[3].tex_coord = (SDL_FPoint){1, tex_y0};
            }

            int indices[] = { 0, 1, 2, 0, 3, 2 };
            SDL_RenderGeometry(app->renderer, sprite->tex,
                               sdl_verts, ArrayCount(sdl_verts),
                               indices, ArrayCount(indices));
        }

        if (app->debug.draw_collision_box)
        {
            ForU32(object_index, app->object_count)
            {
                Object *obj = app->object_pool + object_index;
                if (!(obj->flags & ObjectFlag_Collide)) continue;

                V2 verts[4];
                static_assert(sizeof(verts) == sizeof(obj->collision_vertices.arr));
                memcpy(verts, obj->collision_vertices.arr, sizeof(obj->collision_vertices.arr));
                Game_VerticesCameraTransform(app, verts, camera_scale, window_transform);

                ColorF color = ColorF_RGBA(1, 0, 0.8f, 0.8f);
                if (obj->has_collision)
                {
                    color.g = 1;
                    color.a = 1;
                }

                SDL_FColor fcolor = ColorF_To_SDL_FColor(color);
                SDL_Vertex sdl_verts[4];
                SDL_zerop(sdl_verts);

                static_assert(ArrayCount(verts) == ArrayCount(sdl_verts));
                ForArray(i, sdl_verts)
                {
                    sdl_verts[i].position = V2_To_SDL_FPoint(verts[i]);
                    sdl_verts[i].color = fcolor;
                }

                Sprite *sprite = Sprite_Get(app, app->sprite_overlay_id);
                {
                    float tex_y0 = 0.f;
                    float tex_y1 = 1.f;
                    if (sprite->frame_count > 1)
                    {
                        Uint32 frame_index = app->debug.collision_sprite_frame_index;
                        float tex_height = 1.f / sprite->frame_count;
                        tex_y0 = frame_index * tex_height;
                        tex_y1 = tex_y0 + tex_height;
                    }
                    sdl_verts[0].tex_coord = (SDL_FPoint){1, tex_y1};
                    sdl_verts[1].tex_coord = (SDL_FPoint){0, tex_y1};
                    sdl_verts[2].tex_coord = (SDL_FPoint){0, tex_y0};
                    sdl_verts[3].tex_coord = (SDL_FPoint){1, tex_y0};
                }

                int indices[] = { 0, 1, 2, 0, 3, 2 };
                SDL_RenderGeometry(app->renderer, sprite->tex,
                                   sdl_verts, ArrayCount(sdl_verts),
                                   indices, ArrayCount(indices));
            }
        }
    }

    // draw debug networking stuff
    {
        ColorF green = ColorF_RGB(0, 1, 0);
        ColorF red = ColorF_RGB(1, 0, 0);

        SDL_FRect rect = { 0, 0, 30, 30 };
        if (!app->net.is_server) rect.x = 40;

        ColorF color = app->net.err ? red : green;
        SDL_SetRenderDrawColorFloat(app->renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(app->renderer, &rect);
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

static void Game_Iterate(AppState *app)
{
    {
        app->frame_id += 1;

        Uint64 new_frame_time = SDL_GetTicks();
        Uint64 delta_time = new_frame_time - app->frame_time;
        app->frame_time = new_frame_time;
        app->dt = delta_time * (0.001f);
        app->tick_dt_accumulator += Min(app->dt, 1.f); // clamp dt to 1s

        if (app->debug.fixed_dt)
        {
            app->dt = app->debug.fixed_dt;
        }
    }

    Net_Iterate(app);

    bool run_simulation = (!app->debug.pause_on_every_frame || !app->debug.paused_frame);
    if (run_simulation)
    {
        while (app->tick_dt_accumulator > TIME_STEP)
        {
            app->tick_id += 1;
            app->tick_dt_accumulator -= TIME_STEP;
            Tick_Iterate(app);
        }
    }
    Game_IssueDrawCommands(app);
}

static void Game_Init(AppState *app)
{
    // init debug options
    {
        //app->debug.fixed_dt = 0.1f;
        //app->debug.pause_on_every_frame = true;
        app->debug.draw_collision_box = true;
    }

    Net_Init(app);

    app->frame_time = SDL_GetTicks();
    app->object_count += 1; // reserve object under index 0 as special 'nil' value
    app->sprite_count += 1; // reserve sprite under index 0 as special 'nil' value
    app->camera_range = 15;

    // this assumes we run the game from build directory
    // @todo in the future we should force CWD or query demongus absolute path etc
    Sprite *sprite_overlay = Sprite_Create(app, "../res/pxart/overlay.png", 6);
    app->sprite_overlay_id = Sprite_IdFromPointer(app, sprite_overlay);

    Sprite *sprite_crate = Sprite_Create(app, "../res/pxart/crate.png", 1);
    Sprite *sprite_dude = Sprite_Create(app, "../res/pxart/dude_walk.png", 5);
    Sprite *sprite_ref = Sprite_Create(app, "../res/pxart/reference.png", 1);

    // add player
    {
        Object *player = Object_Create(app, ObjectFlag_Draw|ObjectFlag_Move|ObjectFlag_Collide);
        player->p.x = -0.f;
        float scale = 0.0175f;
        V2 collision_dim = {0};
        collision_dim.x = sprite_dude->tex->w * scale;
        collision_dim.y = (sprite_dude->tex->h / 4.f) * scale;
        player->vertices_relative_to_p.arr[0] = (V2){player->p.x - collision_dim.x / 2, player->p.y - collision_dim.y / 2};
        player->vertices_relative_to_p.arr[1] = (V2){player->p.x + collision_dim.x / 2, player->p.y - collision_dim.y / 2};
        player->vertices_relative_to_p.arr[2] = (V2){player->p.x + collision_dim.x / 2, player->p.y + collision_dim.y / 2};
        player->vertices_relative_to_p.arr[3] = (V2){player->p.x - collision_dim.x / 2, player->p.y + collision_dim.y / 2};
        player->color = ColorF_RGB(1,1,1);
        //player->rotation = 0.3f;
        player->sprite_id = Sprite_IdFromPointer(app, sprite_dude);
        app->player_ids[0] = Object_IdFromPointer(app, player);
    }
    // add player2
    {
        Object *player = Object_Create(app, ObjectFlag_Draw|ObjectFlag_Move|ObjectFlag_Collide);
        player->p.x = 3.f;
        V2 collision_dim = {0};
        collision_dim.x = 0.3f;
        collision_dim.y = 0.9f;
        player->vertices_relative_to_p.arr[0] = (V2){player->p.x - collision_dim.x / 2, player->p.y - collision_dim.y / 2};
        player->vertices_relative_to_p.arr[1] = (V2){player->p.x + collision_dim.x / 2, player->p.y - collision_dim.y / 2};
        player->vertices_relative_to_p.arr[2] = (V2){player->p.x + collision_dim.x / 2, player->p.y + collision_dim.y / 2};
        player->vertices_relative_to_p.arr[3] = (V2){player->p.x - collision_dim.x / 2, player->p.y + collision_dim.y / 2};
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
            rot_wall->collision_rotation = rot_wall->sprite_rotation = 0.125f;
        }
        {
            float px_x = sprite_ref->tex->w;
            float px_y = sprite_ref->tex->h;
            float scale = 0.035f;
            Object *ref_wall = Object_Wall(app, (V2){0, off*0.5f}, (V2){px_x*scale, px_y*scale});
            ref_wall->color = ColorF_RGBA(1,1,1,1);
            ref_wall->collision_rotation = ref_wall->sprite_rotation = -0.125f;
            ref_wall->sprite_id = Sprite_IdFromPointer(app, sprite_ref);
        }
        {
            float px_x = sprite_crate->tex->w;
            float px_y = sprite_crate->tex->h;
            float scale = 0.015f;
            Object *crate_wall = Object_Wall(app, (V2){0, -off*0.5f},
                                             (V2){px_x*scale, px_y*scale});


            crate_wall->collision_rotation = 0.125f;
            crate_wall->color = ColorF_RGBA(1,1,1,1);
            crate_wall->sprite_id = Sprite_IdFromPointer(app, sprite_crate);
        }
        {
            Object *rot_wall = Object_Wall(app, (V2){off,-off*2.f}, (V2){length*0.5f, thickness});
            rot_wall->collision_rotation = rot_wall->sprite_rotation = 0.125f;
            app->special_wall = Object_IdFromPointer(app, rot_wall);
        }
    }

    // initialize normals (for collision) and vertices (for collision & drawing)
    ForU32(obj_id, app->object_count)
    {
        Object *obj = app->object_pool + obj_id;
        Object_UpdateCollisionVerticesAndNormals(obj);
    }
}
