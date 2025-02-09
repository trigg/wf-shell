#include <gtkmm/window.h>
#include <glibmm/main.h>
#include <gdk/wayland/gdkwayland.h>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <wf-shell-app.hpp>
#include <gtk4-layer-shell.h>
#include <wf-autohide-window.hpp>

#include "dock.hpp"
#include "../util/gtk-utils.hpp"

class WfDock::impl
{
    WayfireOutput *output;
    std::unique_ptr<WayfireAutohidingWindow> window;
    wl_surface *_wl_surface;

    Gtk::Box box;

    WfOption<std::string> css_path{"dock/css_path"};
    WfOption<int> dock_height{"dock/dock_height"};

  public:
    impl(WayfireOutput *output)
    {
        this->output = output;
        window = std::unique_ptr<WayfireAutohidingWindow>(
            new WayfireAutohidingWindow(output, "dock"));

        window->set_size_request(dock_height, dock_height);
        gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);

        /*window->signal_size_allocate().connect_notify(
            sigc::mem_fun(this, &WfDock::impl::on_allocation));*/
        // TODO Reimplement allocation somehow
        window->set_child(box);

        if ((std::string)css_path != "")
        {
            auto css = load_css_from_path(css_path);
            if (css)
            {
                auto display = Gdk::Display::get_default();
                Gtk::StyleContext::add_provider_for_display(display, css, GTK_STYLE_PROVIDER_PRIORITY_USER);
            }
        }

        /*_wl_surface = gdk_wayland_window_get_wl_surface(
            window->gobj());*/
    }

    void add_child(Gtk::Widget& widget)
    {
        box.append(widget);
    }

    void rem_child(Gtk::Widget& widget)
    {
        this->box.remove(widget);

        /* We now need to resize the dock so it fits the remaining widgets. */
        /*
        int total_width  = 0;
        int total_height = last_height;
        box.foreach([&] (Gtk::Widget& child)
        {
            Gtk::Requisition min_req, pref_req;
            child.get_preferred_size(min_req, pref_req);

            total_width += min_req.width;
            total_height = std::max(total_height, min_req.height);
        });

        total_width = std::min(total_height, 100);
        this->window->resize(total_width, total_height);
        this->window->set_size_request(total_width, total_height);
        */
        // TODO Find a cleaner way than resizing window
    }

    wl_surface *get_wl_surface()
    {
        return this->_wl_surface;
    }

    int32_t last_width = 100, last_height = 100;
    void on_allocation(Gtk::Allocation& alloc)
    {
        if ((last_width != alloc.get_width()) || (last_height != alloc.get_height()))
        {
            last_width  = alloc.get_width();
            last_height = alloc.get_height();
        }
    }
};

WfDock::WfDock(WayfireOutput *output) :
    pimpl(new impl(output))
{}
WfDock::~WfDock() = default;

void WfDock::add_child(Gtk::Widget& w)
{
    return pimpl->add_child(w);
}

void WfDock::rem_child(Gtk::Widget& w)
{
    return pimpl->rem_child(w);
}

wl_surface*WfDock::get_wl_surface()
{
    return pimpl->get_wl_surface();
}
