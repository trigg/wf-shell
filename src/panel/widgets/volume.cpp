#include <gtkmm.h>
#include <iostream>
#include <glibmm.h>
#include "volume.hpp"
#include "launchers.hpp"
#include "gtk-utils.hpp"
#include "../../util/gtk-utils.hpp"

WayfireVolumeScale::WayfireVolumeScale()
{
    value_changed = this->signal_value_changed().connect([=] ()
    {
        this->current_volume.animate(this->get_value(), this->get_value());
        if (this->user_changed_callback)
        {
            this->user_changed_callback();
        }
    });
}

void WayfireVolumeScale::set_target_value(double value)
{
    this->current_volume.animate(value);
    add_tick_callback(sigc::mem_fun(*this, &WayfireVolumeScale::update_animation));
}

gboolean WayfireVolumeScale::update_animation(Glib::RefPtr<Gdk::FrameClock> frame_clock)
{
    value_changed.block();
    this->set_value(this->current_volume);
    value_changed.unblock();
    // Once we've finished fading, stop this callback
    return this->current_volume.running() ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

double WayfireVolumeScale::get_target_value() const
{
    return this->current_volume.end;
}

void WayfireVolumeScale::set_user_changed_callback(
    std::function<void()> callback)
{
    this->user_changed_callback = callback;
}

enum VolumeLevel
{
    VOLUME_LEVEL_MUTE = 0,
    VOLUME_LEVEL_LOW,
    VOLUME_LEVEL_MED,
    VOLUME_LEVEL_HIGH,
    VOLUME_LEVEL_OOR, /* Out of range */
};

static VolumeLevel get_volume_level(pa_volume_t volume, pa_volume_t max)
{
    auto third = max / 3;
    if (volume == 0)
    {
        return VOLUME_LEVEL_MUTE;
    } else if ((volume > 0) && (volume <= third))
    {
        return VOLUME_LEVEL_LOW;
    } else if ((volume > third) && (volume <= (third * 2)))
    {
        return VOLUME_LEVEL_MED;
    } else if ((volume > (third * 2)) && (volume <= max))
    {
        return VOLUME_LEVEL_HIGH;
    }

    return VOLUME_LEVEL_OOR;
}

void WayfireVolume::update_menu()
{
    bool muted_mic = (gvc_stream_mic && gvc_mixer_stream_get_is_muted(gvc_stream_mic));
    bool muted_speaker = (gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream));
    microphone_action->set_state(Glib::Variant<bool>::create(muted_mic));
    speaker_action->set_state(Glib::Variant<bool>::create(muted_speaker));
}

void WayfireVolume::update_icon()
{
    VolumeLevel current =
        get_volume_level(volume_scale.get_target_value(), max_norm);

    if (gvc_stream && gvc_mixer_stream_get_is_muted(gvc_stream))
    {
        main_image.set_from_icon_name("audio-volume-muted");
        return;
    }

    std::map<VolumeLevel, std::string> icon_name_from_state = {
        {VOLUME_LEVEL_MUTE, "audio-volume-muted"},
        {VOLUME_LEVEL_LOW, "audio-volume-low"},
        {VOLUME_LEVEL_MED, "audio-volume-medium"},
        {VOLUME_LEVEL_HIGH, "audio-volume-high"},
        {VOLUME_LEVEL_OOR, "audio-volume-muted"},
    };

    main_image.set_from_icon_name(icon_name_from_state.at(current));
}

bool WayfireVolume::on_popover_timeout(int timer)
{
    popover.popdown();
    return false;
}

void WayfireVolume::check_set_popover_timeout()
{
    popover_timeout.disconnect();

    popover_timeout = Glib::signal_timeout().connect(sigc::bind(sigc::mem_fun(*this,
        &WayfireVolume::on_popover_timeout), 0), timeout * 1000);
}

void WayfireVolume::set_volume(pa_volume_t volume, set_volume_flags_t flags)
{
    volume_scale.set_target_value(volume);
    if (gvc_stream && (flags & VOLUME_FLAG_UPDATE_GVC))
    {
        gvc_mixer_stream_set_volume(gvc_stream, volume);
        gvc_mixer_stream_push_volume(gvc_stream);
    }

    if (flags & VOLUME_FLAG_SHOW_POPOVER)
    {
        if (!popover.is_visible())
        {
            popover.popup();
        } else
        {
            check_set_popover_timeout();
        }
    }

    update_icon();
}

void WayfireVolume::on_volume_changed_external()
{
    auto volume = gvc_mixer_stream_get_volume(gvc_stream);
    if (volume != (pa_volume_t)this->volume_scale.get_target_value())
    {
        set_volume(volume, VOLUME_FLAG_SHOW_POPOVER);
    }

    check_set_popover_timeout();
}

static void notify_volume(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->on_volume_changed_external();
}

static void notify_is_muted(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->update_icon();
    wf_volume->update_menu();
}

static void notify_is_mic_muted(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->update_menu();
}

void WayfireVolume::disconnect_gvc_stream_signals()
{
    if (notify_volume_signal)
    {
        g_signal_handler_disconnect(gvc_stream, notify_volume_signal);
    }

    notify_volume_signal = 0;

    if (notify_is_muted_signal)
    {
        g_signal_handler_disconnect(gvc_stream, notify_is_muted_signal);
    }

    notify_is_muted_signal = 0;
}

void WayfireVolume::disconnect_gvc_stream_signals_mic()
{
    if (notify_is_mic_muted_signal)
    {
        g_signal_handler_disconnect(gvc_stream_mic, notify_is_mic_muted_signal);
    }
    notify_is_mic_muted_signal = 0;
}

void WayfireVolume::on_default_sink_changed()
{
    gvc_stream = gvc_mixer_control_get_default_sink(gvc_control);
    if (!gvc_stream)
    {
        printf("GVC: Failed to get default sink\n");
        return;
    }

    /* Reconnect signals to new sink */
    disconnect_gvc_stream_signals();
    notify_volume_signal = g_signal_connect(gvc_stream, "notify::volume",
        G_CALLBACK(notify_volume), this);
    notify_is_muted_signal = g_signal_connect(gvc_stream, "notify::is-muted",
        G_CALLBACK(notify_is_muted), this);

    /* Update the scale attributes */
    max_norm = gvc_mixer_control_get_vol_max_norm(gvc_control);
    volume_scale.set_range(0.0, max_norm);
    volume_scale.set_increments(max_norm * scroll_sensitivity,
        max_norm * scroll_sensitivity * 2);

    /* Finally, update the displayed volume. However, do not show the
     * popup */
    set_volume(gvc_mixer_stream_get_volume(gvc_stream), VOLUME_FLAG_NO_ACTION);
    update_menu();
}

void WayfireVolume::on_default_source_changed()
{
    gvc_stream_mic = gvc_mixer_control_get_default_source(gvc_control);
    if (!gvc_stream_mic)
    {
        printf("GVC: Failed to get default source\n");
        return;
    }

    disconnect_gvc_stream_signals_mic();
    notify_is_mic_muted_signal = g_signal_connect(gvc_stream_mic, "notify::is-muted",
        G_CALLBACK(notify_is_mic_muted), this);

    update_menu();
}

static void default_sink_changed(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->on_default_sink_changed();
}

static void default_source_changed(GvcMixerControl *gvc_control,
    guint id, gpointer user_data)
{
    WayfireVolume *wf_volume = (WayfireVolume*)user_data;
    wf_volume->on_default_source_changed();    
}

void WayfireVolume::on_volume_value_changed()
{
    /* User manually changed volume */
    set_volume(volume_scale.get_target_value());
}

void WayfireVolume::init(Gtk::Box *container)
{
    /* Setup button */
    button.signal_clicked().connect([=]
    {
        if (!popover.is_visible())
        {
            popover.popup();
        } else
        {
            popover.popdown();
        }
    });
    auto style = button.get_style_context();
    style->add_class("volume");
    style->add_class("flat");

    gtk_widget_set_parent(GTK_WIDGET(popover.gobj()), GTK_WIDGET(button.gobj()));
    gtk_widget_set_parent(GTK_WIDGET(second_popover.gobj()), GTK_WIDGET(button.gobj()));

    auto scroll_gesture = Gtk::EventControllerScroll::create();
    scroll_gesture->signal_scroll().connect([=] (double dx, double dy)
    {
        int change = 0;
        if (scroll_gesture->get_unit() == Gdk::ScrollUnit::WHEEL)
        {
            // +- number of clicks.
            change = dy * max_norm * scroll_sensitivity;
        } else
        {
            // Number of pixels expected to have scrolled. usually in 100s
            change = (dy / 100.0) * max_norm * scroll_sensitivity;
        }

        set_volume(std::clamp(volume_scale.get_target_value() - change,
            0.0, max_norm));
        return true;
    }, true);
    scroll_gesture->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);

    auto click_gesture = Gtk::GestureClick::create();
    click_gesture->set_button(3); // Only use right-click for this callback
    click_gesture->signal_pressed().connect([=] (int count, double x, double y)
    {
        if (!second_popover.is_visible())
        {
            second_popover.popup();
        } else
        {
            second_popover.popdown();
        }
    });

    button.add_controller(scroll_gesture);
    button.add_controller(click_gesture);

    /* Setup popover */
    popover.set_autohide(false);
    popover.set_child(volume_scale);
    // popover->set_modal(false);
    popover.get_style_context()->add_class("volume-popover");
    popover.add_controller(scroll_gesture);

    volume_scale.set_draw_value(false);
    volume_scale.set_size_request(300, 0);
    volume_scale.set_user_changed_callback([=] () { on_volume_value_changed(); });

    volume_scale.signal_state_flags_changed().connect(
        [=] (Gtk::StateFlags) { check_set_popover_timeout(); });

    /* Setup gvc control */
    gvc_control = gvc_mixer_control_new("Wayfire Volume Control");
    notify_default_sink_changed = g_signal_connect(gvc_control,
        "default-sink-changed", G_CALLBACK(default_sink_changed), this);
    notify_default_source_changed = g_signal_connect(gvc_control,
        "default-source-changed", G_CALLBACK(default_source_changed), this);
    gvc_mixer_control_open(gvc_control);

    /* Setup Menu */
    auto menu = Gio::Menu::create();
    auto menu_item_settings = Gio::MenuItem::create("Settings", "action.settings");
    auto menu_item_toggle_volume = Gio::MenuItem::create("Mute Speaker", "action.togglespeaker");
    auto menu_item_toggle_mic = Gio::MenuItem::create("Mute Microphone", "action.togglemicrophone");

    menu->append_item(menu_item_toggle_volume);
    menu->append_item(menu_item_toggle_mic);
    menu->append_item(menu_item_settings);

    second_popover.set_menu_model(menu);

    /* Menu Actions*/
    actions = Gio::SimpleActionGroup::create();

    auto settings_action = Gio::SimpleAction::create("settings");
    speaker_action = Gio::SimpleAction::create_bool("togglespeaker", false);
    microphone_action = Gio::SimpleAction::create_bool("togglemicrophone", false);

    settings_action->signal_activate().connect([=] (Glib::VariantBase vb)
    {
        std::vector<std::string> volume_programs = {"pavucontrol", "mate-volume-control"};

        Glib::RefPtr<Gio::DesktopAppInfo> app_info;
        std::string value = volume_settings;
        if (value == "")
        {
            // Auto guess
            for(auto program : volume_programs)
            {
                if(executable_exists(program))
                {
                    auto keyfile = Glib::KeyFile::create();
                    keyfile->set_string("Desktop Entry", "Type", "Application");
                    keyfile->set_string("Desktop Entry", "Exec", "/bin/sh -c \"" + program + "\"");
                    app_info = Gio::DesktopAppInfo::create_from_keyfile(keyfile);
                    break;                   
                }
            }
            
        } else 
        {
            auto keyfile = Glib::KeyFile::create();
            keyfile->set_string("Desktop Entry", "Type", "Application");
            keyfile->set_string("Desktop Entry", "Exec", "/bin/sh -c \"" + value + "\"");
            app_info = Gio::DesktopAppInfo::create_from_keyfile(keyfile);
        }

        if (app_info)
        {
            auto ctx = Gdk::Display::get_default()->get_app_launch_context();
            app_info->launch(std::vector<Glib::RefPtr<Gio::File>>(), ctx);
        }
    });

    speaker_action->signal_activate().connect([=] (Glib::VariantBase vb)
    {
        if (gvc_stream)
        {        
            bool muted = !gvc_mixer_stream_get_is_muted(gvc_stream);
            gvc_mixer_stream_change_is_muted(gvc_stream, muted);
            gvc_mixer_stream_push_volume(gvc_stream);

        }    
    });

    microphone_action->signal_activate().connect([=] (Glib::VariantBase vb)
    {
        if (gvc_stream_mic)
        {
            bool muted = !(gvc_stream_mic && gvc_mixer_stream_get_is_muted(gvc_stream_mic));
            gvc_mixer_stream_change_is_muted(gvc_stream_mic, muted);
            gvc_mixer_stream_push_volume(gvc_stream_mic);

        }
    });

    actions->add_action(speaker_action);
    actions->add_action(microphone_action);
    actions->add_action(settings_action);

    button.insert_action_group("action", actions);

    /* Setup layout */
    container->append(button);
    button.set_child(main_image);
}

WayfireVolume::~WayfireVolume()
{
    gtk_widget_unparent(GTK_WIDGET(popover.gobj()));
    disconnect_gvc_stream_signals();

    if (notify_default_sink_changed)
    {
        g_signal_handler_disconnect(gvc_control, notify_default_sink_changed);
    }
    if (notify_default_source_changed)
    {
        g_signal_handler_disconnect(gvc_control, notify_default_source_changed);
    }

    gvc_mixer_control_close(gvc_control);
    g_object_unref(gvc_control);

    popover_timeout.disconnect();
}
