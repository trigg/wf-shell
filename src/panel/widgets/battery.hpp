#ifndef WIDGETS_BATTERY_HPP
#define WIDGETS_BATTERY_HPP

#include <gtkmm/menubutton.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/popovermenu.h>


#include <giomm/dbusproxy.h>
#include <giomm/dbusconnection.h>
#include <giomm/menu.h>
#include <giomm/simpleaction.h>
#include <giomm/simpleactiongroup.h>
#include <giomm/file.h>
#include <giomm/desktopappinfo.h>

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
    WfOption<std::string> battery_settings{"panel/battery_settings"};

    Gtk::MenuButton button;
    Gtk::Label label;
    Gtk::Box button_box;

    Gtk::Image icon;
    Gtk::PopoverMenu popover;

    std::shared_ptr<Gio::Menu> menu;
    std::shared_ptr<Gio::Menu> profiles_menu;

    std::shared_ptr<Gio::SimpleAction> state_action, settings_action;


    DBusConnection connection;
    DBusProxy upower_proxy, powerprofile_proxy, display_device;

    bool setup_dbus();

    void update_icon();
    void update_details();
    void update_state();

    void on_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

    void on_upower_properties_changed(
        const Gio::DBus::Proxy::MapChangedProperties& properties,
        const std::vector<Glib::ustring>& invalidated);

    void set_current_profile(Glib::ustring profile);

    void setup_profiles(std::vector<std::map<Glib::ustring, Glib::VariantBase>> profiles);

    bool executable_exists(std::string name);

  public:
    virtual void init(Gtk::Box *container);
    virtual ~WayfireBatteryInfo() = default;
};


#endif /* end of include guard: WIDGETS_BATTERY_HPP */
