// Small GTK host. WebKit itself is supplied by the Linux distribution.
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QXmlStreamWriter>
#include <cstdio>
#undef signals
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <webkit2/webkit2.h>
struct Host { GtkWidget* plug; WebKitWebView* view; };
static void send(const QJsonObject& object){const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Compact);fwrite(bytes.constData(),1,bytes.size(),stdout);fputc('\n',stdout);fflush(stdout);}
// QWindow reparents the foreign window but does not implement GtkSocket's
// activation protocol. GTK needs both XEmbed activation and logical focus to
// paint the insertion caret (see the XEmbed specification, section 5.2).
static void xembed(Host* h,long message){
    auto* window=gtk_widget_get_window(h->plug);if(!window)return;
    auto* display=GDK_WINDOW_XDISPLAY(window);
    XEvent event{};event.xclient.type=ClientMessage;event.xclient.window=GDK_WINDOW_XID(window);
    event.xclient.message_type=XInternAtom(display,"_XEMBED",False);event.xclient.format=32;
    event.xclient.data.l[0]=CurrentTime;event.xclient.data.l[1]=message;
    XSendEvent(display,event.xclient.window,False,NoEventMask,&event);XFlush(display);
}
static void focus(Host* h,bool focused){
    if(focused){xembed(h,1);xembed(h,4);} // WINDOW_ACTIVATE, FOCUS_IN/CURRENT
    else xembed(h,5);                 // FOCUS_OUT
}
static void state(Host* h){
    const auto* uri=webkit_web_view_get_uri(h->view);const auto* title=webkit_web_view_get_title(h->view);
    send({{"event","state"},{"url",QString::fromUtf8(uri?uri:"")},{"title",QString::fromUtf8(title?title:"")},
        {"loading",bool(webkit_web_view_is_loading(h->view))},{"back",bool(webkit_web_view_can_go_back(h->view))},{"forward",bool(webkit_web_view_can_go_forward(h->view))}});
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const QByteArray base(argv[1]);
    // WebKit bug 325185: Skia loops forever on local WOFF cache entries
    // without a family. Scope the filter to this host and its web processes.
    const auto configPath=QString::fromUtf8(base)+"/fontconfig.conf";
    QDir().mkpath(QString::fromUtf8(base));
    QFile config(configPath);
    if(!config.open(QIODevice::WriteOnly|QIODevice::Truncate))return 2;
    QXmlStreamWriter xml(&config);xml.writeStartDocument();xml.writeStartElement("fontconfig");
    const auto inherited=qEnvironmentVariable("FONTCONFIG_FILE");
    xml.writeTextElement("include",inherited.isEmpty()?QStringLiteral("/etc/fonts/fonts.conf"):inherited);
    xml.writeStartElement("selectfont");xml.writeStartElement("rejectfont");
    xml.writeTextElement("glob","/usr/share/fonts/**/*.woff");
    xml.writeTextElement("glob","/usr/share/fonts/**/*.woff2");
    for(const auto* wrapper:{"WOFF","WOFF2"}){
        xml.writeStartElement("pattern");xml.writeStartElement("patelt");xml.writeAttribute("name","fontwrapper");
        xml.writeTextElement("string",QString::fromLatin1(wrapper));xml.writeEndElement();xml.writeEndElement();
    }
    xml.writeEndElement();xml.writeEndElement();xml.writeEndElement();xml.writeEndDocument();config.close();
    g_setenv("FONTCONFIG_FILE",configPath.toUtf8().constData(),TRUE);
    gdk_set_allowed_backends("x11");
    if(!gtk_init_check(&argc,&argv))return 2;
    auto* manager=webkit_website_data_manager_new("base-data-directory",(base+"/data").constData(),"base-cache-directory",(base+"/cache").constData(),nullptr);
    auto* context=webkit_web_context_new_with_website_data_manager(manager);
    Host h{gtk_plug_new(0),WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(context))};
    g_object_unref(context);g_object_unref(manager);
    gtk_container_add(GTK_CONTAINER(h.plug),GTK_WIDGET(h.view));
    g_signal_connect(h.view,"notify",G_CALLBACK(+[](GObject*,GParamSpec*,gpointer p){state(static_cast<Host*>(p));}),&h);
    g_signal_connect(h.view,"load-changed",G_CALLBACK(+[](WebKitWebView*,WebKitLoadEvent event,gpointer p){if(event==WEBKIT_LOAD_STARTED)send({{"event","started"}});state(static_cast<Host*>(p));}),&h);
    g_signal_connect(h.view,"load-failed",G_CALLBACK(+[](WebKitWebView*,WebKitLoadEvent,const gchar*,GError* error,gpointer)->gboolean{
        if(!g_error_matches(error,WEBKIT_NETWORK_ERROR,WEBKIT_NETWORK_ERROR_CANCELLED))send({{"event","error"},{"message",QString::fromUtf8(error->message)}});return TRUE;
    }),nullptr);
    g_signal_connect(h.view,"web-process-terminated",G_CALLBACK(+[](WebKitWebView*,WebKitWebProcessTerminationReason,gpointer){send({{"event","error"},{"message","O processo do navegador foi interrompido. Tente recarregar a página."}});}),nullptr);
    g_signal_connect(h.view,"decide-policy",G_CALLBACK(+[](WebKitWebView* view,WebKitPolicyDecision* decision,WebKitPolicyDecisionType type,gpointer)->gboolean{
        if(type!=WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)return FALSE;
        auto* action=webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
        const QByteArray uri(webkit_uri_request_get_uri(webkit_navigation_action_get_request(action)));
        webkit_policy_decision_ignore(decision);webkit_web_view_load_uri(view,uri.constData());return TRUE;
    }),nullptr);
    g_signal_connect(h.view,"button-press-event",G_CALLBACK(+[](GtkWidget*,GdkEventButton*,gpointer p)->gboolean{
        focus(static_cast<Host*>(p),true);return FALSE;
    }),&h);
    g_signal_connect(h.view,"focus-in-event",G_CALLBACK(+[](GtkWidget*,GdkEventFocus*,gpointer p)->gboolean{focus(static_cast<Host*>(p),true);send({{"event","focus"},{"focused",true}});return FALSE;}),&h);
    g_signal_connect(h.view,"focus-out-event",G_CALLBACK(+[](GtkWidget*,GdkEventFocus*,gpointer)->gboolean{send({{"event","focus"},{"focused",false}});return FALSE;}),nullptr);
    g_signal_connect(h.view,"key-press-event",G_CALLBACK(+[](GtkWidget*,GdkEventKey* event,gpointer)->gboolean{
        if((event->state&GDK_CONTROL_MASK)&&(event->keyval==GDK_KEY_l||event->keyval==GDK_KEY_L)){send({{"event","address"}});return TRUE;}return FALSE;
    }),nullptr);
    auto* input=g_io_channel_unix_new(0);
    g_io_add_watch(input,GIOCondition(G_IO_IN|G_IO_HUP|G_IO_ERR),+[](GIOChannel* channel,GIOCondition condition,gpointer p)->gboolean{
        if(condition&(G_IO_HUP|G_IO_ERR)){gtk_main_quit();return FALSE;}
        gchar* line=nullptr;gsize size=0;
        if(g_io_channel_read_line(channel,&line,&size,nullptr,nullptr)!=G_IO_STATUS_NORMAL)return TRUE;
        const auto command=QJsonDocument::fromJson(QByteArray(line,int(size))).object();g_free(line);
        auto* h=static_cast<Host*>(p);const auto op=command["op"].toString();
        if(op=="navigate")webkit_web_view_load_uri(h->view,command["url"].toString().toUtf8().constData());
        else if(op=="back")webkit_web_view_go_back(h->view);
        else if(op=="forward")webkit_web_view_go_forward(h->view);
        else if(op=="reload")webkit_web_view_reload(h->view);
        else if(op=="stop")webkit_web_view_stop_loading(h->view);
        else if(op=="focus")focus(h,command["focused"].toBool());
        else if(op=="active")xembed(h,command["active"].toBool()?1:2);
        else if(op=="size")gtk_window_resize(GTK_WINDOW(h->plug),command["width"].toInt(),command["height"].toInt());
        return TRUE;
    },&h);
    gtk_widget_show_all(h.plug);
    send({{"event","ready"},{"window",QString::number(gtk_plug_get_id(GTK_PLUG(h.plug)))}});
    gtk_main();g_io_channel_unref(input);gtk_widget_destroy(h.plug);return 0;
}
