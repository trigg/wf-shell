#include "tray.hpp"

void WayfireStatusNotifier::init(Gtk::HBox *container)
{
    icons_hbox.get_style_context()->add_class("tray");
    icons_hbox.set_spacing(5);
    container->pack_start(icons_hbox, false, false);
}

void WayfireStatusNotifier::add_item(const Glib::ustring & service)
{
    if (items.count(service) != 0)
    {
        return;
    }

    items.emplace(service, service);
    icons_hbox.pack_start(items.at(service), false, false);
    icons_hbox.show_all();
}

void WayfireStatusNotifier::remove_item(const Glib::ustring & service)
{
    items.erase(service);
}
