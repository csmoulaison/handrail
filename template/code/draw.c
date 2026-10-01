// A minimal renderer front end, #included into game.c. It's yours to grow:
// add instance kinds here and in the shaders as the game needs them.
//
// handrail's renderer (handrail/code/handrail/render.h) knows nothing about
// what is drawn. Each frame the game writes three things straight into GPU
// memory, through the RenderFrame it's handed:
// - globals:   one DrawGlobals, which every shader invocation reads.
// - instances: one DrawInstance per thing drawn.
// - commands:  one RenderDrawCommand per instance, saying how many vertices
//   to run and which instance they belong to.
// The backend then draws all of a pass's commands with one indirect draw, and
// code/shaders/draw.vert and draw.frag turn each instance into pixels.
//
// Here, every draw call is a quad (6 vertices, two triangles) in one alpha
// blended pass, in screen pixels with the origin at the bottom left.
//
// DrawGlobals, DrawInstance, and DrawInstanceKind are mirrored in
// code/shaders/draw.glsl. Change them together.

#define DRAW_QUAD_VERTICES_LEN 6
#define DRAW_MAX_INSTANCES     (RENDER_FRAME_INSTANCES_SIZE / sizeof(DrawInstance))

typedef enum {
    DRAW_INSTANCE_KIND_QUAD,  // A textured rectangle
    DRAW_INSTANCE_KIND_GLYPH  // One glyph of a vector font
} DrawInstanceKind;

typedef struct {
    v2 screen_size; // In pixels
} DrawGlobals;

typedef struct {
    v2  position; // Quads: bottom left corner. Glyphs: baseline origin.
    v2  size;     // Quads: width and height. Glyphs: pixels per em.
    v4  color;    // Multiplies the texture, or colors the glyph
    u32 kind;     // DrawInstanceKind
    u32 index;    // Quads: TEXTURE handle. Glyphs: index in FONT_VECTOR_GLYPHS.
} DrawInstance;

// One frame's drawing. Draw calls write instances and commands straight
// into the frame, so there is nothing to build at the end.
typedef struct {
    RenderFrame* frame;
    u32          instances_len;
    Stack*       frame_stack; // Scratch for text layout, cleared every frame
} Draw;

// Start a frame's drawing: write the globals and clear the screen to clear_color.
void draw_init(Draw* draw, RenderFrame* frame, Stack* frame_stack, iv2 screen_size, v4 clear_color) {
    draw->frame         = frame;
    draw->instances_len = 0;
    draw->frame_stack   = frame_stack;

    frame->clear_color = clear_color;
    DrawGlobals* globals = (DrawGlobals*)frame->globals;
    globals->screen_size = v2_new((f32)screen_size.x, (f32)screen_size.y);
}

// Add one instance, and the command that draws it as a quad in pass 0.
void draw_instance(Draw* draw, DrawInstanceKind kind, v2 position, v2 size, v4 color, u32 index) {
    RenderFrame* frame = draw->frame;
    assert(draw->instances_len < DRAW_MAX_INSTANCES);
    assert(frame->commands_len[0] < RENDER_FRAME_MAX_COMMANDS);

    DrawInstance* instance = &((DrawInstance*)frame->instances)[draw->instances_len];
    instance->position = position;
    instance->size     = size;
    instance->color    = color;
    instance->kind     = kind;
    instance->index    = index;

    // first_instance is what the shaders read as gl_InstanceIndex
    frame->commands[0][frame->commands_len[0]++] = (RenderDrawCommand){
        .vertex_count   = DRAW_QUAD_VERTICES_LEN,
        .instance_count = 1,
        .first_vertex   = 0,
        .first_instance = draw->instances_len,
    };
    draw->instances_len++;
}

// Draw a whole texture stretched over a rectangle, tinted by color.
void draw_quad(Draw* draw, v2 position, v2 size, u32 texture, v4 color) {
    draw_instance(draw, DRAW_INSTANCE_KIND_QUAD, position, size, color, texture);
}

// Draw text with its first baseline starting at position, at size pixels to
// the em. Glyphs are drawn from their outlines, so text is sharp at any size.
void draw_text(Draw* draw, FontVectorData* font, String text, v2 position, f32 size, v4 color) {
    // Lay the text out into one placement per glyph (handrail/code/handrail/media/font.h)
    FontVectorPlacement* placements = (FontVectorPlacement*)stack_alloc(draw->frame_stack, text.len * sizeof(FontVectorPlacement));
    u32 placements_len = font_vector_layout(font, text, position, size, placements);
    for(i32 i = 0; i < placements_len; i++) {
        draw_instance(draw, DRAW_INSTANCE_KIND_GLYPH, placements[i].origin, v2_new(size, size), color, placements[i].glyph);
    }
}
