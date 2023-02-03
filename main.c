#include <ncurses.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>

struct Form {
    struct Intense {
        enum Status {
            Disabled = 0,
            Enabled = 1
        } status_bg, status_fg;
        const char* brackets[2];
        const char* text;
        short pos_y, pos_x;
    } intense;

    struct ColorType {
        enum {
            Foreground = 0,
            Background = 1
        } type;

        const char* text[2];
        short pos_y, pos_x;
    } color_type;

    struct ColorComponent {
        enum {
            None = 0,
            Red = 1,
            Green = 2,
            Blue = 3
        } color;

        const char* title[4];
    } color_component;

    struct Sample {
        const char* text;
    } sample;

    int selected_color_bg;
    int selected_color_fg;
    int selected_color_pair;

    enum Selected {
        Color = 0,
        RGB,
        Intensity
    } selected;

} form;

void init() {
    form.intense.status_bg = Disabled;
    form.intense.status_fg = Disabled;
    form.intense.brackets[0] = "[ ]";
    form.intense.brackets[1] = "[x]";
    form.intense.text = "Intense";

    form.color_type.type = Foreground;
    form.color_type.text[0] = "Foreground";
    form.color_type.text[1] = "Background";

    form.color_component.color = Blue;
    form.color_component.title[None]  = " R  G  B ";
    form.color_component.title[Red]   = "[R] G  B ";
    form.color_component.title[Green] = " R [G] B ";
    form.color_component.title[Blue]  = " R  G [B]";

    form.sample.text = "Lorem ipsum dolor sitamet, consectetur ...";

    form.selected = Color;

    form.selected_color_fg = 3;
    form.selected_color_bg = 1;
}

int relmove(WINDOW* w, short y, short x) {
    return wmove(w, getcury(w) + y, getcurx(w) + x);
}

void draw_sample(WINDOW* w) {
    WINDOW* box_win = derwin(w, 4, getmaxx(w) - 4, 9, 2);
    box(box_win, 0,0);

    WINDOW* text_win = derwin(box_win, getmaxy(box_win)-2, getmaxx(box_win)-2, 1, 1);

    const int intense_bit_bg = form.intense.status_bg == Enabled && COLORS > 8 ? 0b00001000 : 0b00000000;
    int bg_color = form.selected_color_bg | intense_bit_bg;

    const int intense_bit_fg = form.intense.status_fg == Enabled && COLORS > 8 ? 0b00001000 : 0b00000000;
    int fg_color = form.selected_color_fg | intense_bit_fg;

    init_pair(17, fg_color, bg_color);

    wattr_on(text_win, COLOR_PAIR(17), NULL);
    wprintw(text_win, "%s", form.sample.text);
    wattr_off(text_win, COLOR_PAIR(17), NULL);

    free_pair(17);

    delwin(text_win);
    delwin(box_win);
}

void draw_rgb(WINDOW* w) {
    WINDOW* rgb_win = derwin(w, 3, getmaxx(w) - 4, 6, 2);
    box(rgb_win, 0, 0);

    const int selection = form.selected == RGB ? form.color_component.color : None;
    mvwprintw(rgb_win, 0,6, "%s", form.color_component.title[selection]);

    enum Status s = form.color_type.type == Foreground ? form.intense.status_fg : form.intense.status_bg;
    const int intense_bit = s == Enabled && COLORS > 8 ? 0b00001000 : 0b00000000;
    const int selected_color = (form.color_type.type == Foreground ? form.selected_color_fg : form.selected_color_bg) | intense_bit;

    short fg, bg;
    pair_content(selected_color, &bg, &fg);

    short color = fg+1;

    short r,g,b;
    color_content(color, &r, &g, &b);

    short intensity;
    switch (form.color_component.color) {
        case Red: intensity = r; break;
        case Green: intensity = g; break;
        case Blue: intensity = b; break;
        default: intensity = 0;
    }

    if (form.selected == Intensity)
        mvwprintw(rgb_win, 1, 2, "intensity:    [%4d]", intensity);
    else
        mvwprintw(rgb_win, 1, 2, "intensity:     %4d ", intensity);

    delwin(rgb_win);
}

void draw_colors(WINDOW* w) {
    enum Status s = form.color_type.type == Foreground ? form.intense.status_fg : form.intense.status_bg;
    const int intense_bit = s == Enabled && COLORS > 8 ? 0b00001000 : 0b00000000;

    WINDOW* sub = derwin(w, 3, 4, 3, 1);

    for (int color = 0 ; color < 8; ++color) {
        mvderwin(sub, 3, 1 + color*3);
        wborder(sub, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');

        int c = color | intense_bit;
        wattr_on(sub, COLOR_PAIR(c+1), NULL);
        mvwprintw(sub, 1, 1, "  ");
        wattr_off(sub, COLOR_PAIR(c+1), NULL);
    }

    if (form.selected == Color) {
        const int selected = form.color_type.type == Foreground ? form.selected_color_fg : form.selected_color_bg;
        mvderwin(sub, 3, 1 + selected*3);
        wborder(sub, ' ', ' ', ' ', ' ', ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    }

    delwin(sub);
}

void draw(WINDOW* w) {
    
    enum Status s = form.color_type.type == Foreground ? form.intense.status_fg : form.intense.status_bg;
    mvwprintw(w, 2, 2, "%s %s ", form.intense.brackets[s], form.intense.text);
    relmove(w, 0, 1);
    wprintw(w, "%s ", form.color_type.text[form.color_type.type]);

    draw_colors(w);
    draw_rgb(w);
    draw_sample(w);

    wrefresh(w);
}

void terminal_resized_handler(int sig) {
    if (sig == SIGWINCH) {
        struct winsize w;
        ioctl(0, TIOCGWINSZ, &w);
        resizeterm(w.ws_col, w.ws_row);
    }
}

const char* err_msg = 0;
int exit_result = 0;

int fail(const char* msg) {
    err_msg = "Terminal does not support colors\n";
    exit_result = -1;
    return -1;
}

int check() {
    if (!has_colors())
        return fail("Terminal does not support colors.\n");

    if (!can_change_color())
        return fail("Terminal does not support color change.\n");

    return 0;
}

int main(int argc, char* argv[]) {

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    if (check() != 0)
        goto exit;

    start_color();
    
    signal(SIGWINCH, terminal_resized_handler);

    init();

    for (short color = 0; color < 16; ++color) {
        short r, g, b;
        color_content(color, &r, &g, &b);
        init_color(color, r, g, b);
        init_pair(color+1, COLOR_WHITE, color);
    }
    
    init_pair(17, form.selected_color_fg, form.selected_color_bg);

    WINDOW* w1 = newwin(14, 27, 2, 2);
    keypad(w1, true);
    box (w1, 0,0);
    draw(w1);

    int keepGoing = 1;
    while (keepGoing) {
        bool changed = true;

        switch(wgetch(w1)) {
            case ' ': {
                enum Status *status = form.color_type.type == Foreground ? &form.intense.status_fg : &form.intense.status_bg;
                *status ^= Enabled;
                } break;
            case 27:
                keepGoing = false;
                break;
            case KEY_RIGHT:
                if (form.selected == RGB) {
                    form.color_component.color = form.color_component.color < 3 ? form.color_component.color + 1 : form.color_component.color;
                }
                if (form.selected == Color) {
                    int *selected = form.color_type.type == Foreground ? &form.selected_color_fg : &form.selected_color_bg;
                    *selected = *selected < 7 ? *selected + 1 : *selected;
                }
                if (form.selected == Intensity) {
                    enum Status s = form.color_type.type == Foreground ? form.intense.status_fg : form.intense.status_bg;
                    const int intense_bit = s == Enabled && COLORS > 8 ? 0b00001000 : 0b00000000;
                    short color = (form.color_type.type == Foreground ? form.selected_color_fg : form.selected_color_bg) | intense_bit;

                    short r,g,b;
                    color_content(color, &r, &g, &b);

                    switch (form.color_component.color) {
                        case Red: r = r < 1000 ? r + 10 : r; break;
                        case Green: g = g < 1000 ? g + 10 : g; break;
                        case Blue: b = b < 1000 ? b + 10 : b; break;
                        default: ;
                    }

                    init_color(color, r, g, b);
                }
                break;
            case KEY_LEFT:
                if (form.selected == RGB) {
                    form.color_component.color = form.color_component.color > 1 ? form.color_component.color - 1 : form.color_component.color;
                }
                if (form.selected == Color) {
                    int *selected = form.color_type.type == Foreground ? &form.selected_color_fg : &form.selected_color_bg;
                    *selected = *selected > 0 ? *selected - 1 : *selected;
                }
                if (form.selected == Intensity) {
                    enum Status s = form.color_type.type == Foreground ? form.intense.status_fg : form.intense.status_bg;
                    const int intense_bit = s == Enabled && COLORS > 8 ? 0b00001000 : 0b00000000;
                    short color = (form.color_type.type == Foreground ? form.selected_color_fg : form.selected_color_bg) | intense_bit;

                    short r,g,b;
                    color_content(color, &r, &g, &b);

                    switch (form.color_component.color) {
                        case Red: r = r >= 10 ? r-10 : r; break;
                        case Green: g = g >= 10 ? g-10 : g; break;
                        case Blue: b = b >= 10 ? b-10 : b; break;
                        default: ;
                    }

                    init_color(color, r, g, b);
                }
                break;
            case KEY_UP:
                form.selected = form.selected > 0 ? form.selected - 1 : form.selected;
                break;
            case KEY_DOWN:
                form.selected = form.selected < 2 ? form.selected + 1 : form.selected;
                break;
            case 9:
                form.color_type.type = form.color_type.type == Foreground ? Background : Foreground;
                break;
            default:
                changed = false;
        }

        if (changed)
            draw(w1);
    }

exit:    
    endwin();

    if (err_msg)
        printf("ERROR: %s", err_msg);

    return exit_result;
}
