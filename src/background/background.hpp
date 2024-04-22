#pragma once

#include <glibmm/refptr.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/cssprovider.h>
#include <wf-shell-app.hpp>
#include <wf-option-wrap.hpp>
#include <wayfire/util/duration.hpp>

class WayfireBackground
{
    WayfireShellApp *app;
    WayfireOutput *output;

    std::vector<std::string> images;
    Gtk::Window window;

    Glib::RefPtr<Gtk::CssProvider> css_rule;

    bool inhibited     = false;
    std::string current_path;
    uint current_background;
    sigc::connection change_bg_conn;
    bool first_image=true;

    WfOption<int> fade_duration{"background/fade_duration"};
    WfOption<std::string> background_image{"background/image"};
    WfOption<int> background_cycle_timeout{"background/cycle_timeout"};
    WfOption<bool> background_randomize{"background/randomize"};
    WfOption<std::string> background_fill_mode{"background/fill_mode"};

    Glib::RefPtr<Gdk::Pixbuf> create_from_file_safe(std::string path);
    bool background_transition_frame(int timer);
    void present_image(std::string path);
    bool load_images_from_dir(std::string path);
    bool load_next_background(std::string & path);
    void reset_background();
    void set_background();
    void reset_cycle_timeout();
    std::string sanitize(std::string path, std::string from, std::string to);

    void setup_window();

  public:
    WayfireBackground(WayfireShellApp *app, WayfireOutput *output);
    bool change_background();
    ~WayfireBackground();
};
