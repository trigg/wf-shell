#ifndef WIDGETS_BATTERY_HPP
#define WIDGETS_BATTERY_HPP

#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>

#include "../widget.hpp"

using DBusConnection = Glib::RefPtr<Gio::DBus::Connection>;
using DBusProxy = Glib::RefPtr<Gio::DBus::Proxy>;

static const std::string BATTERY_STATUS_ICON    = "icon"; // icon
static const std::string BATTERY_STATUS_PERCENT = "percentage"; // icon + percentage
static const std::string BATTERY_STATUS_FULL    = "full"; // icon + percentage + TimeToFull/TimeToEmpty

class wayfire_config;
class WayfireBatteryInfo : public WayfireWidget
{
    WfOption<std::string> status_opt{"panel/battery_status"};
    WfOption<std::string> font_opt{"panel/battery_font"};
    WfOption<bool> invert_opt{"panel/battery_icon_invert"};

    Gtk::Button button;
    Gtk::Label label;
    Gtk::Box button_box;

    Gtk::Image icon;

    DBusConnection connection;
    DBusProxy upower_proxy, display_device;

    bool setup_dbus();

    void update_font();
    void update_icon();
    void update_details();
    void update_state();

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

  public:
    virtual void init(Gtk::Box *container);
    virtual ~WayfireBatteryInfo() = default;
};


#endif /* end of include guard: WIDGETS_BATTERY_HPP */
