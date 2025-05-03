// ---
// Constants
// ---
#define ScaleMetersPerPixel 0.025f
#define TIME_STEP (1.f / 128.f)

#define NET_DEFAULT_SEVER_PORT 21037
#define NET_MAGIC_VALUE 0xfda0'dead'beef'1234llu
#define NET_MAX_TICK_HISTORY 4096

typedef enum {
    Axis2_X,
    Axis2_Y,
    Axis2_COUNT
} Axis2;

static Axis2 Axis2_Other(Axis2 axis)
{
    return (axis == Axis2_X ? Axis2_Y : Axis2_X);
}

typedef struct
{
    SDL_Texture *tex;
    Uint32 frame_count;
} Sprite;

typedef enum {
    ObjectFlag_Draw          = (1 << 0),
    ObjectFlag_Move          = (1 << 1),
    ObjectFlag_Collide       = (1 << 2),
} ObjectFlags;

typedef struct
{
    V2 arr[4];
} Vertices, Normals;
typedef union
{
    V2 arr[2];
    V2 A;
    V2 B;
} Line;

typedef struct
{
    Uint32 flags;
    V2 p; // position of center
    V2 dp; // change of p
    V2 prev_p; // position from the last frame

    float collision_rotation;
    // calculated after applying rotation
    Uint8 num_vertices; // @todo(poleonek) allow other numbers than 4
    Vertices vertices_relative_to_p; // constant after init
    Normals collision_normals; // right, top, left, bottom
    Vertices collision_vertices; // bottom-left, bottom-right, top-left, top-right

    // visuals
    ColorF color;
    Uint32 sprite_id;
    float sprite_rotation;
    float sprite_scale;
    bool dirty_sprite_vertices;
    Vertices sprite_vertices;

    float sprite_animation_t;
    Uint32 sprite_animation_index;
    Uint32 sprite_frame_index;

    // temp
    bool has_collision;
} Object;

typedef struct
{
    SDLNet_Address *address;
    Uint16 port;
} Net_User;

typedef struct
{
    V2 move_dir;
    // action buttons etc will be added here
} TickInput;

typedef struct
{
    // SDL, window stuff
    SDL_Window* window;
    SDL_Renderer* renderer;
    int window_width, window_height;
    int window_px, window_py; // initial window px, py specified by cmd options, 0 if wasn't set
    bool window_on_top;
    bool window_borderless;

    // user input
    V2 mouse;
    SDL_MouseButtonFlags mouse_keys;
    bool keyboard[SDL_SCANCODE_COUNT]; // true == key is down

    // circular buffer with tick inputs
    TickInput tick_input_buf[NET_MAX_TICK_HISTORY];
    Uint64 tick_input_min;
    Uint64 tick_input_max; // one past last

    // time
    Uint64 frame_id;
    Uint64 frame_time;
    float dt;
    Uint64 tick_id;
    float tick_dt_accumulator;

    // objects
    Object object_pool[4096];
    Uint32 object_count;
    Uint32 player_ids[2];
    Uint32 special_wall;

    // sprites
    Sprite sprite_pool[32];
    Uint32 sprite_count;
    Uint32 sprite_overlay_id;

    // camera
    V2 camera_p;
    // :: camera_range ::
    // how much of the world is visible in the camera
    // float camera_scale = Max(width, height) / camera_range
    float camera_range;

    // networking
    struct
    {
        bool err; // tracks if network is in error state
        bool is_server;
        SDLNet_DatagramSocket *socket;

        Uint8 buf[1024 * 1024 * 1]; // 1 MB scratch buffer for network payload construction
        Uint32 buf_used;
        bool buf_err; // true on overflows

        struct
        {
            Net_User users[16];
            Uint32 user_count;
        } server;
        struct
        {
            SDLNet_Address *server_address;
            Uint16 server_port;
        } client;
    } net;

    // debug
    struct
    {
        float fixed_dt;
        bool pause_on_every_frame;
        bool paused_frame;

        bool draw_collision_box;
        float collision_sprite_animation_t;
        Uint32 collision_sprite_frame_index;

        bool draw_texture_box;
    } debug;
} AppState;
