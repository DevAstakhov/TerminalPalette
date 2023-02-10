#include <ncurses.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>

struct Form {
    struct BgFg {
        enum { Normal = 0, Intense = 1 } intense;
        enum { Foreground = 0, Background = 1 } color_type;
        int selected_color;
    } bg, fg;

    int active;
    enum { Color = 0, RGB, Value } focus;
    enum { Red = 0, Green = 1, Blue = 2 } selected_rgb;
    struct Sample {
        const char* text;
        int color_pair;
    } sample_text;
} form;

enum HDir { Left, Right };
enum VDir { Up, Down };

struct BgFg get_default(int bg_fg) {
    struct BgFg bgfg = { Normal, bg_fg, 3 };
    return bgfg;
}

void init_form() {
    form.bg = get_default(Background);
    form.fg = get_default(Foreground);

    form.active = Background;
    form.focus = Color;
    form.selected_rgb = Red;
    form.sample_text.text = "Lorem ipsum";
    form.sample_text.color_pair = 17;
}

int relmove(WINDOW* w, short y, short x) {
    return wmove(w, getcury(w) + y, getcurx(w) + x);
}

struct BgFg* get_active_form() {
    return form.active == Background ? &form.bg : &form.fg;
}

int get_intense_bit(const struct BgFg* f) {
    return f->intense == Intense && COLORS > 8 ? 0b00001000 : 0b00000000;
}

int get_selected_color(const struct BgFg* f, bool base) {
    const int intense_bit = base ? 0 : get_intense_bit(f);
    return f->selected_color | intense_bit;
}

void update_sample_color() {
    const struct BgFg *f = get_active_form();
    short color = get_selected_color(f, false);

    short fg,bg;
    pair_content(form.sample_text.color_pair, &fg, &bg);

    if (form.active == Foreground)
        fg = color;
    else
        bg = color;
    
    init_pair(form.sample_text.color_pair, fg, bg);
}

void set_selected_color(struct BgFg* f, int color) {
    f->selected_color = color;
    update_sample_color();
}

void draw_sample(WINDOW* w) {
    WINDOW* box_win = derwin(w, 4, getmaxx(w) - 4, 9, 2);
    box(box_win, 0,0);

    WINDOW* text_win = derwin(box_win, getmaxy(box_win)-2, getmaxx(box_win)-2, 1, 1);

    wattr_on(text_win, COLOR_PAIR(form.sample_text.color_pair), NULL);
    wprintw(text_win, "%s", form.sample_text.text);
    wattr_off(text_win, COLOR_PAIR(form.sample_text.color_pair), NULL);

    delwin(text_win);
    delwin(box_win);
}

void draw_rgb(WINDOW* w) {
    WINDOW* rgb_win = derwin(w, 3, getmaxx(w) - 4, 6, 2);
    box(rgb_win, 0, 0);

    const char* title[3];
    title[0] = "[R] G  B ";
    title[1] = " R [G] B ";
    title[2] = " R  G [B]";

    mvwprintw(rgb_win, 0,7, "%s", title[form.selected_rgb]);

    short fg,bg,comp[3];
    pair_content(get_selected_color(get_active_form(), false) + 1, &fg, &bg);
    color_content(bg, &comp[Red], &comp[Green], &comp[Blue]);

    if (form.focus == Value)
        mvwprintw(rgb_win, 1, 2, "intensity:    [%4d]", comp[form.selected_rgb]);
    else
        mvwprintw(rgb_win, 1, 2, "intensity:     %4d ", comp[form.selected_rgb]);

    delwin(rgb_win);
}

void draw_colors(WINDOW* w) {
    const struct BgFg* active_form = get_active_form();
    const int intense_bit = get_intense_bit(active_form);

    WINDOW* sub = derwin(w, 3, 4, 3, 1);

    for (int color = 0 ; color < 8; ++color) {
        mvderwin(sub, 3, 1 + color*3);
        wborder(sub, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');

        int c = (color | intense_bit) + 1;
        wattr_on(sub, COLOR_PAIR(c), NULL);
        mvwprintw(sub, 1, 1, "  ");
        wattr_off(sub, COLOR_PAIR(c), NULL);
    }

    if (form.focus == Color) {
        const int selected = active_form->selected_color;
        mvderwin(sub, 3, 1 + selected*3);
        wborder(sub, ' ', ' ', ' ', ' ', ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    }

    delwin(sub);
}

void draw(WINDOW* w) {
    const struct BgFg* active_form = get_active_form();

    mvwprintw(w, 2, 2, "%s %s", active_form->intense == Intense ? "[x]" : "[ ]", "Intense");
    relmove(w, 0, 2);
    wprintw(w, "%s ", active_form->color_type == Foreground ? "Foreground" : "Background");

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

void toggle_foreground_background() {
    form.active ^= Background;
}

void toggle_color_intensity() {
    get_active_form()->intense ^= Intense;
    update_sample_color();
}

void move_rgb_component(const enum HDir direction) {
    if (direction == Left)
        form.selected_rgb = form.selected_rgb > 0
            ? form.selected_rgb - 1
            : form.selected_rgb;

    else form.selected_rgb = form.selected_rgb < 2
            ? form.selected_rgb + 1
            : form.selected_rgb;
}

void move_color(const enum HDir direction) {
    struct BgFg *f = get_active_form();
    int selected = get_selected_color(f, true);
    
    if (direction == Left)
        selected = selected > 0 ? selected - 1 : selected;
    else
        selected = selected < 7 ? selected + 1 : selected;

    set_selected_color(f, selected);
}

int get_new_rgb_comp_value(short value, const enum HDir direction) {
    if (direction == Left)
        return value >= 10 ? value - 10 : value;
    else
        return value <= 1000 ? value + 10 : value;
}

void change_selected_rgb_component_value(const enum HDir direction) {
    struct BgFg* f = get_active_form();
    short fg,bg, comp[3];

    pair_content(get_selected_color(f, false) + 1, &fg, &bg);
    color_content(bg, &comp[Red], &comp[Green], &comp[Blue]);
    comp[form.selected_rgb] = get_new_rgb_comp_value(comp[form.selected_rgb], direction);

    init_color(bg, comp[Red], comp[Green], comp[Blue]);
}

void move_focus(const enum VDir direction) {
    if (direction == Up)
        form.focus = form.focus > 0 ? form.focus - 1 : form.focus;
    else
        form.focus = form.focus < 2 ? form.focus + 1 : form.focus;
}

void init_colors() {
    for (short color = 0; color < 16; ++color) {
        short r,g,b;
        color_content(color, &r, &g, &b);
        init_color(color, r, g, b);
        init_pair(color+1, COLOR_WHITE, color);
    }

    init_color(8, 200, 200, 200);
}

int main(int argc, char* argv[]) {

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    if (check() != 0)
        goto exit;

    start_color();
    init_colors();
    
    signal(SIGWINCH, terminal_resized_handler);

    init_form();
    
    init_pair(form.sample_text.color_pair, form.fg.selected_color, form.bg.selected_color);

    WINDOW* w1 = newwin(14, 27, 2, 2);
    keypad(w1, true);
    box (w1, 0,0);
    draw(w1);

    int keepGoing = 1;
    while (keepGoing) {
        bool changed = true;

        switch(wgetch(w1)) {
            case 27: keepGoing = false; break;
            case ' ': toggle_color_intensity(); break;
            case KEY_RIGHT:
                if (form.focus == RGB)
                    move_rgb_component(Right);

                if (form.focus == Color)
                    move_color(Right);
                    
                if (form.focus == Value)
                    change_selected_rgb_component_value(Right);

                break;
            case KEY_LEFT:
                if (form.focus == RGB)
                    move_rgb_component(Left);

                if (form.focus == Color)
                    move_color(Left);
                    
                if (form.focus == Value)
                    change_selected_rgb_component_value(Left);

                break;
            case KEY_UP:   move_focus(Up); break;
            case KEY_DOWN: move_focus(Down); break;
            case 9:        toggle_foreground_background(); break;
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
