#ifndef handrail_ui_h_INCLUDED
#define handrail_ui_h_INCLUDED

// Immediate-mode UI layout. Each frame, ui_begin makes the screen the root
// panel, and every element is placed inside the innermost open panel by a
// UiPlace. Panels nest with ui_panel_begin / ui_panel_end and draw nothing
// themselves. Everything is in screen pixels with the origin bottom left.
//
// The UI doesn't draw. Elements append UiPrimitives (solid rects, sprites, and
// vector glyphs) to a list in Ui, in painter's order, and after ui_end the game
// walks the list and draws each one however its renderer likes.
//
// The UI keeps nothing between frames. What must persist, such as window
// rects, belongs to the caller and is passed in every frame.

#ifndef UI_MAX_DEPTH
#define UI_MAX_DEPTH 16
#endif

// Primitives one frame of UI can emit. The debug frame graph alone draws a few
// thousand rects.
#ifndef UI_MAX_PRIMITIVES
#define UI_MAX_PRIMITIVES 16384
#endif

#ifndef UI_WINDOW_TITLE_HEIGHT
#define UI_WINDOW_TITLE_HEIGHT 24.0f
#endif

// Window chrome colors. Overridable so a game can theme windows without
// passing a style to every call.
#ifndef UI_WINDOW_BACKGROUND_COLOR
#define UI_WINDOW_BACKGROUND_COLOR   v4_new(0.05f, 0.05f, 0.08f, 0.85f)
#endif
#ifndef UI_WINDOW_TITLE_COLOR
#define UI_WINDOW_TITLE_COLOR        v4_new(0.2f, 0.2f, 0.28f, 0.95f)
#endif
#ifndef UI_WINDOW_TITLE_ACTIVE_COLOR
#define UI_WINDOW_TITLE_ACTIVE_COLOR v4_new(0.3f, 0.3f, 0.4f, 0.95f)
#endif
#ifndef UI_WINDOW_TEXT_COLOR
#define UI_WINDOW_TEXT_COLOR         v4_new(1.0f, 1.0f, 1.0f, 1.0f)
#endif
#ifndef UI_WINDOW_BOX_COLOR
#define UI_WINDOW_BOX_COLOR          v4_new(0.6f, 0.6f, 0.7f, 1.0f)
#endif
#ifndef UI_WINDOW_BOX_ACTIVE_COLOR
#define UI_WINDOW_BOX_ACTIVE_COLOR   v4_new(1.0f, 1.0f, 1.0f, 1.0f)
#endif

// The smallest a window can be on each axis, whatever its own min_size
#define UI_WINDOW_MIN_SIZE   v2_new(120.0f, UI_WINDOW_TITLE_HEIGHT + 40.0f)
#define UI_WINDOW_RESIZE_BOX 12.0f

typedef struct {
    v2 min;
    v2 max;
} UiRect;

// Where an element sits in its parent panel. The anchors are normalized points
// in the parent ((0,0) bottom left, (1,1) top right), and the offsets are pixels
// added to the corners they land on:
//   rect.min = parent.min + parent.size * anchor_min + offset_min
//   rect.max = parent.min + parent.size * anchor_max + offset_max
// Equal anchors give a fixed size (see ui_place_at). Differing anchors stretch
// the element with its parent (see ui_place_fill), separately on each axis.
typedef struct {
    v2 anchor_min;
    v2 anchor_max;
    v2 offset_min;
    v2 offset_max;
} UiPlace;

// Mouse input for one frame, in screen pixels. Built from platform events by
// ui_input_from_platform, or by the caller.
typedef struct {
    v2   mouse;          // Current position
    v2   press_position; // Where the left button went down this frame
    bool left_pressed;   // The left button went down this frame
    bool left_released;  // The left button went up this frame, or focus was lost
    v2   scroll;         // Wheel notches this frame, positive y away from the user
} UiInput;

typedef enum {
    UI_PRIMITIVE_RECT,
    UI_PRIMITIVE_SPRITE,
    UI_PRIMITIVE_GLYPH
} UiPrimitiveKind;

// One thing to draw, in screen pixels with the origin bottom left. Primitives
// are in painter's order: each draws over the ones before it.
typedef struct {
    UiPrimitiveKind kind;
    v4              color;
    union {
        // UI_PRIMITIVE_RECT: a solid rect
        UiRect rect;
        // UI_PRIMITIVE_SPRITE: image stretched over rect. image is the game's
        // own handle, passed through untouched. uv_rect is x, y, w, h within it.
        struct {
            UiRect rect;
            v4     uv_rect;
            u32    image;
        } sprite;
        // UI_PRIMITIVE_GLYPH: a vector glyph with its baseline origin at
        // origin, size pixels to the em
        struct {
            v2  origin;
            f32 size;
            u32 glyph; // Index into FONT_VECTOR_GLYPHS
        } glyph;
    };
} UiPrimitive;

// One frame of UI. Lives for a frame: ui_begin allocates its primitive list
// from the frame stack.
typedef struct {
    UiInput      input;
    UiRect       panels[UI_MAX_DEPTH]; // panels[0] is the screen
    u32          panels_len;
    UiPrimitive* primitives;           // UI_MAX_PRIMITIVES long
    u32          primitives_len;
    Stack*       frame_stack;          // For per-call scratch, such as text layout
} Ui;

// A titled window, drawn as a background and a title bar with a content panel
// below it. Dragging the title bar moves the window, and dragging the resize box
// at the right of the title bar moves its top right corner.
//
// The caller owns what must persist (each window's rect, the order windows are
// drawn in, and the one drag in progress) and passes it in every frame;
// ui_windows_input is the only function that changes it. Size limits are
// configuration, passed in rather than stored.
typedef struct {
    v2 position; // Top left corner, in screen pixels (origin bottom left)
    v2 size;     // Including the title bar
} UiWindow;

typedef struct {
    v2 min_size; // Per axis. Equal min and max lock an axis.
    v2 max_size; // Per axis, 0 for no limit
} UiWindowLimits;

typedef enum {
    UI_WINDOW_DRAG_NONE,
    UI_WINDOW_DRAG_MOVE,
    UI_WINDOW_DRAG_RESIZE
} UiWindowDragMode;

// The drag in progress. There is one mouse, so at most one window is dragged.
// Zeroed means no drag.
typedef struct {
    UiWindowDragMode mode;
    i32              window; // Index of the dragged window
    v2               offset; // Mouse position minus the dragged corner
} UiWindowDrag;

// Placement
// A fixed-size element whose pivot (a normalized point on the element) sits
// offset pixels from anchor (a normalized point in the parent).
UiPlace ui_place_at(v2 anchor, v2 pivot, v2 offset, v2 size);
// Fill the parent, inset by margin pixels on every side.
UiPlace ui_place_fill(f32 margin);
// Resolve a place against the innermost open panel.
UiRect  ui_resolve(Ui* ui, UiPlace place);
bool    ui_rect_contains(UiRect rect, v2 point);

// Frame. ui_begin allocates the primitive list from frame_stack, which must
// outlive the game's use of it (platform->frame_stack does).
void    ui_begin(Ui* ui, Stack* frame_stack, iv2 screen_size, UiInput input);
void    ui_end(Ui* ui);
// Mouse input for one frame, from the platform's events. Optional: a game that
// filters or remaps input builds UiInput itself.
UiInput ui_input_from_platform(Platform* platform);

// Panels. A panel draws nothing; give it a background with
// ui_rect(ui, ui_place_fill(0), color).
UiRect  ui_panel_begin(Ui* ui, UiPlace place);
void    ui_panel_end(Ui* ui);

// Elements, placed in the innermost open panel
UiRect  ui_rect(Ui* ui, UiPlace place, v4 color);
UiRect  ui_sprite(Ui* ui, UiPlace place, u32 image, v4 uv_rect, v4 color);
// Text is placed by its line box (see font_vector_measure), so it takes a
// point anchor and pivot rather than a UiPlace.
UiRect  ui_text(Ui* ui, FontVectorData* font, String text, v2 anchor, v2 pivot, v2 offset, f32 size, v4 color);
// A solid rect at an absolute screen rect, ignoring panels. For drawing that
// computes its own geometry, such as graphs.
void    ui_push_rect(Ui* ui, UiRect rect, v4 color);

// Windows
// Clamp a size to a window's limits.
v2      ui_window_size_clamp(UiWindowLimits limits, v2 size);
// Apply this frame's mouse input to windows. limits holds each window's size
// limits, and order holds indices into windows, back to front. A left press
// goes to the frontmost window under it, which comes to the front: on the
// resize box it starts a resize, elsewhere on the title bar a move. Returns
// true if a window took the press.
bool    ui_windows_input(Ui* ui, UiWindow* windows, const UiWindowLimits* limits, i32* order, UiWindowDrag* drag, i32 windows_len);
// The frontmost window under the mouse, or -1. order holds indices into
// windows, back to front.
i32     ui_windows_hovered(Ui* ui, UiWindow* windows, i32* order, i32 windows_len);
// Open a window: elements placed until the matching ui_window_end are inside
// its content area. Placed on the screen, whatever panel is open. drag_mode is
// how this window is being dragged, which it's highlighted by.
void    ui_window_begin(Ui* ui, UiWindow* window, String title, UiWindowDragMode drag_mode, FontVectorData* font);
void    ui_window_end(Ui* ui);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_ui_h_IMPLEMENTED)
#define handrail_ui_h_IMPLEMENTED

UiPlace ui_place_at(v2 anchor, v2 pivot, v2 offset, v2 size) {
    v2 offset_min = v2_sub(offset, v2_mult(size, pivot));
    return (UiPlace){
        .anchor_min = anchor,
        .anchor_max = anchor,
        .offset_min = offset_min,
        .offset_max = v2_add(offset_min, size),
    };
}

UiPlace ui_place_fill(f32 margin) {
    return (UiPlace){
        .anchor_min = v2_new(0.0f, 0.0f),
        .anchor_max = v2_new(1.0f, 1.0f),
        .offset_min = v2_new(margin, margin),
        .offset_max = v2_new(-margin, -margin),
    };
}

UiRect ui_resolve(Ui* ui, UiPlace place) {
    UiRect parent = ui->panels[ui->panels_len - 1];
    v2 parent_size = v2_sub(parent.max, parent.min);
    return (UiRect){
        .min = v2_add(v2_add(parent.min, v2_mult(parent_size, place.anchor_min)), place.offset_min),
        .max = v2_add(v2_add(parent.min, v2_mult(parent_size, place.anchor_max)), place.offset_max),
    };
}

bool ui_rect_contains(UiRect rect, v2 point) {
    return point.x >= rect.min.x && point.x < rect.max.x && point.y >= rect.min.y && point.y < rect.max.y;
}

void ui_begin(Ui* ui, Stack* frame_stack, iv2 screen_size, UiInput input) {
    ui->input = input;
    ui->panels[0] = (UiRect){ v2_zero(), v2_from_iv2(screen_size) };
    ui->panels_len = 1;
    ui->primitives = (UiPrimitive*)stack_alloc_labeled(frame_stack, UI_MAX_PRIMITIVES * sizeof(UiPrimitive), string_const("UiPrimitives")).memory;
    ui->primitives_len = 0;
    ui->frame_stack = frame_stack;
}

void ui_end(Ui* ui) {
    assert(ui->panels_len == 1);
}

UiInput ui_input_from_platform(Platform* platform) {
    UiInput input = { .mouse = v2_from_iv2(platform->mouse_position) };
    for(i32 i = 0; i < platform->events_len; i++) {
        PlatformEvent* event = &platform->events[i];
        if(event->type == PLATFORM_EVENT_MOUSE_DOWN && event->mouse.button == PLATFORM_MOUSE_BUTTON_LEFT) {
            input.left_pressed = true;
            input.press_position = v2_from_iv2(event->mouse.position);
        }
        if(event->type == PLATFORM_EVENT_MOUSE_SCROLL) {
            input.scroll = v2_add(input.scroll, event->scroll.delta);
        }
        if((event->type == PLATFORM_EVENT_MOUSE_UP && event->mouse.button == PLATFORM_MOUSE_BUTTON_LEFT)
        || event->type == PLATFORM_EVENT_DEFOCUS) {
            input.left_released = true;
        }
    }
    return input;
}

UiRect ui_panel_begin(Ui* ui, UiPlace place) {
    assert(ui->panels_len < UI_MAX_DEPTH);
    UiRect rect = ui_resolve(ui, place);
    ui->panels[ui->panels_len++] = rect;
    return rect;
}

void ui_panel_end(Ui* ui) {
    assert(ui->panels_len > 1);
    ui->panels_len--;
}

void ui_push_rect(Ui* ui, UiRect rect, v4 color) {
    assert(ui->primitives_len < UI_MAX_PRIMITIVES);
    ui->primitives[ui->primitives_len++] = (UiPrimitive){ .kind = UI_PRIMITIVE_RECT, .color = color, .rect = rect };
}

UiRect ui_rect(Ui* ui, UiPlace place, v4 color) {
    UiRect rect = ui_resolve(ui, place);
    ui_push_rect(ui, rect, color);
    return rect;
}

UiRect ui_sprite(Ui* ui, UiPlace place, u32 image, v4 uv_rect, v4 color) {
    UiRect rect = ui_resolve(ui, place);
    assert(ui->primitives_len < UI_MAX_PRIMITIVES);
    UiPrimitive* primitive = &ui->primitives[ui->primitives_len++];
    *primitive = (UiPrimitive){ .kind = UI_PRIMITIVE_SPRITE, .color = color };
    primitive->sprite.rect    = rect;
    primitive->sprite.uv_rect = uv_rect;
    primitive->sprite.image   = image;
    return rect;
}

UiRect ui_text(Ui* ui, FontVectorData* font, String text, v2 anchor, v2 pivot, v2 offset, f32 size, v4 color) {
    UiRect rect = ui_resolve(ui, ui_place_at(anchor, pivot, offset, font_vector_measure(font, text, size)));

    // Lay out from the top of the line box, then emit a glyph per placement
    v2 origin = v2_new(rect.min.x, rect.max.y - font->ascender * size);
    FontVectorPlacement* placements = (FontVectorPlacement*)stack_alloc(ui->frame_stack, text.len * sizeof(FontVectorPlacement));
    u32 placements_len = font_vector_layout(font, text, origin, size, placements);
    assert(ui->primitives_len + placements_len <= UI_MAX_PRIMITIVES);
    for(i32 i = 0; i < placements_len; i++) {
        UiPrimitive* primitive = &ui->primitives[ui->primitives_len++];
        *primitive = (UiPrimitive){ .kind = UI_PRIMITIVE_GLYPH, .color = color };
        primitive->glyph.origin = placements[i].origin;
        primitive->glyph.size   = size;
        primitive->glyph.glyph  = placements[i].glyph;
    }
    return rect;
}

v2 ui_window_size_clamp(UiWindowLimits limits, v2 size) {
    v2 min_size = v2_max(limits.min_size, UI_WINDOW_MIN_SIZE);
    for(i32 i = 0; i < 2; i++) {
        size.comps[i] = f32_max(size.comps[i], min_size.comps[i]);
        if(limits.max_size.comps[i] > 0.0f) {
            size.comps[i] = f32_min(size.comps[i], f32_max(limits.max_size.comps[i], min_size.comps[i]));
        }
    }
    return size;
}

// The top left corner that keeps a window of this size on screen, preferring
// its top left corner when the screen is too small.
static v2 ui_window_clamp_position(Ui* ui, v2 position, v2 size) {
    v2 screen = ui->panels[0].max;
    v2 top_left = v2_new(f32_min(position.x, screen.x - size.x), f32_max(position.y, size.y));
    return v2_new(f32_max(top_left.x, 0.0f), f32_min(top_left.y, screen.y));
}

// Where the window is drawn: its stored rect, kept on screen.
static UiRect ui_window_rect(Ui* ui, UiWindow* window) {
    v2 top_left = ui_window_clamp_position(ui, window->position, window->size);
    return (UiRect){ v2_new(top_left.x, top_left.y - window->size.y), v2_new(top_left.x + window->size.x, top_left.y) };
}

static UiRect ui_window_resize_box(UiRect rect) {
    v2 max = v2_new(rect.max.x - 6.0f, rect.max.y - (UI_WINDOW_TITLE_HEIGHT - UI_WINDOW_RESIZE_BOX) * 0.5f);
    return (UiRect){ v2_sub(max, v2_new(UI_WINDOW_RESIZE_BOX, UI_WINDOW_RESIZE_BOX)), max };
}

bool ui_windows_input(Ui* ui, UiWindow* windows, const UiWindowLimits* limits, i32* order, UiWindowDrag* drag, i32 windows_len) {
    UiInput* input = &ui->input;
    v2 screen = ui->panels[0].max;

    // Apply size limits, which may have changed, keeping the top left corner in place
    for(i32 i = 0; i < windows_len; i++) {
        windows[i].size = ui_window_size_clamp(limits[i], windows[i].size);
    }

    // Continue the drag
    if(drag->mode != UI_WINDOW_DRAG_NONE) {
        assert(drag->window >= 0 && drag->window < windows_len);
        UiWindow* window = &windows[drag->window];
        if(drag->mode == UI_WINDOW_DRAG_MOVE) {
            window->position = ui_window_clamp_position(ui, v2_sub(input->mouse, drag->offset), window->size);
        } else {
            // The bottom left corner stays where it's drawn
            UiRect rect = ui_window_rect(ui, window);
            v2 top_right = v2_min(v2_sub(input->mouse, drag->offset), screen);
            window->size = ui_window_size_clamp(limits[drag->window], v2_sub(top_right, rect.min));
            window->position = v2_new(rect.min.x, rect.min.y + window->size.y);
        }
    }

    // Start a drag on the frontmost window under a press
    bool taken = false;
    if(input->left_pressed) {
        v2 press = input->press_position;
        for(i32 i = windows_len - 1; i >= 0; i--) {
            UiRect rect = ui_window_rect(ui, &windows[order[i]]);
            if(!ui_rect_contains(rect, press)) continue;

            if(ui_rect_contains(ui_window_resize_box(rect), press)) {
                *drag = (UiWindowDrag){ .mode = UI_WINDOW_DRAG_RESIZE, .window = order[i], .offset = v2_sub(press, rect.max) };
            } else if(press.y >= rect.max.y - UI_WINDOW_TITLE_HEIGHT) {
                *drag = (UiWindowDrag){ .mode = UI_WINDOW_DRAG_MOVE, .window = order[i], .offset = v2_sub(press, v2_new(rect.min.x, rect.max.y)) };
            }

            // Bring it to the front
            i32 index = order[i];
            for(i32 j = i; j < windows_len - 1; j++) {
                order[j] = order[j + 1];
            }
            order[windows_len - 1] = index;
            taken = true;
            break;
        }
    }

    if(input->left_released) {
        *drag = (UiWindowDrag){};
    }
    return taken;
}

i32 ui_windows_hovered(Ui* ui, UiWindow* windows, i32* order, i32 windows_len) {
    for(i32 i = windows_len - 1; i >= 0; i--) {
        if(ui_rect_contains(ui_window_rect(ui, &windows[order[i]]), ui->input.mouse)) {
            return order[i];
        }
    }
    return -1;
}

void ui_window_begin(Ui* ui, UiWindow* window, String title, UiWindowDragMode drag_mode, FontVectorData* font) {
    // Window rect, as a panel directly in the screen
    UiRect rect = ui_window_rect(ui, window);
    assert(ui->panels_len + 1 < UI_MAX_DEPTH);
    ui->panels[ui->panels_len++] = rect;
    ui_rect(ui, ui_place_fill(0.0f), UI_WINDOW_BACKGROUND_COLOR);

    // Title bar, lighter while it's being dragged
    v4 title_color = drag_mode == UI_WINDOW_DRAG_MOVE ? UI_WINDOW_TITLE_ACTIVE_COLOR : UI_WINDOW_TITLE_COLOR;
    ui_rect(ui, (UiPlace){ .anchor_min = v2_new(0.0f, 1.0f), .anchor_max = v2_new(1.0f, 1.0f),
                           .offset_min = v2_new(0.0f, -UI_WINDOW_TITLE_HEIGHT), .offset_max = v2_zero() },
            title_color);
    ui_text(ui, font, title, v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f), v2_new(8.0f, -UI_WINDOW_TITLE_HEIGHT * 0.5f),
            UI_WINDOW_TITLE_HEIGHT * 0.7f, UI_WINDOW_TEXT_COLOR);

    // Resize box outline, brighter while hovered or dragged
    UiRect box = ui_window_resize_box(rect);
    bool box_active = drag_mode == UI_WINDOW_DRAG_RESIZE || (drag_mode == UI_WINDOW_DRAG_NONE && ui_rect_contains(box, ui->input.mouse));
    v4 box_color = box_active ? UI_WINDOW_BOX_ACTIVE_COLOR : UI_WINDOW_BOX_COLOR;
    ui_push_rect(ui, (UiRect){ box.min, v2_new(box.max.x, box.min.y + 1.0f) }, box_color);
    ui_push_rect(ui, (UiRect){ v2_new(box.min.x, box.max.y - 1.0f), box.max }, box_color);
    ui_push_rect(ui, (UiRect){ box.min, v2_new(box.min.x + 1.0f, box.max.y) }, box_color);
    ui_push_rect(ui, (UiRect){ v2_new(box.max.x - 1.0f, box.min.y), box.max }, box_color);

    // Content area below the title bar
    ui_panel_begin(ui, (UiPlace){ .anchor_min = v2_zero(), .anchor_max = v2_new(1.0f, 1.0f),
                                  .offset_min = v2_zero(), .offset_max = v2_new(0.0f, -UI_WINDOW_TITLE_HEIGHT) });
}

void ui_window_end(Ui* ui) {
    ui_panel_end(ui);
    ui_panel_end(ui);
}

#endif
