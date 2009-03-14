/*
 * Geeqie
 * Copyright (C) 2008 - 2009 The Geeqie Team
 *
 * Author: Vladimir Nadvornik
 * based on a patch by Uwe Ohse
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "histogram.h"

#include "pixbuf_util.h"

#include <math.h>

/*
 *----------------------------------------------------------------------------
 * image histogram
 *----------------------------------------------------------------------------
 */

#define HISTMAP_SIZE 256

struct _HistMap {
	gulong r[HISTMAP_SIZE];
	gulong g[HISTMAP_SIZE];
	gulong b[HISTMAP_SIZE];
	gulong max[HISTMAP_SIZE];
};


Histogram *histogram_new(void)
{
	Histogram *histogram;

	histogram = g_new0(Histogram, 1);
	histogram->histogram_channel = HCHAN_RGB;
	histogram->histogram_mode = 0;

	/* grid */
	histogram->vgrid = 5;
	histogram->hgrid = 3;
	histogram->grid_color.R	= 160;
	histogram->grid_color.G	= 160;
	histogram->grid_color.B	= 160;
	histogram->grid_color.A	= 250;

	return histogram;
}

void histogram_free(Histogram *histogram)
{
	g_free(histogram);
}


gint histogram_set_channel(Histogram *histogram, gint chan)
{
	if (!histogram) return 0;
	histogram->histogram_channel = chan;
	return chan;
}

gint histogram_get_channel(Histogram *histogram)
{
	if (!histogram) return 0;
	return histogram->histogram_channel;
}

gint histogram_set_mode(Histogram *histogram, gint mode)
{
	if (!histogram) return 0;
	histogram->histogram_mode = mode;
	return mode;
}

gint histogram_get_mode(Histogram *histogram)
{
	if (!histogram) return 0;
	return histogram->histogram_mode;
}

gint histogram_toggle_channel(Histogram *histogram)
{
	if (!histogram) return 0;
	return histogram_set_channel(histogram, (histogram_get_channel(histogram)+1)%HCHAN_COUNT);
}

gint histogram_toggle_mode(Histogram *histogram)
{
	if (!histogram) return 0;
	return histogram_set_mode(histogram, !histogram_get_mode(histogram));
}

const gchar *histogram_label(Histogram *histogram)
{
	const gchar *t1 = "";
	
	if (!histogram) return NULL;

	if (histogram->histogram_mode)
		switch (histogram->histogram_channel)
			{
			case HCHAN_R:   t1 = _("Log Histogram on Red"); break;
			case HCHAN_G:   t1 = _("Log Histogram on Green"); break;
			case HCHAN_B:   t1 = _("Log Histogram on Blue"); break;
			case HCHAN_RGB: t1 = _("Log Histogram on RGB"); break;
			case HCHAN_MAX: t1 = _("Log Histogram on value"); break;
			}
	else
		switch (histogram->histogram_channel)
			{
			case HCHAN_R:   t1 = _("Linear Histogram on Red"); break;
			case HCHAN_G:   t1 = _("Linear Histogram on Green"); break;
			case HCHAN_B:   t1 = _("Linear Histogram on Blue"); break;
			case HCHAN_RGB: t1 = _("Linear Histogram on RGB"); break;
			case HCHAN_MAX: t1 = _("Linear Histogram on value"); break;
			}
	return t1;
}

static HistMap *histmap_read(GdkPixbuf *imgpixbuf)
{
	gint w, h, i, j, srs, has_alpha, step;
	guchar *s_pix;
	HistMap *histmap;
	
	w = gdk_pixbuf_get_width(imgpixbuf);
	h = gdk_pixbuf_get_height(imgpixbuf);
	srs = gdk_pixbuf_get_rowstride(imgpixbuf);
	s_pix = gdk_pixbuf_get_pixels(imgpixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha(imgpixbuf);

	histmap = g_new0(HistMap, 1);

	step = 3 + !!(has_alpha);
	for (i = 0; i < h; i++)
		{
		guchar *sp = s_pix + (i * srs); /* 8bit */
		for (j = 0; j < w; j++)
			{
			guint max = sp[0];
			if (sp[1] > max) max = sp[1];
			if (sp[2] > max) max = sp[2];
		
			histmap->r[sp[0]]++;
			histmap->g[sp[1]]++;
			histmap->b[sp[2]]++;
			histmap->max[max]++;

			sp += step;
			}
		}
	
	return histmap;
}

const HistMap *histmap_get(FileData *fd)
{
	if (fd->histmap) return fd->histmap;
	
	if (fd->pixbuf)
		{
		fd->histmap = histmap_read(fd->pixbuf);
		return fd->histmap;
		}
	return NULL;
}

static void histogram_vgrid(Histogram *histogram, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height)
{
	guint i;
	float add;
	
	if (histogram->vgrid == 0) return;

	add = width / (float)histogram->vgrid;

	for (i = 1; i < histogram->vgrid; i++)
		{
		gint xpos = x + (int)(i * add + 0.5);

		pixbuf_draw_line(pixbuf, x, y, width, height, xpos, y, xpos, y + height,
				 histogram->grid_color.R,
				 histogram->grid_color.G,
				 histogram->grid_color.B,
				 histogram->grid_color.A);
		}
}

static void histogram_hgrid(Histogram *histogram, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height)
{
	guint i;
	float add;
	
	if (histogram->hgrid == 0) return;

	add = height / (float)histogram->hgrid;

	for (i = 1; i < histogram->hgrid; i++)
		{
		gint ypos = y + (int)(i * add + 0.5);
	
		pixbuf_draw_line(pixbuf, x, y, width, height, x, ypos, x + width, ypos,
				 histogram->grid_color.R,
				 histogram->grid_color.G,
				 histogram->grid_color.B,
				 histogram->grid_color.A);
		}
}

gboolean histogram_draw(Histogram *histogram, const HistMap *histmap, GdkPixbuf *pixbuf, gint x, gint y, gint width, gint height)
{
	/* FIXME: use the coordinates correctly */
	gint i;
	gulong max = 0;
	gdouble logmax;
	gint combine = (HISTMAP_SIZE - 1) / width + 1;
	gint ypos = y + height;
	
	if (!histogram || !histmap) return FALSE;
	
	/* Draw the grid */
	histogram_vgrid(histogram, pixbuf, x, y, width, height);
	histogram_hgrid(histogram, pixbuf, x, y, width, height);

	for (i = 0; i < HISTMAP_SIZE; i++)
		{
		if (histmap->r[i] > max) max = histmap->r[i];
		if (histmap->g[i] > max) max = histmap->g[i];
		if (histmap->b[i] > max) max = histmap->b[i];
		}

	if (max > 0)
		logmax = log(max);
	else
		logmax = 1.0;

	for (i = 0; i < width; i++)
		{
		gint j;
		glong v[4] = {0, 0, 0, 0};
		gint rplus = 0;
		gint gplus = 0;
		gint bplus = 0;
		gint ii = i * HISTMAP_SIZE / width;
		gint xpos = x + i;

		for (j = 0; j < combine; j++)
			{
			guint p = ii + j;
			v[0] += histmap->r[p];
			v[1] += histmap->g[p];
			v[2] += histmap->b[p];
			v[3] += histmap->max[p];
			}
	
		for (j = 0; combine > 1 && j < 4; j++)
			v[j] /= combine;
		
		for (j = 0; j < 4; j++)
			{
			gint chanmax = HCHAN_R;
		
			if (v[HCHAN_G] > v[HCHAN_R]) chanmax = HCHAN_G;
			if (v[HCHAN_B] > v[HCHAN_G]) chanmax = HCHAN_B;
				
			if (histogram->histogram_channel >= HCHAN_RGB
			    || chanmax == histogram->histogram_channel)
			    	{
				gulong pt;
				gint r = rplus;
				gint g = gplus;
				gint b = bplus;

				switch (chanmax)
					{
					case HCHAN_R: rplus = r = 255; break;
					case HCHAN_G: gplus = g = 255; break;
					case HCHAN_B: bplus = b = 255; break;
					}

				switch (histogram->histogram_channel)
					{
					case HCHAN_RGB:
						if (r == 255 && g == 255 && b == 255)
							{
							r = 0; 	b = 0; 	g = 0;
							}
						break;
					case HCHAN_R:	  	b = 0; 	g = 0; 	break;
					case HCHAN_G:   r = 0; 	b = 0;		break;
					case HCHAN_B:   r = 0;		g = 0; 	break;
					case HCHAN_MAX: r = 0; 	b = 0; 	g = 0; 	break;
					}
				
				if (v[chanmax] == 0)
					pt = 0;
				else if (histogram->histogram_mode)
					pt = ((gdouble)log(v[chanmax])) / logmax * (height - 1);
				else
					pt = ((gdouble)v[chanmax]) / max * (height - 1);

				pixbuf_draw_line(pixbuf,
					x, y, width, height,
					xpos, ypos, xpos, ypos - pt,
					r, g, b, 255);
				}

			v[chanmax] = -1;
			}
		}

	return TRUE;
}

void histogram_notify_cb(FileData *fd, NotifyType type, gpointer data)
{
	if ((type & (NOTIFY_CHANGE || NOTIFY_REREAD)) && fd->histmap)
		{
		g_free(fd->histmap);
		fd->histmap = NULL;
		}
}

/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
