/*
  giface - tries to give GUIs to command line tools using Glade
  Copyright (C) 2007   Mauro Panigada

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307  USA
*/

/*
** giface 1.0
**
** 2007-05-28            ended level-1 interface support.
**
**
** NOTE: it would be useful if you can specify the userdata in the handlers
**       but it seems you can't, at least using Glade!
**
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>


/* Maximum command line length */
#define CMDLINE_LEN 4096

/* size of "interim" buffers */
#define INTERIM_BUF_LEN 256

/* strings definitions for prefixes (and postfixes) */
#define PVOID "void_"
#define PTEXT "text_"
#define POPT "opt_"
#define PACTIVATOR "activator_"
#define PFILECHOOSER "_filechooser"

/* widget names */
#define CMD_WIDGET "commandname"
#define SHOWOUTPUT_WIDGET "giface_showoutput"
#define INTERACTIVE_WIDGET "giface_interactive"


/* the Glade XML "hook" */
GladeXML *xml;

/* cmd line buffer */
gchar cmdlinebuf[CMDLINE_LEN];

/* a flag for future use... interface level */
int iflevel=1;


/* show output? default false */
gboolean showoutput = FALSE;
/* interactive? default false */
gboolean interact = FALSE;


/* prototypes */
void addincmdline(const gchar *);
void addincmdline0(const gchar *);
void efunc(gpointer, gpointer);
void unghosted(GtkWidget*, gboolean);
void unghosthand(GtkWidget*, gpointer);
void giface_toggle_active(GtkWidget *, gpointer);
void giface_update_entry(GtkWidget *, gpointer);
void giface_emit_commandline(GtkWidget *, gpointer);
void giface_execute_commandline(GtkWidget *, gpointer);



/* add the string into the cmdline buffer and add a space at the end */
void addincmdline(const gchar *s)
{
    strncat(cmdlinebuf, s, CMDLINE_LEN);
    strncat(cmdlinebuf, " ", CMDLINE_LEN);
    cmdlinebuf[CMDLINE_LEN-1]=0;
}

/* the same as addincmdline, but without trailing space */
void addincmdline0(const gchar *s)
{
    strncat(cmdlinebuf, s, CMDLINE_LEN);
    cmdlinebuf[CMDLINE_LEN-1]=0;
}


/*
  this is the hook function used to get widgets option+[value]
  or value (for void_); the list of widgets with specific prefix
  is got by glade, and then this function is called for each element of
  the list.
*/
void efunc(gpointer data, gpointer ud)
{
    const char *name = NULL;

    /* if the widget is not sensitive, skip it */
    if ( !GTK_WIDGET_SENSITIVE((GtkWidget*)data) ) return;

    /* gets the name of the widget */
    name = glade_get_widget_name((GtkWidget*)data);

    /* if the widget is a GtkEntry but not a GtkSpinButton (since a spin
       button is a "child" of the GtkEntry class) then ... */
    if (GTK_IS_ENTRY((GtkWidget*)data) && !GTK_IS_SPIN_BUTTON((GtkWidget*)data)) {
        /* get the content of the entry */
        const gchar *te=gtk_entry_get_text((GtkEntry*)data);
        /* entry with 0-long content are not used: so writing a string inside
           an entry means to put the related option */
        if (strlen(te)>0) {
            /* if the name of the widget start with void_, the entry content
               must be put on the commandline stream as is */
            if ( strncmp(name, PVOID, strlen(PVOID))==0 ) {
                addincmdline0("\""); /* adds quoting so that spaces are not a problem */
                addincmdline0(te);
                addincmdline("\"");
            } else if ( strncmp(name, PTEXT, strlen(PTEXT))==0) {
                /* if the name of the widget begin with "text_", skip this prefix,
                   find the last _ after text_ (hopefully this is not part of
                   the option... FIXME) and then use this string fragment as
                   option, followed by the quoted content of the entry */
                gchar buf[INTERIM_BUF_LEN];
                strncpy(buf,name+strlen(PTEXT),INTERIM_BUF_LEN);
                char *ri = rindex(buf,'_');
                addincmdline(ri==NULL?buf:ri+1);
                addincmdline0("\"");
                addincmdline0(te);
                addincmdline("\"");
            }
        }
    } else if (GTK_IS_SPIN_BUTTON((GtkWidget*)data)) {
        /* if we have a spin button... update it (needed?), then
           get its integer value (FIXME why integer? this should be specified
           somewhere, then we need a level-2 interface to provide this extra
           info
        */
        gtk_spin_button_update((GtkSpinButton*)data);
        gint val = gtk_spin_button_get_value_as_int((GtkSpinButton*)data);
        gchar buf[INTERIM_BUF_LEN], bbuf[INTERIM_BUF_LEN];
        strncpy(buf,name+strlen(PTEXT),INTERIM_BUF_LEN); /* 5 -> skip text_/void_ */
        /* NOTE the above line works for both VOID and TEXT if they have
           the same length, as is if their values are void_ and text_ */
        char *ri = rindex(buf,'_'); /* find the last _ if any; this is the trick
                                       for same option / different widget name
                                       we have seen before (FIXME it can cause
                                       problems if an option contain _) */
        /* i would prefer a len check for this sprintf, but it would be
           hard to do... */
        sprintf(bbuf,"%s %d", ri==NULL?buf:ri+1, val);
        addincmdline(bbuf);
    } else if (strncmp(name,POPT,strlen(POPT))==0) {
        /* no gtk entry nor gtk spin, if the widget name starts with opt_
           check if is a toggle (ie checkbutton ie radiobutton; gtk toggle
           button test should hit the children too as gtkentry and gtkspinbutton,
           so the OR could be deleted) */
        if ( GTK_IS_TOGGLE_BUTTON((GtkWidget*)data)||GTK_IS_CHECK_BUTTON((GtkWidget*)data)||GTK_IS_RADIO_BUTTON((GtkWidget*)data)) {
            /* according to the status of the toggle, skip the opt_ and
               add (if the toggle is on) the option */
            if ( gtk_toggle_button_get_active((GtkToggleButton*)data) )
            {
                gchar buf[INTERIM_BUF_LEN];
                strncpy(buf,name+strlen(POPT),INTERIM_BUF_LEN);
                char *ri = rindex(buf,'_');
                addincmdline(ri==NULL?buf:ri+1);
            }
        }
    } else if (GTK_IS_COMBO_BOX((GtkWidget*)data)) {
        /* it is not an entry, it does not start with opt_, check if it is
           a combo, if it is, get its active content text */
        const gchar *cb;
        cb = gtk_combo_box_get_active_text((GtkComboBox*)data);
        if ( cb!=NULL) {
            gchar buf[INTERIM_BUF_LEN];
            strncpy(buf,name+strlen(PTEXT),INTERIM_BUF_LEN);
            char *ri = rindex(buf,'_'); /* the usual trick to be fixed if ... */
            addincmdline(ri==NULL?buf:ri+1);
            addincmdline(cb);
            /* FIXME combo box text values should be surrounded by quotes? */
        }
    } else {
        /* we cannot handle the widget and then we print it out
           TODO check carefully all kind of widget that can be used and hold
           a value for an option.
        */
        fprintf(stderr,"widget name %s\n", name);
    }
}


/* handler for iterator; it simply call unghosted over its argument */
void unghosthand(GtkWidget *w, gpointer dat)
{
    if (w==NULL) return;
    unghosted(w, *((gboolean *)dat));
}

/* should this widget be unghosted (=sensitive) or not? */
void unghosted(GtkWidget *w, gboolean T)
{
    /* if the widget is a container, call the function over its contents,
       since disabling/enabling the only container do not prevent the option to be
       added/skipped; then, make the container (in)sensitive too */
    if (GTK_IS_CONTAINER(w)) {
        gtk_container_foreach((GtkContainer*)w, unghosthand, (gpointer)&T);
        gtk_widget_set_sensitive(w,T);
    } else {
        /* if it is a simple widget, just enable/disable it */
        gtk_widget_set_sensitive(w,T);
    }
}


/*
  handler for disabling/enabling according to a toggle widget.
  the widget passed as argument is the one we want to ghost/unghost;
  the relation with the toggle is found by naming convention:
  prefix_name -> activator_name
  NOTE the prefix must exist i.e. at least one _ must exist in the
  name of the widget to be disable/enabled! A better convention
  should be thought
*/
void giface_toggle_active(GtkWidget *w, gpointer ud)
{
    gchar buf[INTERIM_BUF_LEN];
    const char *part;
    GtkWidget *rapp;

    if (w==NULL) return;
    strncpy(buf, PACTIVATOR, INTERIM_BUF_LEN);
    part = index(glade_get_widget_name(w),'_');
    part++;
    strncat(buf,part,INTERIM_BUF_LEN);
    rapp=glade_xml_get_widget(xml,buf);
    if (rapp==NULL) return;
    if ( gtk_toggle_button_get_active((GtkToggleButton*)rapp))
    {
        unghosted(w,TRUE);
    } else {
        unghosted(w,FALSE);
    }
}


/*
  handler for updating entry according to the content value of a
  file chooser.
  the widget of the argument is the widget to be updated, with
  name of the form prefix_name.
  The file chooser widget must have name_filechooser as name.
  NOTE a prefix ending with _ must exist i.e. at least one _ must
  exist in the name of the widget given as argument!
  (see giface_toggle_active)
*/
void giface_update_entry(GtkWidget *widget, gpointer user_data)
{
    const char *name; gchar buf[INTERIM_BUF_LEN];
    char *part;
    GtkWidget *w;

    if (widget==NULL) return;

    name = glade_get_widget_name(widget);
    part = index(name,'_');
    part++;
    strncpy(buf,part,INTERIM_BUF_LEN);
    strncat(buf,PFILECHOOSER,INTERIM_BUF_LEN);
    w = glade_xml_get_widget(xml,buf);
    if (w!=NULL)
    {
        const gchar *pt = gtk_file_chooser_get_filename((GtkFileChooser*)w);
        if ( pt != NULL )
            gtk_entry_set_text((GtkEntry*)widget,pt);
    }
}



/* execute the command line; attach this handler to a clicked event of
   a ok button or something like that */
void giface_execute_commandline(GtkWidget *w, gpointer user_data)
{
    GList *thelist;
    GtkWidget *cn, *app;

    if ( (app=glade_xml_get_widget(xml,SHOWOUTPUT_WIDGET))!=NULL )
    {
        showoutput = gtk_toggle_button_get_active((GtkToggleButton*)app);
        if (showoutput)
        {
            fprintf(stderr,"Show output inside a window not implemented yet\n");
            /* TODO implement it */
        }
    }
    if ( (app=glade_xml_get_widget(xml,INTERACTIVE_WIDGET))!=NULL )
    {
        interact = gtk_toggle_button_get_active((GtkToggleButton*)app);
        if (interact)
        {
            fprintf(stderr,"Interaction not implemented yet\n");
            /* TODO implement it */
        }
    }

    cn = glade_xml_get_widget(xml,CMD_WIDGET);
    if ( cn==NULL )
    {
        fprintf(stderr,"Cannot execute command line: you must provide a command!\n");
        gtk_main_quit();
    }

    if ( GTK_IS_ENTRY(cn) ) {
        addincmdline(gtk_entry_get_text((GtkEntry*)cn));
    } else {
        fprintf(stderr,"Problem with commandname widget... I try to go on\n");
    }

    /* the same as _emit_commandline... TODO put them into a function */
    thelist=glade_xml_get_widget_prefix(xml,PVOID);
    g_list_foreach(thelist,efunc,user_data);
    g_list_free(thelist);
    thelist=glade_xml_get_widget_prefix(xml,PTEXT);
    g_list_foreach(thelist,efunc,user_data);
    g_list_free(thelist);
    thelist=glade_xml_get_widget_prefix(xml,POPT);
    g_list_foreach(thelist,efunc,user_data);
    g_list_free(thelist);

    /* the command line should be ok, now we must execute it... */
    system(cmdlinebuf);
    /* regardless interaction and showoutput since they are not
       implemented! */

    gtk_main_quit();
}



/* "emits" the command line to the standard output */
void giface_emit_commandline(GtkWidget *widget, gpointer user_data)
{
    GList *thelist;
    GtkWidget *cn;

    /* get the content of the commandname widget that should be an entry... */
    cn = glade_xml_get_widget(xml,CMD_WIDGET);
    if ( cn==NULL ) {
        fprintf(stderr,"No command line\n");
        gtk_main_quit();
    }
    if ( GTK_IS_ENTRY(cn) ) {
        addincmdline(gtk_entry_get_text((GtkEntry*)cn));
    } else {
        fprintf(stderr,"Problem with commandname widget... I try to go on\n");
    }

    /* pass thru the lists of prefixed widget names and call efunc on
       each element...
       Order: first the void (value without an option before; NOTE sometime
       you need the opposite, ie these should be the last!)
       then the option pairs "option value",
       then the switches.
    */
    thelist=glade_xml_get_widget_prefix(xml,PVOID);
    g_list_foreach(thelist,efunc,user_data);
    g_list_free(thelist);
    thelist=glade_xml_get_widget_prefix(xml,PTEXT);
    g_list_foreach(thelist,efunc,user_data);
    g_list_free(thelist);
    thelist=glade_xml_get_widget_prefix(xml,POPT);
    g_list_foreach(thelist,efunc,user_data);
    g_list_free(thelist);
    /* emits command line and exit */
    printf("%s\n", cmdlinebuf);
    gtk_main_quit();
}




int main(int argc, char *argv[])
{
    if ( argc <= 1 ) { exit(1); }
    /* TODO level-2 interfaces need two arguments... but it is not
       implemented yet! */
    if ( argc > 2 )
    {
        fprintf(stderr,"It seems you want to use level-2 interface,\n"
                "but this version still haven't its support!\n");
        exit(1);
    }
    cmdlinebuf[0] = 0;

    gtk_init(&argc, &argv);

    /* load the interface */
    xml = glade_xml_new(argv[1], NULL, NULL);
    /* let glade warn for failing */
    if (xml==NULL) exit(1);

    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(xml);

    /* start the event loop */
    gtk_main();

    return 0;
}
