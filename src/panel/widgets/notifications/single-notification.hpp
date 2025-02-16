#ifndef WIDGETS_SINGLE_NOTIFICATION_HPP
#define WIDGETS_SINGLE_NOTIFICATION_HPP

#include <gdkmm/pixbuf.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/revealer.h>

#include "notification-info.hpp"

class WfSingleNotification : public Gtk::Revealer
{
  public:
    static const int WIDTH = 300;

  private:
    Gtk::Box outer_box;
    Gtk::Button default_action_ev_box;

    /// The revealer's child, containing all other widgets
    Gtk::Box child;

    Gtk::Box top_bar;
    Gtk::Box content;

    Gtk::Image app_icon;
    Gtk::Label app_name;
    Gtk::Label time_label;
    sigc::connection time_label_update;
    Gtk::Button close_button;
    Gtk::Image close_image;

    Gtk::Label text;
    Gtk::Image image;

    Gtk::Box actions;

  public:
    explicit WfSingleNotification(const Notification & notification);
    ~WfSingleNotification() override;
};

#endif
