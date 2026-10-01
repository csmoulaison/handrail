#ifndef handrail_debug_view_h_INCLUDED
#define handrail_debug_view_h_INCLUDED

// Debug views drawn with ui.h: a table of frame times, a frame graph, and a
// tree of tracked memory. Each view draws one window's contents into the
// innermost open panel. Which windows exist, where they sit, how they're
// toggled, and which timers they show are the game's to compose.

#ifndef DEBUG_VIEW_TEXT_SIZE
#define DEBUG_VIEW_TEXT_SIZE 16.0f
#endif
#ifndef DEBUG_VIEW_ROW_HEIGHT
#define DEBUG_VIEW_ROW_HEIGHT 22.0f
#endif
// The times view shows the mean of the most recent complete block of this
// many frames of profile history (0.2 s at 60 Hz), so values hold steady
#ifndef DEBUG_VIEW_AVERAGE_FRAMES
#define DEBUG_VIEW_AVERAGE_FRAMES 12
#endif
// The graph's vertical scale never goes below this (two 60 Hz frames)
#ifndef DEBUG_VIEW_GRAPH_SCALE_MIN_NS
#define DEBUG_VIEW_GRAPH_SCALE_MIN_NS 33333333ull
#endif
// Memory view: how many rows can be open at once
#ifndef DEBUG_VIEW_MEMORY_EXPANDED_MAX
#define DEBUG_VIEW_MEMORY_EXPANDED_MAX 64
#endif

#define DEBUG_VIEW_GRAPH_LEGEND_HEIGHT 24.0f
#define DEBUG_VIEW_GRAPH_LINE_WIDTH    1.0f
// Inset of the graph's plot from its panel's sides
#define DEBUG_VIEW_GRAPH_PADDING       8.0f
// Content width at which the graph draws one frame per pixel. Use it to lock
// the graph window's width with UiWindowLimits.
#define DEBUG_VIEW_GRAPH_WIDTH         (PROFILE_HISTORY_LEN + 2.0f * DEBUG_VIEW_GRAPH_PADDING)
// Memory view: indent per tree level, and rows scrolled per wheel notch
#define DEBUG_VIEW_MEMORY_INDENT       14.0f
#define DEBUG_VIEW_MEMORY_SCROLL_ROWS  3.0f

// A profile timer to show, and its color. Its name comes from profile_name.
typedef struct {
    u64 timer;
    v4  color;
} DebugViewTimer;

// The engine's own timers, as an initializer for a game's DebugViewTimer array:
//   DebugViewTimer timers[] = { DEBUG_VIEW_ENGINE_TIMERS, { PROFILE_GAME_SIM, ... } };
// A macro rather than a static table, so translation units that don't use it
// don't warn.
#define DEBUG_VIEW_ENGINE_TIMERS \
    { PROFILE_FRAME,        { .x = 0.90f, .y = 0.90f, .z = 0.90f, .w = 1.0f } }, \
    { PROFILE_GAME_UPDATE,  { .x = 0.35f, .y = 0.80f, .z = 0.40f, .w = 1.0f } }, \
    { PROFILE_RENDER_END,   { .x = 0.95f, .y = 0.60f, .z = 0.20f, .w = 1.0f } }, \
    { PROFILE_RENDER_BEGIN, { .x = 0.30f, .y = 0.45f, .z = 0.85f, .w = 1.0f } }, \
    { PROFILE_GPU,          { .x = 0.95f, .y = 0.30f, .z = 0.65f, .w = 1.0f } }

// A tracked buffer, identified across frames by its range. Memory alone isn't
// enough: a stack's first allocation starts where the stack does.
typedef struct {
    u8* memory;
    u64 size;
} DebugViewMemoryKey;

// What the memory view keeps between frames. Zeroed is all rows closed,
// scrolled to the top.
typedef struct {
    DebugViewMemoryKey expanded[DEBUG_VIEW_MEMORY_EXPANDED_MAX];
    i32                expanded_len;
    f32                scroll; // First row in view, fractional so smooth wheels add up
} DebugViewMemory;

// A row per timer: color swatch, name, and mean time over the latest block of frames
void debug_view_times(Ui* ui, FontVectorData* font, const DebugViewTimer* timers, i32 timers_len);
// A legend, then a line per timer across the profile history, a column per
// frame, scaled to the slowest frame
void debug_view_graph(Ui* ui, FontVectorData* font, const DebugViewTimer* timers, i32 timers_len);
// A tree of tracked buffers, a row each. The wheel scrolls it when hovered,
// and a click on a row with suballocations opens or closes it when
// interactive (for example, when its window is in front).
void debug_view_memory(DebugViewMemory* view, Ui* ui, FontVectorData* font, BufferTracker* buffers, bool hovered, bool interactive);

#endif

#if defined(HANDRAIL_IMPLEMENTATION_PASS) && !defined(handrail_debug_view_h_IMPLEMENTED)
#define handrail_debug_view_h_IMPLEMENTED

// Format a byte count with a decimal unit, matching KILOBYTE and MEGABYTE
static void debug_view_format_bytes(char* out, usize out_size, u64 bytes) {
    if(bytes >= MEGABYTE) {
        snprintf(out, out_size, "%.1f MB", (f64)bytes / MEGABYTE);
    } else if(bytes >= KILOBYTE) {
        snprintf(out, out_size, "%.1f KB", (f64)bytes / KILOBYTE);
    } else {
        snprintf(out, out_size, "%" PRIu64 " B", bytes);
    }
}

void debug_view_times(Ui* ui, FontVectorData* font, const DebugViewTimer* timers, i32 timers_len) {
    v4 white = v4_new(1.0f, 1.0f, 1.0f, 1.0f);
    // The latest complete block starts this many frames ago, so it
    // moves forward only every DEBUG_VIEW_AVERAGE_FRAMES frames
    u32 block_start = (u32)(profile_frames_recorded() % DEBUG_VIEW_AVERAGE_FRAMES);
    for(i32 i = 0; i < timers_len; i++) {
        u64 sum_ns = 0;
        for(u32 j = 0; j < DEBUG_VIEW_AVERAGE_FRAMES; j++) {
            sum_ns += profile_history_ns(timers[i].timer, block_start + j);
        }
        f64 average_ms = (f64)sum_ns / DEBUG_VIEW_AVERAGE_FRAMES / 1e6;

        f32 y = -8.0f - i * DEBUG_VIEW_ROW_HEIGHT;
        ui_rect(ui, ui_place_at(v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f), v2_new(8.0f, y - DEBUG_VIEW_ROW_HEIGHT * 0.5f), v2_new(10.0f, 10.0f)),
                timers[i].color);
        ui_text(ui, font, profile_name(timers[i].timer), v2_new(0.0f, 1.0f), v2_new(0.0f, 1.0f), v2_new(26.0f, y), DEBUG_VIEW_TEXT_SIZE, white);
        char value[32];
        snprintf(value, sizeof(value), "%.2f ms", average_ms);
        ui_text(ui, font, string_const(value), v2_new(1.0f, 1.0f), v2_new(1.0f, 1.0f), v2_new(-10.0f, y), DEBUG_VIEW_TEXT_SIZE, white);
    }
}

void debug_view_graph(Ui* ui, FontVectorData* font, const DebugViewTimer* timers, i32 timers_len) {
    v4 white = v4_new(1.0f, 1.0f, 1.0f, 1.0f);
    // Legend
    f32 legend_x = 8.0f;
    for(i32 i = 0; i < timers_len; i++) {
        ui_rect(ui, ui_place_at(v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f), v2_new(legend_x, -DEBUG_VIEW_GRAPH_LEGEND_HEIGHT * 0.5f), v2_new(8.0f, 8.0f)),
                timers[i].color);
        UiRect text = ui_text(ui, font, profile_name(timers[i].timer), v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f),
                              v2_new(legend_x + 12.0f, -DEBUG_VIEW_GRAPH_LEGEND_HEIGHT * 0.5f), DEBUG_VIEW_TEXT_SIZE * 0.8f, white);
        legend_x += 12.0f + (text.max.x - text.min.x) + 10.0f;
    }

    // Plot area below the legend
    UiRect plot = ui_panel_begin(ui, (UiPlace){ .anchor_min = v2_zero(), .anchor_max = v2_new(1.0f, 1.0f),
                                                .offset_min = v2_new(DEBUG_VIEW_GRAPH_PADDING, DEBUG_VIEW_GRAPH_PADDING),
                                                .offset_max = v2_new(-DEBUG_VIEW_GRAPH_PADDING, -DEBUG_VIEW_GRAPH_LEGEND_HEIGHT) });
    v2 plot_size = v2_sub(plot.max, plot.min);
    ui_rect(ui, ui_place_fill(0.0f), v4_new(0.0f, 0.0f, 0.0f, 0.5f));

    // Scale to the slowest frame in the history, but at least two 60 Hz frames
    u32 filled = profile_frames_recorded() < PROFILE_HISTORY_LEN ? (u32)profile_frames_recorded() : PROFILE_HISTORY_LEN;
    u64 scale_ns = DEBUG_VIEW_GRAPH_SCALE_MIN_NS;
    for(u32 j = 0; j < filled; j++) {
        u64 ns = profile_history_ns(PROFILE_FRAME, j);
        if(ns > scale_ns) {
            scale_ns = ns;
        }
    }
    f32 pixels_per_ns = plot_size.y / (f32)scale_ns;

    // Lines, filling from the left until the history is full, then scrolling.
    // Each column is one pixel and draws a vertical span from the previous
    // frame's value to this one's, so consecutive columns join into a line.
    // Drawn in reverse so the first timer is on top.
    for(i32 i = timers_len - 1; i >= 0; i--) {
        f32 previous_y = 0.0f;
        for(u32 j = 0; j < filled; j++) {
            f32 y = f32_min(profile_history_ns(timers[i].timer, filled - 1 - j) * pixels_per_ns, plot_size.y);
            if(j == 0) {
                previous_y = y;
            }

            f32 bottom = f32_max(f32_min(previous_y, y) - DEBUG_VIEW_GRAPH_LINE_WIDTH * 0.5f, 0.0f);
            f32 top = f32_min(f32_max(previous_y, y) + DEBUG_VIEW_GRAPH_LINE_WIDTH * 0.5f, plot_size.y);
            ui_push_rect(ui, (UiRect){ v2_new(plot.min.x + j, plot.min.y + bottom), v2_new(plot.min.x + j + 1.0f, plot.min.y + top) }, timers[i].color);
            previous_y = y;
        }
    }

    // Reference lines at one and two 60 Hz frames, labeled
    for(i32 i = 1; i <= 2; i++) {
        f32 ms = 1000.0f / 60.0f * i;
        f32 line_y = plot.min.y + ms * 1e6f * pixels_per_ns;
        if(line_y > plot.max.y) continue;
        ui_push_rect(ui, (UiRect){ v2_new(plot.min.x, line_y), v2_new(plot.max.x, line_y + 1.0f) }, v4_new(1.0f, 1.0f, 1.0f, 0.35f));
        char label[16];
        snprintf(label, sizeof(label), "%.1f ms", ms);
        ui_text(ui, font, string_const(label), v2_zero(), v2_new(0.0f, 1.0f), v2_new(4.0f, line_y - plot.min.y - 2.0f),
                DEBUG_VIEW_TEXT_SIZE * 0.75f, v4_new(1.0f, 1.0f, 1.0f, 0.7f));
    }

    ui_panel_end(ui);
}

void debug_view_memory(DebugViewMemory* view, Ui* ui, FontVectorData* font, BufferTracker* buffers, bool hovered, bool interactive) {
    v4 white = v4_new(1.0f, 1.0f, 1.0f, 1.0f);
    BufferRecord* records = buffers->records;
    i32 records_len = buffers->records_len;

    // Each record's parent is the smallest other record containing it. Of two
    // records with the same range, the earlier is the parent. A parent's used
    // size is how far into it its last child ends.
    i32 parents[BUFFER_TRACKER_MAX_RECORDS];
    u64 used[BUFFER_TRACKER_MAX_RECORDS] = {};
    bool has_children[BUFFER_TRACKER_MAX_RECORDS] = {};
    for(i32 i = 0; i < records_len; i++) {
        parents[i] = -1;
        for(i32 j = 0; j < records_len; j++) {
            if(j == i) continue;
            bool contains = records[j].memory <= records[i].memory
                         && records[j].memory + records[j].size >= records[i].memory + records[i].size;
            bool same = records[j].memory == records[i].memory && records[j].size == records[i].size;
            if(!contains || (same && j > i)) continue;
            if(parents[i] == -1 || records[j].size < records[parents[i]].size) {
                parents[i] = j;
            }
        }
        if(parents[i] != -1) {
            i32 parent = parents[i];
            has_children[parent] = true;
            u64 end = (u64)(records[i].memory + records[i].size - records[parent].memory);
            used[parent] = end > used[parent] ? end : used[parent];
        }
    }

    // Forget open rows whose buffer is gone
    i32 kept = 0;
    for(i32 i = 0; i < view->expanded_len; i++) {
        DebugViewMemoryKey key = view->expanded[i];
        for(i32 j = 0; j < records_len; j++) {
            if(records[j].memory == key.memory && records[j].size == key.size) {
                view->expanded[kept++] = key;
                break;
            }
        }
    }
    view->expanded_len = kept;

    // List the visible rows: walk the tree depth first, top level records first.
    // Children are pushed in reverse so they come off in allocation order,
    // which is address order.
    i32 walk_indices[BUFFER_TRACKER_MAX_RECORDS];
    i32 walk_depths[BUFFER_TRACKER_MAX_RECORDS];
    i32 walk_len = 0;
    for(i32 i = records_len - 1; i >= 0; i--) {
        if(parents[i] == -1) {
            walk_indices[walk_len] = i;
            walk_depths[walk_len] = 0;
            walk_len++;
        }
    }
    i32 row_indices[BUFFER_TRACKER_MAX_RECORDS];
    i32 row_depths[BUFFER_TRACKER_MAX_RECORDS];
    bool row_expanded[BUFFER_TRACKER_MAX_RECORDS];
    i32 rows_len = 0;
    while(walk_len > 0) {
        walk_len--;
        i32 index = walk_indices[walk_len];
        i32 depth = walk_depths[walk_len];
        bool expanded = false;
        for(i32 i = 0; i < view->expanded_len; i++) {
            if(view->expanded[i].memory == records[index].memory && view->expanded[i].size == records[index].size) {
                expanded = true;
            }
        }
        row_indices[rows_len] = index;
        row_depths[rows_len] = depth;
        row_expanded[rows_len] = expanded;
        rows_len++;

        if(expanded) {
            for(i32 i = records_len - 1; i >= 0; i--) {
                if(parents[i] == index) {
                    walk_indices[walk_len] = i;
                    walk_depths[walk_len] = depth + 1;
                    walk_len++;
                }
            }
        }
    }

    // Scroll by whole rows, so rows never draw past the content's edges. The
    // dropped count is one more row at the end.
    UiRect content = ui->panels[ui->panels_len - 1];
    i32 total_rows = rows_len + (buffers->dropped > 0 ? 1 : 0);
    i32 visible_rows = (i32)((content.max.y - content.min.y - 4.0f) / DEBUG_VIEW_ROW_HEIGHT);
    i32 max_scroll = total_rows > visible_rows ? total_rows - visible_rows : 0;
    if(hovered) {
        view->scroll -= ui->input.scroll.y * DEBUG_VIEW_MEMORY_SCROLL_ROWS;
    }
    view->scroll = f32_min(f32_max(view->scroll, 0.0f), (f32)max_scroll);
    i32 first_row = (i32)view->scroll;

    // Scroll bar, when the rows overflow
    if(max_scroll > 0) {
        f32 track = content.max.y - content.min.y - 8.0f;
        f32 thumb = track * visible_rows / total_rows;
        f32 thumb_top = content.max.y - 4.0f - (track - thumb) * first_row / max_scroll;
        ui_push_rect(ui, (UiRect){ v2_new(content.max.x - 6.0f, thumb_top - thumb), v2_new(content.max.x - 3.0f, thumb_top) }, v4_new(1.0f, 1.0f, 1.0f, 0.35f));
    }

    // Draw the rows in view
    i32 last_row = first_row + visible_rows < rows_len ? first_row + visible_rows : rows_len;
    f32 y = -4.0f;
    for(i32 row_index = first_row; row_index < last_row; row_index++) {
        i32 index = row_indices[row_index];
        BufferRecord* record = &records[index];
        UiRect row = ui_resolve(ui, (UiPlace){ .anchor_min = v2_new(0.0f, 1.0f), .anchor_max = v2_new(1.0f, 1.0f),
                                               .offset_min = v2_new(0.0f, y - DEBUG_VIEW_ROW_HEIGHT), .offset_max = v2_new(0.0f, y) });

        // Toggle on a click. The rows below change next frame.
        if(has_children[index] && interactive && ui->input.left_pressed && ui_rect_contains(row, ui->input.press_position)) {
            if(row_expanded[row_index]) {
                for(i32 i = 0; i < view->expanded_len; i++) {
                    if(view->expanded[i].memory == record->memory && view->expanded[i].size == record->size) {
                        view->expanded[i] = view->expanded[--view->expanded_len];
                        break;
                    }
                }
            } else if(view->expanded_len < DEBUG_VIEW_MEMORY_EXPANDED_MAX) {
                view->expanded[view->expanded_len++] = (DebugViewMemoryKey){ record->memory, record->size };
            }
        }

        // Usage bar, marker, label, and size
        f32 indent = 8.0f + row_depths[row_index] * DEBUG_VIEW_MEMORY_INDENT;
        char size_text[48];
        char total[16];
        debug_view_format_bytes(total, sizeof(total), record->size);
        if(has_children[index]) {
            f32 fraction = record->size > 0 ? (f32)used[index] / (f32)record->size : 0.0f;
            v2 bar_min = v2_new(row.min.x + 2.0f, row.min.y + 1.0f);
            ui_push_rect(ui, (UiRect){ bar_min, v2_new(bar_min.x + (row.max.x - row.min.x - 12.0f) * fraction, bar_min.y + DEBUG_VIEW_ROW_HEIGHT - 2.0f) },
                         v4_new(0.3f, 0.5f, 0.9f, 0.25f));
            ui_text(ui, font, string_const(row_expanded[row_index] ? "-" : "+"), v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f),
                    v2_new(indent, y - DEBUG_VIEW_ROW_HEIGHT * 0.5f), DEBUG_VIEW_TEXT_SIZE, white);
            char used_text[16];
            debug_view_format_bytes(used_text, sizeof(used_text), used[index]);
            snprintf(size_text, sizeof(size_text), "%s / %s", used_text, total);
        } else {
            snprintf(size_text, sizeof(size_text), "%s", total);
        }
        String label = { .text = record->label, .capacity = BUFFER_LABEL_MAX_LENGTH, .len = record->label_len };
        ui_text(ui, font, label, v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f), v2_new(indent + 14.0f, y - DEBUG_VIEW_ROW_HEIGHT * 0.5f), DEBUG_VIEW_TEXT_SIZE, white);
        ui_text(ui, font, string_const(size_text), v2_new(1.0f, 1.0f), v2_new(1.0f, 0.5f), v2_new(-14.0f, y - DEBUG_VIEW_ROW_HEIGHT * 0.5f),
                DEBUG_VIEW_TEXT_SIZE, white);
        y -= DEBUG_VIEW_ROW_HEIGHT;
    }

    if(buffers->dropped > 0 && rows_len >= first_row && rows_len < first_row + visible_rows) {
        char dropped[48];
        snprintf(dropped, sizeof(dropped), "%u allocations untracked", buffers->dropped);
        ui_text(ui, font, string_const(dropped), v2_new(0.0f, 1.0f), v2_new(0.0f, 0.5f), v2_new(8.0f, y - DEBUG_VIEW_ROW_HEIGHT * 0.5f),
                DEBUG_VIEW_TEXT_SIZE, v4_new(1.0f, 0.6f, 0.4f, 1.0f));
    }
}

#endif
