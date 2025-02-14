#include <glibmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/box.h>
#include <gtkmm/application.h>
#include <gdk/wayland/gdkwayland.h>
#include <gtk4-layer-shell.h>

#include <stdio.h>
#include <iostream>
#include <sstream>

#include <map>
#include <filesystem>
#include <css-config.hpp>

#include "panel.hpp"
#include "../util/gtk-utils.hpp"

#include "widgets/battery.hpp"
#include "widgets/command-output.hpp"
#include "widgets/menu.hpp"
#include "widgets/clock.hpp"
#include "widgets/launchers.hpp"
#include "widgets/network.hpp"
#include "widgets/spacing.hpp"
#include "widgets/separator.hpp"
#ifdef HAVE_PULSE
    #include "widgets/volume.hpp"
#endif
#include "widgets/window-list/window-list.hpp"
#include "widgets/notifications/notification-center.hpp"
#include "widgets/tray/tray.hpp"

#include "wf-autohide-window.hpp"

class WayfirePanel::impl
{
    std::unique_ptr<WayfireAutohidingWindow> window;

    Gtk::Box content_box;
    Gtk::Box left_box, center_box, right_box;

    using Widget = std::unique_ptr<WayfireWidget>;
    using WidgetContainer = std::vector<Widget>;
    WidgetContainer left_widgets, center_widgets, right_widgets;

    WayfireOutput *output;

    WfOption<std::string> panel_layer{"panel/layer"};
    std::function<void()> set_panel_layer = [=] ()
    {
        if ((std::string)panel_layer == "overlay")
        {
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
        }

        if ((std::string)panel_layer == "top")
        {
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_TOP);
        }

        if ((std::string)panel_layer == "bottom")
        {
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_BOTTOM);
        }

        if ((std::string)panel_layer == "background")
        {
            gtk_layer_set_layer(window->gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
        }
    };

    WfOption<int> minimal_panel_height{"panel/minimal_height"};

    void create_window()
    {
        window = std::make_unique<WayfireAutohidingWindow>(output, "panel");
        window->set_interactive_debugging(true);

        window->set_default_size(0, minimal_panel_height);
        window->get_style_context()->add_class("wf-panel");
        panel_layer.set_callback(set_panel_layer);
        set_panel_layer(); // initial setting
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
        gtk_layer_set_anchor(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_LEFT, 0);
        gtk_layer_set_margin(window->gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, 0);

        window->present();
        init_widgets();
        init_layout();

        //window->signal_close_request().connect(
        //    sigc::mem_fun(*this, &WayfirePanel::impl::on_delete));
    }

    void init_layout()
    {
        left_box.get_style_context()->add_class("left");
        right_box.get_style_context()->add_class("right");
        center_box.get_style_context()->add_class("center");
        content_box.append(left_box);
        content_box.append(center_box);
        content_box.append(right_box);

        content_box.set_hexpand(true);


        left_box.set_halign(Gtk::Align::START);
        center_box.set_halign(Gtk::Align::CENTER);
        right_box.set_halign(Gtk::Align::END);

        window->set_child(content_box);
    }

    std::optional<int> widget_with_value(std::string value, std::string prefix)
    {
        if (value.find(prefix) == 0)
        {
            auto output_str = value.substr(prefix.size());
            int output = std::atoi(output_str.c_str());
            if (output > 0)
            {
                return output;
            }

            std::cerr << "Invalid widget value: " << value << std::endl;
        }

        return {};
    }

    Widget widget_from_name(std::string name)
    {
        if (name == "menu")
        {
            return Widget(new WayfireMenu(output));
        }

        if (name == "launchers")
        {
            return Widget(new WayfireLaunchers());
        }

        if (name == "clock")
        {
            return Widget(new WayfireClock());
        }

        if (name == "network")
        {
            return Widget(new WayfireNetworkInfo());
        }

        if (name == "battery")
        {
            return Widget(new WayfireBatteryInfo());
        }

        if (name == "volume")
        {
#ifdef HAVE_PULSE
            return Widget(new WayfireVolume());
#else
    #warning "Pulse not found, volume widget will not be available."
            std::cerr << "Built without pulse support, volume widget "
                         " is not available." << std::endl;
#endif
        }

        if (name == "window-list")
        {
            return Widget(new WayfireWindowList(output));
        }

        if (name == "notifications")
        {
            return Widget(new WayfireNotificationCenter());
        }

        if (name == "tray")
        {
            return Widget(new WayfireStatusNotifier());
        }

        if (name == "command-output")
        {
            return Widget(new WfCommandOutputButtons());
        }

        if (auto pixel = widget_with_value(name, "spacing"))
        {
            return Widget(new WayfireSpacing(*pixel));
        }

        if (auto pixel = widget_with_value(name, "separator"))
        {
            return Widget(new WayfireSeparator(*pixel));
        }

        if (name != "none")
        {
            std::cerr << "Invalid widget: " << name << std::endl;
        }

        return nullptr;
    }

    static std::vector<std::string> tokenize(std::string list)
    {
        std::string token;
        std::istringstream stream(list);
        std::vector<std::string> result;

        while (stream >> token)
        {
            if (token.size())
            {
                result.push_back(token);
            }
        }

        return result;
    }

    void reload_widgets(std::string list, WidgetContainer& container,
        Gtk::Box& box)
    {
        const auto lock_sn_watcher = Watcher::Instance();
        const auto lock_notification_daemon = Daemon::Instance();
        for(auto child : box.get_children()){
            box.remove(*child);
        }
        container.clear();
        auto widgets = tokenize(list);
        for (auto widget_name : widgets)
        {
            if(widget_name == "window-list"){
                box.set_hexpand(true);
            }
            auto widget = widget_from_name(widget_name);
            if (!widget)
            {
                continue;
            }

            widget->widget_name = widget_name;
            widget->init(&box);

            container.push_back(std::move(widget));
        }
    }

    WfOption<std::string> left_widgets_opt{"panel/widgets_left"};
    WfOption<std::string> right_widgets_opt{"panel/widgets_right"};
    WfOption<std::string> center_widgets_opt{"panel/widgets_center"};
    void init_widgets()
    {
        left_widgets_opt.set_callback([=] ()
        {
            reload_widgets((std::string)left_widgets_opt, left_widgets, left_box);
        });
        right_widgets_opt.set_callback([=] ()
        {
            reload_widgets((std::string)right_widgets_opt, right_widgets, right_box);
        });
        center_widgets_opt.set_callback([=] ()
        {
            reload_widgets((std::string)center_widgets_opt, center_widgets, center_box);

            if (center_box.get_children().empty())
            {
                content_box.set_homogeneous(false);
            } else
            {
                content_box.set_homogeneous(true);
            }
        });

        reload_widgets((std::string)left_widgets_opt, left_widgets, left_box);
        reload_widgets((std::string)right_widgets_opt, right_widgets, right_box);
        reload_widgets((std::string)center_widgets_opt, center_widgets, center_box);
        if (center_box.get_children().empty())
        {
            content_box.set_homogeneous(false);
        } else
        {
            content_box.set_homogeneous(true);
        }
    }

  public:
    impl(WayfireOutput *output) : output(output)
    {
        // Intentionally leaking feels bad.
        new CssFromConfigInt("panel/launchers_size", ".menu-button,.launcher{-gtk-icon-size:", "px;}");
        new CssFromConfigInt("panel/launchers_spacing", ".launcher{padding: 0px ", "px;}");
        new CssFromConfigInt("panel/battery_icon_size", ".battery image{-gtk-icon-size:", "px;}");
        new CssFromConfigInt("panel/network_icon_size", ".network{-gtk-icon-size:","px;}");
        new CssFromConfigInt("panel/volume_icon_size", ".volume{-gtk-icon-size:", "px;}");
        new CssFromConfigInt("panel/notifications_icon_size", ".notification-center{-gtk-icon-size:", "px;}");
        new CssFromConfigInt("panel/tray_icon_size", ".tray-button{-gtk-icon-size:", "px;}");
        new CssFromConfigString("panel/background_color", ".wf-panel{background-color:",";}");
        new CssFromConfigBool("panel/battery_icon_invert", ".battery image{filter:invert(100%);}", "");
        new CssFromConfigBool("panel/network_icon_invert_color", ".network-icon{filter:invert(100%);}", "");
        
        // People will probably need to update sizes to have a measure
        // 16px, 1.1rem, 1em .
        // So on
        new CssFromConfigFont("panel/battery_font", ".battery {","}");
        new CssFromConfigFont("panel/clock_font", ".clock {","}");

        create_window();
    }

    wl_surface *get_wl_surface()
    {
        return window->get_wl_surface();
    }

    Gtk::Window& get_window()
    {
        return *window;
    }

    void handle_config_reload()
    {
        for (auto& w : left_widgets)
        {
            w->handle_config_reload();
        }

        for (auto& w : right_widgets)
        {
            w->handle_config_reload();
        }
        for (auto& w : center_widgets)
        {
            w->handle_config_reload();
        }
    }
};

WayfirePanel::WayfirePanel(WayfireOutput *output) : pimpl(new impl(output))
{}
wl_surface*WayfirePanel::get_wl_surface()
{
    return pimpl->get_wl_surface();
}

Gtk::Window& WayfirePanel::get_window()
{
    return pimpl->get_window();
}

void WayfirePanel::handle_config_reload()
{
    return pimpl->handle_config_reload();
}

class WayfirePanelApp::impl
{
  public:
    std::map<WayfireOutput*, std::unique_ptr<WayfirePanel>> panels;
};

void WayfirePanelApp::on_config_reload()
{
    for (auto& p : priv->panels)
    {
        p.second->handle_config_reload();
    }
}

void WayfirePanelApp::on_css_reload()
{
    clear_css_rules();
    /* Add user directory */
    std::string ext(".css");
    for (auto & p : std::filesystem::directory_iterator(get_css_config_dir()))
    {
        if (p.path().extension() == ext)
        {
            int priority = GTK_STYLE_PROVIDER_PRIORITY_USER;
            if (p.path().filename() == "default.css")
            {
                priority = GTK_STYLE_PROVIDER_PRIORITY_APPLICATION;
            }

            add_css_file(p.path().string(), priority);
        }
    }

    /* Add one user file */
    auto custom_css_config = WfOption<std::string>{"panel/css_path"};
    std::string custom_css = custom_css_config;
    if (custom_css != "")
    {
        add_css_file(custom_css, GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
}

void WayfirePanelApp::clear_css_rules()
{
    auto display = Gdk::Display::get_default();
    for (auto css_provider : css_rules)
    {
        Gtk::StyleContext::remove_provider_for_display(display, css_provider);
    }

    css_rules.clear();
}

void WayfirePanelApp::add_css_file(std::string file, int priority)
{
    auto display = Gdk::Display::get_default();
    if (file != "")
    {
        auto css_provider = load_css_from_path(file);
        if (css_provider)
        {
            Gtk::StyleContext::add_provider_for_display(
                display, css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
            css_rules.push_back(css_provider);
        }
    }
}

void WayfirePanelApp::handle_new_output(WayfireOutput *output)
{
    priv->panels[output] = std::unique_ptr<WayfirePanel>(
        new WayfirePanel(output));
}

WayfirePanel*WayfirePanelApp::panel_for_wl_output(wl_output *output)
{
    for (auto& p : priv->panels)
    {
        if (p.first->wo == output)
        {
            return p.second.get();
        }
    }

    return nullptr;
}

void WayfirePanelApp::handle_output_removed(WayfireOutput *output)
{
    priv->panels.erase(output);
}

WayfirePanelApp& WayfirePanelApp::get()
{
    if (!instance)
    {
        throw std::logic_error("Calling WayfirePanelApp::get() before starting app!");
    }

    return dynamic_cast<WayfirePanelApp&>(*instance.get());
}

void WayfirePanelApp::create(int argc, char **argv)
{
    if (instance)
    {
        throw std::logic_error("Running WayfirePanelApp twice!");
    }

    instance = std::unique_ptr<WayfireShellApp>(new WayfirePanelApp{argc, argv});
    instance->run();
}

WayfirePanelApp::~WayfirePanelApp() = default;
WayfirePanelApp::WayfirePanelApp(int argc, char **argv) :
    WayfireShellApp(argc, argv), priv(new impl())
{}

int main(int argc, char **argv)
{
    WayfirePanelApp::create(argc, argv);
    return 0;
}
