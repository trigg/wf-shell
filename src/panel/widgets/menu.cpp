#include <dirent.h>
#include <sstream>

#include <cassert>
#include <giomm.h>
#include <glibmm/spawn.h>
#include <iostream>
#include <gtk4-layer-shell.h>
#include <gtk/gtk.h>

#include "menu.hpp"
#include "gtk-utils.hpp"
#include "launchers.hpp"
#include "wf-autohide-window.hpp"

const std::string default_icon = "wayfire";

WfMenuCategory::WfMenuCategory(std::string _name, std::string _icon_name) :
    name(_name), icon_name(_icon_name)
{}

std::string WfMenuCategory::get_name()
{
    return name;
}

std::string WfMenuCategory::get_icon_name()
{
    return icon_name;
}

WfMenuCategoryButton::WfMenuCategoryButton(WayfireMenu *_menu, std::string _category, std::string _label,
    std::string _icon_name) :
    Gtk::Button(), menu(_menu), category(_category), label(_label), icon_name(_icon_name)
{
    m_image.set_from_icon_name(icon_name);
    m_image.set_pixel_size(32);
    m_label.set_text(label);
    m_label.set_xalign(0.0);

    m_box.append(m_image);
    m_box.append(m_label);
    m_box.set_homogeneous(false);

    this->set_child(m_box);
    this->get_style_context()->add_class("flat");
    this->get_style_context()->add_class("app-category");

    this->signal_clicked().connect(
        sigc::mem_fun(*this, &WfMenuCategoryButton::on_click));
}

void WfMenuCategoryButton::on_click()
{
    menu->set_category(category);
}

WfMenuMenuItem::WfMenuMenuItem(WayfireMenu *_menu, Glib::RefPtr<Gio::DesktopAppInfo> app) :
    Gtk::FlowBoxChild(), menu(_menu), m_app_info(app)
{
    m_image.set((const Glib::RefPtr<const Gio::Icon>&)app->get_icon());
    m_image.set_pixel_size(48);
    m_label.set_text(app->get_name());
    m_label.set_xalign(0.0);
    m_label.set_hexpand(true);
    m_has_actions = app->list_actions().size() > 0;
    m_button_box.append(m_image);
    m_button_box.append(m_label);

    m_button.set_child(m_button_box);
    m_button.signal_clicked().connect(
        [this](){
            this->on_click();
        }
    );
    m_padding_box.append(m_button);
    m_label.set_ellipsize(Pango::EllipsizeMode::END);
    m_button.get_style_context()->add_class("flat");


    if (menu->menu_list)
    {
        this->set_size_request(menu->menu_min_content_width, 48);
        if (m_has_actions)
        {
            auto action_button = new Gtk::Button();
            action_button->set_image_from_icon_name("pan-end");
            action_button->get_style_context()->add_class("flat");
            action_button->get_style_context()->add_class("app-button-extras");
            m_padding_box.append(*action_button);
            action_button->set_halign(Gtk::Align::END);

            action_button->signal_clicked().connect(
                [this] ()
            {
                // TODO Context menu
                //m_action_menu.popup_at_widget(this, Gdk::Gravity::NORTH_EAST, Gdk::Gravity::NORTH_WEST, NULL);
            });
        }
    }

    /*for (auto action : app->list_actions())
    {
        auto menu_item = new Gtk::MenuItem();
        menu_item->set_label(m_app_info->get_action_name(action));
        menu_item->show();
        menu_item->signal_activate().connect(
            [this, action] ()
        {
            m_app_info->launch_action(action);
            menu->hide_menu();
        });
        m_action_menu.append(*menu_item);
    }*/
    // TODO Fix Additional actions menu

    set_child(m_padding_box);
    get_style_context()->add_class("app-button");

    // TODO Right click
}

void WfMenuMenuItem::on_click()
{
    m_app_info->launch(std::vector<Glib::RefPtr<Gio::File>>());
    menu->hide_menu();
}

void WfMenuMenuItem::set_search_value(uint32_t value)
{
    m_search_value = value;
}

uint32_t WfMenuMenuItem::get_search_value()
{
    return m_search_value;
}

/* Fuzzy search for pattern in text. We use a greedy algorithm as follows:
 * As long as the pattern isn't matched, try to match the leftmost unmatched
 * character in pattern with the first occurence of this character after the
 * partial match. In the end, we just check if we successfully matched all
 * characters */
static bool fuzzy_match(Glib::ustring text, Glib::ustring pattern)
{
    size_t i = 0, // next character in pattern to match
        j    = 0; // the first unmatched character in text

    while (i < pattern.length() && j < text.length())
    {
        /* Found a match, advance both pointers */
        if (pattern[i] == text[j])
        {
            ++i;
            ++j;
        } else
        {
            /* Try to match current unmatched character in pattern with the next
             * character in text */
            ++j;
        }
    }

    /* If this happens, then we have already matched all characters */
    return i == pattern.length();
}

uint32_t WfMenuMenuItem::fuzzy_match(Glib::ustring pattern)
{
    uint32_t match_score = 0;
    Glib::ustring name   = m_app_info->get_name();
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring progr     = m_app_info->get_executable();

    auto name_lower = name.lowercase();
    auto long_name_lower = long_name.lowercase();
    auto progr_lower     = progr.lowercase();
    auto pattern_lower   = pattern.lowercase();

    if (::fuzzy_match(progr_lower, pattern_lower))
    {
        match_score += 100;
    }

    if (::fuzzy_match(name_lower, pattern_lower))
    {
        match_score += 100;
    }

    if (::fuzzy_match(long_name_lower, pattern_lower))
    {
        match_score += 10;
    }

    return match_score;
}

uint32_t WfMenuMenuItem::matches(Glib::ustring pattern)
{
    uint32_t match_score    = 0;
    Glib::ustring long_name = m_app_info->get_display_name();
    Glib::ustring name  = m_app_info->get_name();
    Glib::ustring progr = m_app_info->get_executable();
    Glib::ustring descr = m_app_info->get_description();

    auto name_lower = name.lowercase();
    auto long_name_lower = long_name.lowercase();
    auto progr_lower     = progr.lowercase();
    auto descr_lower     = descr.lowercase();
    auto pattern_lower   = pattern.lowercase();

    auto pos = name_lower.find(pattern_lower);
    if (pos != name_lower.npos)
    {
        match_score += 1000 - pos;
    }

    pos = progr_lower.find(pattern_lower);
    if (pos != progr_lower.npos)
    {
        match_score += 1000 - pos;
    }

    pos = long_name_lower.find(pattern_lower);
    if (pos != long_name_lower.npos)
    {
        match_score += 500 - pos;
    }

    pos = descr_lower.find(pattern_lower);
    if (pos != descr_lower.npos)
    {
        match_score += 300 - pos;
    }

    return match_score;
}

bool WfMenuMenuItem::operator <(const WfMenuMenuItem& other)
{
    return Glib::ustring(m_app_info->get_name()).lowercase() <
           Glib::ustring(other.m_app_info->get_name()).lowercase();
}

void WayfireMenu::load_menu_item(AppInfo app_info)
{
    if (!app_info)
    {
        return;
    }
    if (app_info->get_nodisplay())
    {
        return;
    }

    auto name = app_info->get_name();
    auto exec = app_info->get_executable();
    /* If we don't have the following, then the entry won't be useful anyway,
     * so we should skip it */
    if (name.empty() || !app_info->get_icon() || exec.empty())
    {
        return;
    }

    /* Already created such a launcher, skip */
    if (loaded_apps.count({name, exec}))
    {
        return;
    }

    loaded_apps.insert({name, exec});

    /* Check if this has a 'OnlyShownIn' for a different desktop env
    *  If so, we throw it in a pile at the bottom just to be safe */
    if (!app_info->should_show())
    {
        add_category_app("Hidden", app_info);
        return;
    }

    add_category_app("All", app_info);

    /* Split the Categories, iterate to place into submenus */
    std::stringstream categories_stream(app_info->get_categories());
    std::string segment;

    while (std::getline(categories_stream, segment, ';'))
    {
        add_category_app(segment, app_info);
    }
}

void WayfireMenu::add_category_app(std::string category, AppInfo app)
{
    /* Filter for allowed categories */
    if (category_list.count(category) == 1)
    {
        category_list[category]->items.push_back(app);
    }
}

void WayfireMenu::populate_menu_categories()
{
    // Ensure the category list is empty
    for (auto child : category_box.get_children())
    {
        category_box.remove(*child);
        //child->destroy();
        //gtk_widget_destroy(GTK_WIDGET(child->gobj()));
    }

    // Iterate allowed categories in order
    for (auto category_name : category_order)
    {
        if (category_list.count(category_name) == 1)
        {
            auto& category = category_list[category_name];
            if (category->items.size() > 0)
            {
                auto icon_name = category->get_icon_name();
                auto name = category->get_name();
                auto category_button = new WfMenuCategoryButton(this, category_name, name, icon_name);
                category_box.append(*category_button);
            }
        } else
        {
            std::cerr << "Category in orderlist without Category object : " << category << std::endl;
        }
    }

}

void WayfireMenu::populate_menu_items(std::string category)
{
    /* Ensure the flowbox is empty */
    for (auto child : flowbox.get_children())
    {
        flowbox.remove(*child);
        //gtk_widget_destroy(GTK_WIDGET(child->gobj()));
    }

    for (auto app_info : category_list[category]->items)
    {
        auto app = new WfMenuMenuItem(this, app_info);
        flowbox.append(*app);
    }

}

static bool ends_with(std::string text, std::string pattern)
{
    if (text.length() < pattern.length())
    {
        return false;
    }

    return text.substr(text.length() - pattern.length()) == pattern;
}

void WayfireMenu::load_menu_items_from_dir(std::string path)
{
    /* Expand path */
    auto dir = opendir(path.c_str());
    if (!dir)
    {
        return;
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

        auto fullpath = path + "/" + file->d_name;

        if (ends_with(fullpath, ".desktop"))
        {
            load_menu_item(Gio::DesktopAppInfo::create_from_filename(fullpath));
        }
    }
}

void WayfireMenu::load_menu_items_all()
{
    std::string home_dir = getenv("HOME");
    auto app_list = Gio::AppInfo::get_all();
    for (auto app : app_list)
    {
        auto desktop_app = std::dynamic_pointer_cast<Gio::DesktopAppInfo>(app);
        load_menu_item(desktop_app);
    }

    load_menu_items_from_dir(home_dir + "/Desktop");
}

void WayfireMenu::on_search_changed()
{
    search_label.set_text(search_contents);
    if (menu_show_categories)
    {
        if (search_contents.length() == 0)
        {
            /* Text has been unset, show categories again */
            populate_menu_items(category);
            category_scrolled_window.show();
            app_scrolled_window.set_min_content_width(int(menu_min_content_width));
        } else
        {
            /* User is filtering, hide categories, ignore chosen category */
            populate_menu_items("All");
            category_scrolled_window.hide();
            app_scrolled_window.set_min_content_width(int(menu_min_content_width) +
                int(menu_min_category_width));
        }
    }

    m_sort_names  = search_contents.length() == 0;
    fuzzy_filter  = false;
    count_matches = 0;
    flowbox.unselect_all();
    flowbox.invalidate_filter();
    flowbox.invalidate_sort();

    /* We got no matches, try to fuzzy-match */
    if ((count_matches <= 0) && fuzzy_search_enabled)
    {
        fuzzy_filter = true;
        flowbox.unselect_all();
        flowbox.invalidate_filter();
        flowbox.invalidate_sort();
    }

    select_first_flowbox_item();
}

bool WayfireMenu::on_filter(Gtk::FlowBoxChild *child)
{
    auto button = dynamic_cast<WfMenuMenuItem*>(child);
    assert(button);

    auto text = search_contents;
    uint32_t match_score = this->fuzzy_filter ?
        button->fuzzy_match(text) : button->matches(text);

    button->set_search_value(match_score);
    if (match_score > 0)
    {
        this->count_matches++;
        return true;
    }

    return false;
}

bool WayfireMenu::on_sort(Gtk::FlowBoxChild *a, Gtk::FlowBoxChild *b)
{
    auto b1 = dynamic_cast<WfMenuMenuItem*>(a);
    auto b2 = dynamic_cast<WfMenuMenuItem*>(b);
    assert(b1 && b2);

    if (m_sort_names)
    {
        return *b2 < *b1;
    }

    return b2->get_search_value() > b1->get_search_value();
}

void WayfireMenu::on_popover_shown()
{
    search_contents = "";
    on_search_changed();
    set_category("All");
    flowbox.unselect_all();
}

bool WayfireMenu::update_icon()
{
    std::string icon;
    if (((std::string)menu_icon).empty())
    {
        icon = default_icon;
    } else
    {
        icon = menu_icon;
    }

    main_image.set_from_icon_name(icon);
    return true;
}

void WayfireMenu::update_popover_layout()
{
    /* First time updating layout, need to setup everything */
    if (popover_layout_box.get_parent() == nullptr)
    {
        button->get_popover()->set_child(popover_layout_box);

        flowbox.set_selection_mode(Gtk::SelectionMode::SINGLE);
        flowbox.set_activate_on_single_click(true);
        flowbox.set_valign(Gtk::Align::START);
        flowbox.set_homogeneous(true);
        flowbox.set_sort_func(sigc::mem_fun(*this, &WayfireMenu::on_sort));
        flowbox.set_filter_func(sigc::mem_fun(*this, &WayfireMenu::on_filter));
        flowbox.get_style_context()->add_class("app-list");

        flowbox_container.append(bottom_pad);
        flowbox_container.append(flowbox);

        scroll_pair.append(category_scrolled_window);
        scroll_pair.append(app_scrolled_window);
        scroll_pair.set_homogeneous(false);

        app_scrolled_window.set_min_content_width(int(menu_min_content_width));
        app_scrolled_window.set_min_content_height(int(menu_min_content_height));
        app_scrolled_window.set_child(flowbox_container);
        app_scrolled_window.get_style_context()->add_class("app-list-scroll");
        app_scrolled_window.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);

        category_box.get_style_context()->add_class("category-list");

        category_scrolled_window.set_min_content_width(int(menu_min_category_width));
        category_scrolled_window.set_min_content_height(int(menu_min_content_height));
        category_scrolled_window.set_child(category_box);
        category_scrolled_window.get_style_context()->add_class("categtory-list-scroll");
        category_scrolled_window.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);

        search_logo.set_from_icon_name("search");
        search_box.get_style_context()->add_class("app-search");
        search_box.set_halign(Gtk::Align::START);

        search_box.append(search_logo);
        search_box.append(search_label);

        auto typing_gesture = Gtk::EventControllerKey::create();
        typing_gesture->signal_key_pressed().connect([=] ( guint keyval, guint keycode, Gdk::ModifierType state)
        {
            if (keyval == GDK_KEY_Escape)
            {
                return false;
            } else if (keyval == GDK_KEY_BackSpace)
            {
                if(search_contents.length() > 0){
                    search_contents.pop_back();
                }
                on_search_changed();
            } else if (keyval == GDK_KEY_Return)
            {
                auto children = flowbox.get_selected_children();
                if (children.size() == 1)
                {
                    auto child = dynamic_cast<WfMenuMenuItem*>(children[0]);
                    child->on_click();
                    return true;
                }
            } else
            {
                std::string input = gdk_keyval_name(keyval);
                if (input.length() == 1){
                    search_contents = search_contents + input;
                    std::cout << keyval << " SHOULD GO TO TEXT BOX " << gdk_keyval_name(keyval)  << std::endl;
                    on_search_changed();
                    return true;
                }

            }
            return false;
        }, false);
        popover_layout_box.add_controller(typing_gesture);
    } else
    {
        /* Layout was already initialized, make sure to remove widgets before
         * adding them again */
        popover_layout_box.remove(search_box);
        popover_layout_box.remove(scroll_pair);
        popover_layout_box.remove(separator);
        popover_layout_box.remove(hbox_bottom);
    }

    if ((std::string)panel_position == WF_WINDOW_POSITION_TOP)
    {
        popover_layout_box.append(search_box);
        popover_layout_box.append(scroll_pair);
        popover_layout_box.append(separator);
        popover_layout_box.append(hbox_bottom);

    } else
    {
        popover_layout_box.append(scroll_pair);
        popover_layout_box.append(search_box);
        popover_layout_box.append(separator);
        popover_layout_box.append(hbox_bottom);

    }


    if (!menu_show_categories)
    {
        category_scrolled_window.hide();
    }
}

void WayfireLogoutUI::on_logout_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(logout_command).c_str(), NULL);
}

void WayfireLogoutUI::on_reboot_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(reboot_command).c_str(), NULL);
}

void WayfireLogoutUI::on_shutdown_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(shutdown_command).c_str(), NULL);
}

void WayfireLogoutUI::on_suspend_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(suspend_command).c_str(), NULL);
}

void WayfireLogoutUI::on_hibernate_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(hibernate_command).c_str(), NULL);
}

void WayfireLogoutUI::on_switchuser_click()
{
    ui.hide();
    bg.hide();
    g_spawn_command_line_async(std::string(switchuser_command).c_str(), NULL);
}

void WayfireLogoutUI::on_cancel_click()
{
    ui.hide();
    bg.hide();
}

#define LOGOUT_BUTTON_SIZE  125
#define LOGOUT_BUTTON_MARGIN 10

void WayfireLogoutUI::create_logout_ui_button(WayfireLogoutUIButton *button, const char *icon,
    const char *label)
{
    button->button.set_size_request(LOGOUT_BUTTON_SIZE, LOGOUT_BUTTON_SIZE);
    button->image.set_from_icon_name(icon);
    button->label.set_text(label);
    button->layout.append(button->image);
    button->layout.append(button->label);
    button->button.set_child(button->layout);
}

WayfireLogoutUI::WayfireLogoutUI()
{
    create_logout_ui_button(&suspend, "emblem-synchronizing", "Suspend");
    suspend.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_suspend_click));
    top_layout.append(suspend.button);

    create_logout_ui_button(&hibernate, "weather-clear-night", "Hibernate");
    hibernate.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_hibernate_click));
    top_layout.append(hibernate.button);

    create_logout_ui_button(&switchuser, "system-users", "Switch User");
    switchuser.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_switchuser_click));
    top_layout.append(switchuser.button);

    create_logout_ui_button(&logout, "system-log-out", "Log Out");
    logout.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_logout_click));
    middle_layout.append(logout.button);

    create_logout_ui_button(&reboot, "system-reboot", "Reboot");
    reboot.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_reboot_click));
    middle_layout.append(reboot.button);

    create_logout_ui_button(&shutdown, "system-shutdown", "Shut Down");
    shutdown.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_shutdown_click));
    middle_layout.append(shutdown.button);

    cancel.button.set_size_request(100, 50);
    cancel.button.set_label("Cancel");
    bottom_layout.append(cancel.button);
    cancel.button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireLogoutUI::on_cancel_click));

    top_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    middle_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    bottom_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.set_spacing(LOGOUT_BUTTON_MARGIN);
    main_layout.append(top_layout);
    main_layout.append(middle_layout);
    main_layout.append(bottom_layout);
    /* Work around spacing bug for immediate child of window */
    hspacing_layout.append(main_layout);
    vspacing_layout.append(hspacing_layout);
    /* Make surfaces layer shell */
    gtk_layer_init_for_window(bg.gobj());
    gtk_layer_set_layer(bg.gobj(), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_exclusive_zone(bg.gobj(), -1);
    gtk_layer_init_for_window(ui.gobj());
    gtk_layer_set_layer(ui.gobj(), GTK_LAYER_SHELL_LAYER_OVERLAY);
    ui.set_child(vspacing_layout);
    bg.set_opacity(0.5);
    auto display = ui.get_display();
    auto css_provider  = Gtk::CssProvider::create();
    bg.set_name("logout_background");
    css_provider->load_from_data("window#logout_background { background-color: black; }");
    Gtk::StyleContext::add_provider_for_display(display,
        css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void WayfireMenu::on_logout_click()
{
    button->get_popover()->hide();
    if (!std::string(menu_logout_command).empty())
    {
        g_spawn_command_line_async(std::string(menu_logout_command).c_str(), NULL);
        return;
    }

    /* If no command specified for logout, show our own logout window */
    logout_ui->ui.present();

    /* Set the background window to the same size of the screen */
    //logout_ui->bg.set_size_request(gdk_screen->get_width(), gdk_screen->get_height());
}

void WayfireMenu::refresh()
{
    loaded_apps.clear();
    for (auto& [key, category] : category_list)
    {
        category->items.clear();
    }

    for (auto child : flowbox.get_children())
    {
        flowbox.remove(*child);
    }

    load_menu_items_all();
    populate_menu_categories();
    populate_menu_items("All");
}

static void app_info_changed(GAppInfoMonitor *gappinfomonitor, gpointer user_data)
{
    WayfireMenu *menu = (WayfireMenu*)user_data;

    menu->refresh();
}

void WayfireMenu::init(Gtk::Box *container)
{
    /* https://specifications.freedesktop.org/menu-spec/latest/apa.html#main-category-registry
     * Using the 'Main' categories, with names and icons assigned
     * Any Categories in .desktop files that are not in this list are ignored */
    category_list["All"]     = std::make_unique<WfMenuCategory>("All", "applications-other");
    category_list["Network"] = std::make_unique<WfMenuCategory>("Internet",
        "applications-internet");
    category_list["Education"] = std::make_unique<WfMenuCategory>("Education",
        "applications-education");
    category_list["Office"] = std::make_unique<WfMenuCategory>("Office",
        "applications-office");
    category_list["Development"] = std::make_unique<WfMenuCategory>("Development",
        "applications-development");
    category_list["Graphics"] = std::make_unique<WfMenuCategory>("Graphics",
        "applications-graphics");
    category_list["AudioVideo"] = std::make_unique<WfMenuCategory>("Multimedia",
        "applications-multimedia");
    category_list["Game"] = std::make_unique<WfMenuCategory>("Games",
        "applications-games");
    category_list["Science"] = std::make_unique<WfMenuCategory>("Science",
        "applications-science");
    category_list["Settings"] = std::make_unique<WfMenuCategory>("Settings",
        "preferences-desktop");
    category_list["System"] = std::make_unique<WfMenuCategory>("System",
        "applications-system");
    category_list["Utility"] = std::make_unique<WfMenuCategory>("Accessories",
        "applications-utilities");
    category_list["Hidden"] = std::make_unique<WfMenuCategory>("Other Desktops",
        "user-desktop");

    main_image.get_style_context()->add_class("menu-icon");

    output->toggle_menu_signal().connect(sigc::mem_fun(*this, &WayfireMenu::toggle_menu));

    menu_icon.set_callback([=] () { update_icon(); });
    menu_size.set_callback([=] () { update_icon(); });
    menu_min_category_width.set_callback([=] () { update_category_width(); });
    menu_min_content_height.set_callback([=] () { update_content_height(); });
    menu_min_content_width.set_callback([=] () { update_content_width(); });
    panel_position.set_callback([=] () { update_popover_layout(); });
    menu_show_categories.set_callback([=] () { update_popover_layout(); });
    menu_list.set_callback([=] () { update_popover_layout(); });

    button = std::make_unique<WayfireMenuButton>("panel");
    button->set_child(main_image);
    auto style = button->get_style_context();
    style->add_class("menu-button");
    style->add_class("flat");
    //button->get_popover()->set_constrain_to(Gtk::POPOVER_CONSTRAINT_NONE);
    button->get_popover()->get_style_context()->add_class("menu-popover");
    button->get_popover()->signal_show().connect(
        sigc::mem_fun(*this, &WayfireMenu::on_popover_shown));

    if (!update_icon())
    {
        return;
    }

    button->property_scale_factor().signal_changed().connect(
        [=] () {update_icon(); });

    container->append(hbox);
    hbox.append(*button);

    logout_button.set_image_from_icon_name("system-shutdown");
    logout_button.signal_clicked().connect(
        sigc::mem_fun(*this, &WayfireMenu::on_logout_click));
    //logout_button.property_margin().set_value(20);
    //logout_button.set_margin_right(35);
    hbox_bottom.append(logout_button);

    popover_layout_box.set_orientation(Gtk::Orientation::VERTICAL);

    logout_ui = std::make_unique<WayfireLogoutUI>();

    load_menu_items_all();
    update_popover_layout();
    populate_menu_categories();
    populate_menu_items("All");


    app_info_monitor_changed_handler_id =
        g_signal_connect(app_info_monitor, "changed", G_CALLBACK(app_info_changed), this);

    hbox.show();
    main_image.show();
    button->show();
}

void WayfireMenu::update_category_width()
{
    category_scrolled_window.set_min_content_width(int(menu_min_category_width));
}

void WayfireMenu::update_content_height()
{
    category_scrolled_window.set_min_content_height(int(menu_min_content_height));
    app_scrolled_window.set_min_content_height(int(menu_min_content_height));
}

void WayfireMenu::update_content_width()
{
    app_scrolled_window.set_min_content_width(int(menu_min_content_width));
}

void WayfireMenu::toggle_menu()
{
    search_contents="";
    if (button->get_active())
    {
        button->set_active(false);
    } else
    {
        button->set_active(true);
    }
}

void WayfireMenu::hide_menu()
{
    button->set_active(false);
}

void WayfireMenu::set_category(std::string in_category)
{
    category = in_category;
    populate_menu_items(in_category);
}
void WayfireMenu::select_first_flowbox_item()
{
    for (auto child : flowbox.get_children())
    {
        if (child->get_mapped())
        {
            Gtk::FlowBoxChild *cast_child = dynamic_cast<Gtk::FlowBoxChild*>(child);
            if (cast_child)
            {
                flowbox.select_child(*cast_child);
                cast_child->grab_focus();
                return;
            }
        }
    }
}
