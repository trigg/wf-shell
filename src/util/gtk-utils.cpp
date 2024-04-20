#include <gtk-utils.hpp>
#include <glibmm.h>
#include <gtkmm/icontheme.h>
#include <gdk/gdkcairo.h>
#include <iostream>

Glib::RefPtr<Gdk::Pixbuf> load_icon_pixbuf_safe(std::string icon_path, int size)
{
    try {
        auto pb = Gdk::Pixbuf::create_from_file(icon_path, size, size);
        return pb;
    } catch (Glib::FileError&)
    {
        std::cerr << "Error loading file: " << icon_path << std::endl;
        return {};
    } catch (Gdk::PixbufError&)
    {
        std::cerr << "Pixbuf error: " << icon_path << std::endl;
        return {};
    } catch (...)
    {
        std::cerr << "Failed to load: " << icon_path << std::endl;
        return {};
    }
}

Glib::RefPtr<Gtk::CssProvider> load_css_from_path(std::string path)
{
    try {
        auto css = Gtk::CssProvider::create();
        css->load_from_path(path);
        return css;
    } catch (Glib::Error& err)
    {
        std::cerr << "Failed to load CSS: " << err.what() << std::endl;
        return {};
    } catch (...)
    {
        std::cerr << "Failed to load CSS at: " << path << std::endl;
        return {};
    }
}

void invert_pixbuf(Glib::RefPtr<Gdk::Pixbuf>& pbuff)
{
    int channels = pbuff->get_n_channels();
    int stride   = pbuff->get_rowstride();

    auto data = pbuff->get_pixels();
    int w     = pbuff->get_width();
    int h     = pbuff->get_height();

    for (int i = 0; i < w; i++)
    {
        for (int j = 0; j < h; j++)
        {
            auto p = data + j * stride + i * channels;
            p[0] = 255 - p[0];
            p[1] = 255 - p[1];
            p[2] = 255 - p[2];
        }
    }
}

void set_image_pixbuf(Gtk::Image & image, Glib::RefPtr<Gdk::Pixbuf> pixbuf, int scale)
{
    auto pbuff = pixbuf->gobj();
    auto cairo_surface = gdk_cairo_surface_create_from_pixbuf(pbuff, scale, NULL);

    gtk_image_set_from_surface(image.gobj(), cairo_surface);
    cairo_surface_destroy(cairo_surface);
}

void set_image_icon(Gtk::Image& image, std::string icon_name, int size,
    const WfIconLoadOptions& options,
    const Glib::RefPtr<Gtk::IconTheme>& icon_theme)
{
    int scale = ((options.user_scale == -1) ?
        image.get_scale_factor() : options.user_scale);
    int scaled_size = size * scale;

    if (!icon_theme->lookup_icon(icon_name, scaled_size))
    {
        std::cerr << "Failed to load icon \"" << icon_name << "\"" << std::endl;
        return;
    }

    auto pbuff = icon_theme->load_icon(icon_name, scaled_size, Gtk::ICON_LOOKUP_FORCE_SIZE)
        ->scale_simple(scaled_size, scaled_size, Gdk::INTERP_BILINEAR);

    if (options.invert)
    {
        invert_pixbuf(pbuff);
    }

    set_image_pixbuf(image, pbuff, scale);
}

Gtk::IconSize get_icon_size(int size)
{
    if (size < 1)
    {
        size = 1;
    }
    auto icon_size = Gtk::IconSize::from_name("icon_size_"+size);
    if (!icon_size)
    {
        icon_size = Gtk::IconSize::register_new("icon_size_"+size, size, size);
    }
    return icon_size;
}

void set_image_gicon(Gtk::Image& icon, std::string icon_name, int size)
{
    if (size < 1)
    {
        size = 1;
    }
    std::string absolute_path = "/";
    if (!icon_name.compare(0, absolute_path.size(), absolute_path))
    {
        auto gfile = Gio::File::create_for_path(icon_name);
        auto gicon_raw = g_file_icon_new(gfile->gobj());
        auto gicon = Glib::wrap(gicon_raw);
        icon.set((const Glib::RefPtr<const Gio::Icon>) gicon, get_icon_size(size));
        return;
    }
    icon.set_from_icon_name(icon_name, get_icon_size(size));
    icon.set_pixel_size(size);
    return;
}