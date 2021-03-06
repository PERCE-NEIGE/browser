/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <gtk/gtk.h>
#include "gailrange.h"
#include "gailadjustment.h"
#include "gail-private-macros.h"

static void	    gail_range_class_init        (GailRangeClass *klass);

static void         gail_range_real_initialize   (AtkObject      *obj,
                                                  gpointer      data);

static void         gail_range_finalize          (GObject        *object);
static AtkStateSet* gail_range_ref_state_set     (AtkObject      *obj);


static void         gail_range_real_notify_gtk   (GObject        *obj,
                                                  GParamSpec     *pspec);

static void	    atk_value_interface_init	 (AtkValueIface  *iface);
static void	    gail_range_get_current_value (AtkValue       *obj,
                                                  GValue         *value);
static void	    gail_range_get_maximum_value (AtkValue       *obj,
                                                  GValue         *value);
static void	    gail_range_get_minimum_value (AtkValue       *obj,
                                                  GValue         *value);
static gboolean	    gail_range_set_current_value (AtkValue       *obj,
                                                  const GValue   *value);
static void         gail_range_value_changed     (GtkAdjustment  *adjustment,
                                                  gpointer       data);
static GailWidgetClass *parent_class = NULL;

GType
gail_range_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (GailRangeClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gail_range_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GailRange), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };

      static const GInterfaceInfo atk_value_info =
      {
        (GInterfaceInitFunc) atk_value_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (GAIL_TYPE_WIDGET,
                                     "GailRange", &tinfo, 0);

      g_type_add_interface_static (type, ATK_TYPE_VALUE,
                                   &atk_value_info);
    }
  return type;
}

static void	 
gail_range_class_init		(GailRangeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  widget_class->notify_gtk = gail_range_real_notify_gtk;

  class->ref_state_set = gail_range_ref_state_set;
  class->initialize = gail_range_real_initialize;

  gobject_class->finalize = gail_range_finalize;

  parent_class = g_type_class_peek_parent (klass);
}

AtkObject* 
gail_range_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_RANGE (widget), NULL);

  object = g_object_new (GAIL_TYPE_RANGE, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  return accessible;
}

static void
gail_range_real_initialize (AtkObject *obj,
                            gpointer  data)
{
  GailRange *range = GAIL_RANGE (obj);
  GtkRange *gtk_range;

  ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

  gtk_range = GTK_RANGE (data);
  /*
   * If a GtkAdjustment already exists for the GtkRange,
   * create the GailAdjustment
   */
  if (gtk_range->adjustment)
    {
      range->adjustment = gail_adjustment_new (gtk_range->adjustment);
      g_signal_connect (gtk_range->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_range_value_changed),
                        range);
    }
  else
    range->adjustment = NULL;

  /*
   * Assumed to GtkScale (either GtkHScale or GtkVScale)
   */
  obj->role = ATK_ROLE_SLIDER;
}

static AtkStateSet*
gail_range_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;
  GtkRange *range;

  state_set = ATK_OBJECT_CLASS (parent_class)->ref_state_set (obj);
  widget = GTK_ACCESSIBLE (obj)->widget;

  if (widget == NULL)
    return state_set;

  range = GTK_RANGE (widget);

  /*
   * We do not generate property change for orientation change as there
   * is no interface to change the orientation which emits a notification
   */
  if (range->orientation == GTK_ORIENTATION_HORIZONTAL)
    atk_state_set_add_state (state_set, ATK_STATE_HORIZONTAL);
  else
    atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);

  return state_set;
}

static void	 
atk_value_interface_init (AtkValueIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_current_value = gail_range_get_current_value;
  iface->get_maximum_value = gail_range_get_maximum_value;
  iface->get_minimum_value = gail_range_get_minimum_value;
  iface->set_current_value = gail_range_set_current_value;

}

static void	 
gail_range_get_current_value (AtkValue		*obj,
                              GValue		*value)
{
  GailRange *range;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_current_value (ATK_VALUE (range->adjustment), value);
}

static void	 
gail_range_get_maximum_value (AtkValue		*obj,
                              GValue		*value)
{
  GailRange *range;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_maximum_value (ATK_VALUE (range->adjustment), value);
}

static void	 
gail_range_get_minimum_value (AtkValue		*obj,
                              GValue		*value)
{
  GailRange *range;

  g_return_if_fail (GAIL_IS_RANGE (obj));

  range = GAIL_RANGE (obj);
  if (range->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_value (ATK_VALUE (range->adjustment), value);
}

static gboolean	 gail_range_set_current_value (AtkValue		*obj,
                                               const GValue	*value)
{
  GtkWidget *widget;

  g_return_val_if_fail (GAIL_IS_RANGE (obj), FALSE);

  widget = GTK_ACCESSIBLE (obj)->widget;
  if (widget == NULL)
    return FALSE;

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      GtkRange *range = GTK_RANGE (widget);
      gdouble new_value;

      new_value = g_value_get_double (value);
      gtk_range_set_value (range, new_value);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gail_range_finalize (GObject            *object)
{
  GailRange *range = GAIL_RANGE (object);

  if (range->adjustment)
    {
      /*
       * The GtkAdjustment may live on so we need to dicsonnect the
       * signal handler
       */
      if (GAIL_ADJUSTMENT (range->adjustment)->adjustment)
        {
          g_signal_handlers_disconnect_by_func (GAIL_ADJUSTMENT (range->adjustment)->adjustment,
                                                (void *)gail_range_value_changed,
                                                range);
        }
      g_object_unref (range->adjustment);
      range->adjustment = NULL;
    }
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gail_range_real_notify_gtk (GObject           *obj,
                            GParamSpec        *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GailRange *range = GAIL_RANGE (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      /*
       * Get rid of the GailAdjustment for the GtkAdjustment
       * which was associated with the range.
       */
      if (range->adjustment)
        {
          g_object_unref (range->adjustment);
          range->adjustment = NULL;
        }
      /*
       * Create the GailAdjustment when notify for "adjustment" property
       * is received
       */
      range->adjustment = gail_adjustment_new (GTK_RANGE (widget)->adjustment);
      g_signal_connect (GTK_RANGE (widget)->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_range_value_changed),
                        range);
    }
  else
    parent_class->notify_gtk (obj, pspec);
}

static void
gail_range_value_changed (GtkAdjustment    *adjustment,
                          gpointer         data)
{
  GailRange *range;

  g_return_if_fail (adjustment != NULL);
  gail_return_if_fail (data != NULL);

  range = GAIL_RANGE (data);

  g_object_notify (G_OBJECT (range), "accessible-value");
}
