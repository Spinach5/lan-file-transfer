#include "file_browser.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FB_MAX_ENTRIES 512
#define FB_VISIBLE     15

static char fb_current_dir[1024];
static char fb_entries[FB_MAX_ENTRIES][256];
static int  fb_entry_types[FB_MAX_ENTRIES]; /* 0=dir, 1=file */
static int  fb_entry_count;
static int  fb_scroll_offset;
static int  fb_selected;      /* highlighted entry, -1 = ".." parent, -2 = none */

/* Double-click detection */
static uint32_t fb_last_click_time = 0;
static int      fb_last_click_idx   = -2;

void file_browser_init(struct app_state *state)
{
    (void)state;
    if (!getcwd(fb_current_dir, sizeof(fb_current_dir))) {
        strncpy(fb_current_dir, "/", sizeof(fb_current_dir) - 1);
    }
    fb_scroll_offset = 0;
    fb_selected = -2;
    fb_last_click_time = 0;
    fb_last_click_idx = -2;

    fb_entry_count = 0;
    DIR *d = opendir(fb_current_dir);
    if (!d) {
        char *slash = strrchr(fb_current_dir, '/');
        if (slash && slash != fb_current_dir) *slash = '\0';
        else strncpy(fb_current_dir, "/", sizeof(fb_current_dir) - 1);
        d = opendir(fb_current_dir);
        if (!d) return;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL && fb_entry_count < FB_MAX_ENTRIES) {
        if (strcmp(de->d_name, ".") == 0) continue;
        char full[1280];
        snprintf(full, sizeof(full), "%s/%s", fb_current_dir, de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;

        strncpy(fb_entries[fb_entry_count], de->d_name, 255);
        fb_entry_types[fb_entry_count] = S_ISDIR(st.st_mode) ? 0 : 1;
        fb_entry_count++;
    }
    closedir(d);
}

void file_browser_render(SDL_Renderer *r, struct app_state *state)
{
    int w = state->window_w, h = state->window_h;
    int bx = w / 2 - 280, by = h / 2 - 200;
    int bw = 560, bh = 400;

    ui_draw_rect(r, 0, 0, w, h, (SDL_Color){0, 0, 0, 190});
    ui_draw_rect(r, bx, by, bw, bh, COLOR_SURFACE);

    /* Title */
    ui_draw_rect(r, bx, by, bw, 28, COLOR_ACCENT);
    const char *target = (state->file_browser_target == 1) ? "Select File/Dir to Send" : "Select Save Directory";
    ui_draw_text(r, target, bx + 10, by + 6, COLOR_BG);

    /* Path bar */
    ui_draw_rect(r, bx, by + 30, bw, 22, COLOR_BG);
    ui_draw_text(r, fb_current_dir, bx + 6, by + 33, COLOR_ACCENT);
    SDL_SetRenderDrawColor(r, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(r, bx, by + 53, bx + bw, by + 53);

    int list_y = by + 56;

    /* ".." parent directory */
    if (strcmp(fb_current_dir, "/") != 0) {
        SDL_Color c = (fb_selected == -1) ? COLOR_ACCENT : COLOR_DIM;
        if (fb_selected == -1) {
            ui_draw_rect(r, bx + 6, list_y - 1, bw - 12, 22, COLOR_HOVER);
        }
        ui_draw_text(r, "[↑]  ..  (parent directory)", bx + 10, list_y + 2, c);
        list_y += 24;
    }

    /* Entries */
    for (int i = fb_scroll_offset; i < fb_entry_count && i < fb_scroll_offset + FB_VISIBLE; i++) {
        int ey = list_y + (i - fb_scroll_offset) * 24;
        bool sel = (i == fb_selected);

        if (sel) {
            ui_draw_rect(r, bx + 6, ey - 1, bw - 12, 22, COLOR_ACCENT);
        }

        SDL_Color c;
        if (sel) c = COLOR_BG;
        else if (fb_entry_types[i] == 0) c = (SDL_Color){0x89, 0xb4, 0xfa, 255};
        else c = COLOR_TEXT;

        char label[280];
        snprintf(label, sizeof(label), "%s %s",
                 fb_entry_types[i] == 0 ? "[DIR] " : "      ", fb_entries[i]);
        ui_draw_text(r, label, bx + 10, ey + 2, c);
    }

    if (fb_entry_count == 0) {
        ui_draw_text(r, "(empty directory)", bx + 10, list_y + 2, COLOR_DIM);
    }

    /* Entry count info */
    if (fb_entry_count > FB_VISIBLE) {
        char info[64];
        snprintf(info, sizeof(info), "%d-%d / %d",
                 fb_scroll_offset + 1,
                 fb_scroll_offset + FB_VISIBLE < fb_entry_count ? fb_scroll_offset + FB_VISIBLE : fb_entry_count,
                 fb_entry_count);
        ui_draw_text(r, info, bx + bw - 130, by + bh - 42, COLOR_DIM);
    }

    /* Hint */
    ui_draw_text(r, "Click: select   DblClick: enter dir   Enter: confirm   Backspace: up   Esc: cancel",
                 bx + 10, by + bh - 42, COLOR_DIM);

    /* Buttons */
    int by2 = by + bh - 36;
    SDL_SetRenderDrawColor(r, COLOR_DIM.r, COLOR_DIM.g, COLOR_DIM.b, COLOR_DIM.a);
    SDL_RenderDrawLine(r, bx, by2 - 4, bx + bw, by2 - 4);
    /* Select */
    {
        int tw, th;
        ui_draw_rect(r, bx + bw - 180, by2 - 2, 80, 28, COLOR_ACCENT);
        ui_text_size("Select", &tw, &th);
        ui_draw_text(r, "Select", bx + bw - 180 + (80 - tw) / 2, by2 + 2, COLOR_BG);
    }
    /* Cancel */
    {
        int tw, th;
        ui_draw_rect(r, bx + bw - 90, by2 - 2, 80, 28, COLOR_SURFACE);
        ui_text_size("Cancel", &tw, &th);
        ui_draw_text(r, "Cancel", bx + bw - 90 + (80 - tw) / 2, by2 + 2, COLOR_TEXT);
    }
}

/* Apply the selected entry and close browser */
static void confirm_selection(struct app_state *state)
{
    if (fb_selected >= 0 && fb_selected < fb_entry_count) {
        snprintf(state->file_browser_result, sizeof(state->file_browser_result),
                 "%s/%s", fb_current_dir, fb_entries[fb_selected]);
    } else {
        /* No entry selected — use current directory */
        strncpy(state->file_browser_result, fb_current_dir,
                sizeof(state->file_browser_result) - 1);
    }

    if (state->file_browser_target == 1) {
        strncpy(state->send_filepath, state->file_browser_result,
                sizeof(state->send_filepath) - 1);
        snprintf(state->status_text, sizeof(state->status_text),
                 "Selected: %s", state->send_filepath);
    } else if (state->file_browser_target == 2) {
        strncpy(state->recv_savepath, state->file_browser_result,
                sizeof(state->recv_savepath) - 1);
        snprintf(state->status_text, sizeof(state->status_text),
                 "Save to: %s", state->recv_savepath);
    }
    state->file_browser_visible = false;
}

/* Navigate into a directory */
static void enter_directory(const char *name)
{
    char newdir[1024];
    snprintf(newdir, sizeof(newdir), "%s/%s", fb_current_dir, name);
    strncpy(fb_current_dir, newdir, sizeof(fb_current_dir) - 1);
    fb_scroll_offset = 0;
    fb_selected = -2;
}

/* Go to parent directory */
static void go_parent(void)
{
    char *slash = strrchr(fb_current_dir, '/');
    if (slash && slash != fb_current_dir) *slash = '\0';
    else if (slash == fb_current_dir) fb_current_dir[1] = '\0';
    fb_scroll_offset = 0;
    fb_selected = -2;
}

bool file_browser_handle_event(SDL_Event *e, struct app_state *state)
{
    int w = state->window_w, h = state->window_h;
    int bx = w / 2 - 280, by = h / 2 - 200;
    int bw = 560, bh = 400;
    int list_y = by + 56;
    bool has_parent = (strcmp(fb_current_dir, "/") != 0);
    if (has_parent) list_y += 24;

    if (e->type == SDL_MOUSEBUTTONDOWN) {
        int mx = e->button.x, my = e->button.y;

        /* Cancel button */
        if (ui_in_rect(mx, my, bx + bw - 90, by + bh - 38, 80, 28)) {
            state->file_browser_visible = false;
            return true;
        }

        /* Select button */
        if (ui_in_rect(mx, my, bx + bw - 180, by + bh - 38, 80, 28)) {
            confirm_selection(state);
            return true;
        }

        /* ".." parent click */
        if (has_parent && ui_in_rect(mx, my, bx + 6, by + 56, bw - 12, 22)) {
            /* Double-click on ".." → go up */
            uint32_t now = SDL_GetTicks();
            if (fb_last_click_idx == -1 && (now - fb_last_click_time) < 400) {
                go_parent();
                file_browser_init(state);
            } else {
                fb_selected = -1;
                fb_last_click_time = now;
                fb_last_click_idx = -1;
            }
            return true;
        }

        /* Entry click */
        for (int i = fb_scroll_offset; i < fb_entry_count && i < fb_scroll_offset + FB_VISIBLE; i++) {
            int ey = list_y + (i - fb_scroll_offset) * 24;
            if (ui_in_rect(mx, my, bx + 6, ey - 1, bw - 12, 22)) {
                uint32_t now = SDL_GetTicks();

                /* Check for double-click (same entry, within 400ms) */
                if (fb_last_click_idx == i && (now - fb_last_click_time) < 400) {
                    /* Double-click! */
                    fb_last_click_time = 0;
                    fb_last_click_idx = -2;
                    if (fb_entry_types[i] == 0) {
                        /* Enter directory */
                        enter_directory(fb_entries[i]);
                        file_browser_init(state);
                    } else {
                        /* Select file immediately */
                        fb_selected = i;
                        confirm_selection(state);
                    }
                } else {
                    /* Single click — highlight */
                    fb_selected = i;
                    fb_last_click_time = now;
                    fb_last_click_idx = i;
                }
                return true;
            }
        }

        /* Click on path bar → go to parent */
        if (ui_in_rect(mx, my, bx, by + 30, bw, 22)) {
            go_parent();
            file_browser_init(state);
            return true;
        }

        /* Click outside = close */
        if (!ui_in_rect(mx, my, bx, by, bw, bh)) {
            state->file_browser_visible = false;
            return true;
        }
    }

    /* Scroll wheel */
    if (e->type == SDL_MOUSEWHEEL) {
        fb_scroll_offset -= e->wheel.y;
        if (fb_scroll_offset < 0) fb_scroll_offset = 0;
        if (fb_scroll_offset > fb_entry_count - FB_VISIBLE)
            fb_scroll_offset = fb_entry_count - FB_VISIBLE;
        if (fb_scroll_offset < 0) fb_scroll_offset = 0;
        return true;
    }

    /* Keyboard */
    if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
        case SDLK_ESCAPE:
            state->file_browser_visible = false;
            return true;
        case SDLK_UP:
            fb_last_click_idx = -2;
            if (has_parent && (fb_selected == -2 || fb_selected == -1)) {
                fb_selected = fb_entry_count - 1;
            } else {
                fb_selected--;
                if (fb_selected < -1) fb_selected = fb_entry_count - 1;
            }
            if (fb_selected >= 0 && fb_selected < fb_scroll_offset)
                fb_scroll_offset = fb_selected;
            if (fb_selected == -1 && has_parent)
                fb_scroll_offset = 0;
            return true;
        case SDLK_DOWN:
            fb_last_click_idx = -2;
            if (fb_selected == -2) {
                fb_selected = has_parent ? -1 : 0;
            } else {
                fb_selected++;
                if (fb_selected >= fb_entry_count)
                    fb_selected = -2;
            }
            if (fb_selected >= fb_scroll_offset + FB_VISIBLE)
                fb_scroll_offset = fb_selected - FB_VISIBLE + 1;
            if (fb_selected == -2) fb_scroll_offset = 0;
            return true;
        case SDLK_RETURN:
            fb_last_click_idx = -2;
            if (fb_selected >= 0 && fb_selected < fb_entry_count) {
                if (fb_entry_types[fb_selected] == 0) {
                    /* Enter directory */
                    enter_directory(fb_entries[fb_selected]);
                    file_browser_init(state);
                } else {
                    /* Confirm file selection */
                    confirm_selection(state);
                }
            } else if (fb_selected == -1 && has_parent) {
                /* Enter ".." */
                go_parent();
                file_browser_init(state);
            }
            return true;
        case SDLK_BACKSPACE:
            fb_last_click_idx = -2;
            go_parent();
            file_browser_init(state);
            return true;
        default:
            break;
        }
    }

    return true;
}
