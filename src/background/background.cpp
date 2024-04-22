#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>
#include <gdk/gdkwayland.h>

#include <random>
#include <algorithm>

#include <iostream>
#include <map>
#include <string>

#include <gtk-utils.hpp>
#include <gtk-layer-shell.h>
#include <glib-unix.h>

#include "background.hpp"


bool WayfireBackground::change_background()
{
    std::string path;

    if (!load_next_background(path))
    {
        return false;
    }
    present_image(path);
    return true;
}

void WayfireBackground::present_image(std::string path){
    if (path=="")
    {
        return;
    }
    if (css_rule)
    {
        window.get_style_context()->remove_provider(css_rule);
    }

    std::string image_url = "background: url(\""+path+"\") no-repeat center center;";
    std::string fade="";
    std::string fit = "background-size: auto";

    if (!first_image){
        fade = "transition: background-image "+ std::to_string(fade_duration)+"ms linear 1s ;";
    }

    std::string fit_mode = background_fill_mode;
    if (fit_mode=="fill_and_crop")
    {
        fit = "background-size: cover;";
    } else if (fit_mode=="preserve_aspect")
    {
        fit = "background-size: contain;";
    } else if (fit_mode=="stretch")
    {
        fit = "background-size: 100% 100%;";
    }

    css_rule = Gtk::CssProvider::create();
    std::string final_rule = ".wf-background { "+ image_url+ fade + fit + " }";
    std::cout << final_rule << std::endl;
    css_rule->load_from_data(final_rule);
    window.get_style_context()->add_provider(css_rule, GTK_STYLE_PROVIDER_PRIORITY_USER);
    current_path = path;
    std::cout << "Loaded " << path << std::endl;
    first_image=false;
}

bool WayfireBackground::load_images_from_dir(std::string path)
{
    wordexp_t exp;

    /* Expand path */
    if (wordexp(path.c_str(), &exp, 0))
    {
        return false;
    }

    if (!exp.we_wordc)
    {
        wordfree(&exp);
        return false;
    }

    auto dir = opendir(exp.we_wordv[0]);
    if (!dir)
    {
        wordfree(&exp);
        return false;
    }

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
        {
            continue;
        }

        auto fullpath = std::string(exp.we_wordv[0]) + "/" + file->d_name;

        struct stat next;
        if (stat(fullpath.c_str(), &next) == 0)
        {
            if (S_ISDIR(next.st_mode))
            {
                /* Recursive search */
                load_images_from_dir(fullpath);
            } else
            {
                images.push_back(fullpath);
            }
        }
    }

    wordfree(&exp);

    if (background_randomize && images.size())
    {
        std::random_device random_device;
        std::mt19937 random_gen(random_device());
        std::shuffle(images.begin(), images.end(), random_gen);
    }

    return true;
}

bool WayfireBackground::load_next_background(std::string & path)
{
    if (!images.size())
    {
        std::cerr << "Failed to load background images from " <<
                (std::string)background_image << std::endl;
        window.remove();
        return false;
    }

    current_background = (current_background + 1) % images.size();

    path = images[current_background];

    return true;
}

void WayfireBackground::reset_background()
{
    images.clear();
    current_background = 0;
    change_bg_conn.disconnect();
}

void WayfireBackground::set_background()
{
    reset_background();
    std::string path = background_image;
    load_images_from_dir(path);
    if (current_path == ""){
        load_next_background(current_path);
    }
    present_image(current_path);

    reset_cycle_timeout();

    if (inhibited && output->output)
    {
        zwf_output_v2_inhibit_output_done(output->output);
        inhibited = false;
    }
}

void WayfireBackground::reset_cycle_timeout()
{
    int cycle_timeout = background_cycle_timeout * 1000;
    change_bg_conn.disconnect();
    if (images.size())
    {
        change_bg_conn = Glib::signal_timeout().connect(sigc::mem_fun(
            this, &WayfireBackground::change_background), cycle_timeout);
    }
}

void WayfireBackground::setup_window()
{
    window.set_decorated(false);

    gtk_layer_init_for_window(window.gobj());
    gtk_layer_set_layer(window.gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_monitor(window.gobj(), this->output->monitor->gobj());

    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    gtk_layer_set_keyboard_mode(window.gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    gtk_layer_set_exclusive_zone(window.gobj(), -1);
    window.add(image);
    window.show_all();
    window.get_style_context()->add_class("wf-background");

    auto reset_background = [=] () { set_background(); };
    auto reset_cycle = [=] () { reset_cycle_timeout(); };
    background_image.set_callback(reset_background);
    background_randomize.set_callback(reset_background);
    background_fill_mode.set_callback(reset_background);
    background_cycle_timeout.set_callback(reset_cycle);

    set_background();
}

WayfireBackground::WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
{
    this->app    = app;
    this->output = output;

    if (output->output)
    {
        this->inhibited = true;
        zwf_output_v2_inhibit_output(output->output);
    }

    setup_window();

    this->window.signal_size_allocate().connect_notify(
        [this, width = 0, height = 0] (Gtk::Allocation& alloc) mutable
    {
        if ((alloc.get_width() != width) || (alloc.get_height() != height))
        {
            this->set_background();
            width  = alloc.get_width();
            height = alloc.get_height();
        }
    });
}

WayfireBackground::~WayfireBackground()
{
    reset_background();
}

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground>> backgrounds;

  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv)
    {
        WayfireShellApp::instance =
            std::make_unique<WayfireBackgroundApp>(argc, argv);
        g_unix_signal_add(SIGUSR1, sigusr1_handler, (void*)instance.get());
        instance->run();
    }

    void handle_new_output(WayfireOutput *output) override
    {
        backgrounds[output] = std::unique_ptr<WayfireBackground>(
            new WayfireBackground(this, output));
    }

    void handle_output_removed(WayfireOutput *output) override
    {
        backgrounds.erase(output);
    }

    static gboolean sigusr1_handler(void *instance)
    {
        for (const auto& [_, bg] : ((WayfireBackgroundApp*)instance)->backgrounds)
        {
            bg->change_background();
        }

        return TRUE;
    }
};

int main(int argc, char **argv)
{
    WayfireBackgroundApp::create(argc, argv);
    return 0;
}
