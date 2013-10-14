#include <iksemel.h>
#include <stdio.h>
#include "xmpp-lib.h"
#include <syslog-ng.h>

typedef int (*xmpp_node_handler)(void* sess, iks* node);

struct _xmpp_session
{
    iksparser* stream;
    char* sid;
    char* account_name;
    char* account_password;
    iksid* account;
    int authorized;
    iksfilter* filter;
    xmpp_node_handler handlers[IKS_NODE_STOP+1];
    int setup_done;
};

int handle_xmpp_features(xmpp_session* session, iks* node)
{
    iks *t;
    int features = iks_stream_features (node);
    printf("Features node arrived features='%d'\n", features);
    if (!session->authorized)
    {
       printf("Starting SASL\n");
       session->account = iks_id_new(iks_parser_stack (session->stream), session->account_name);
       printf("Using password %s\n", session->account_password);
       iks_start_sasl (session->stream, IKS_SASL_PLAIN, session->account->user, session->account_password);
    }
    else
    {
       printf("Finishing authorization\n");
       if (features & IKS_STREAM_BIND) {
          t = iks_make_resource_bind (session->account);
          iks_send (session->stream, t);
          iks_delete (t);
       }
       if (features & IKS_STREAM_SESSION) {
          t = iks_make_session ();
          iks_insert_attrib (t, "id", "auth");
          iks_send (session->stream, t);
          iks_delete (t);
       }

    }

}

int xmpp_session_started(xmpp_session* session, iks* node)
{
   session->sid = strdup(iks_find_attrib (node, "id"));
   printf("Stream started, id='%s'\n", session->sid);
}

int start_node_handler(xmpp_session* session, iks* node)
{
    //iks_start_tls(sess->stream);
    xmpp_session_started(session, node);
    return IKS_OK;
};

int normal_node_handler(xmpp_session* session, iks* node)
{
    if (strcmp ("stream:features", iks_name (node)) == 0)
    {
       handle_xmpp_features(session, node);
    }
    else if (strcmp ("success", iks_name (node)) == 0)
    {
       iks_send_header (session->stream, session->account->server);
       session->authorized = 1;
    }
    else {
       ikspak *pak;
       pak = iks_packet (node);
       iks_filter_packet (session->filter, pak);
    }
    return IKS_OK;
};

int error_node_handler(xmpp_session* session, iks* node)
{
    return IKS_HOOK;
};

int stop_node_handler(xmpp_session* session, iks* node)
{
    return IKS_OK;
};

int xmpp_stream_hook(void *userdata, int type, iks* node)
{
    xmpp_session* session = (xmpp_session*) userdata;
    printf("Node got, type: %d\n", type);
    int res = session->handlers[type](session, node);
    iks_delete(node);
    return res;
}

void
xmpp_log_hook (xmpp_session *sess, const char *data, size_t size, int is_incoming)
{
    if (iks_is_secure (sess->stream)) printf ("Secured ");
    if (is_incoming) printf ("RECV "); else printf ("SEND ");
    printf ("[%s]\n", data);
}

int xmpp_send_msg(xmpp_session *sess, char* to, char* msg)
{
   iks* msgpak = iks_make_msg(IKS_TYPE_CHAT, to, msg);
   iks_send(sess->stream, msgpak);
};

int on_auth_success(xmpp_session *sess, ikspak *pak)
{
   sess->setup_done = 1;
   return IKS_FILTER_EAT;
};

void setup_filter(xmpp_session* sess)
{
   sess->filter = iks_filter_new ();
   iks_filter_add_rule (sess->filter, (iksFilterHook *) on_auth_success, sess,
         IKS_RULE_TYPE, IKS_PAK_IQ,
         IKS_RULE_SUBTYPE, IKS_TYPE_RESULT,
         IKS_RULE_ID, "auth",
         IKS_RULE_DONE);
};

void xmpp_session_setup_handlers(xmpp_session *sess)
{
   sess->handlers[IKS_NODE_START] = start_node_handler;
   sess->handlers[IKS_NODE_NORMAL] = normal_node_handler;
   sess->handlers[IKS_NODE_ERROR] = error_node_handler;
   sess->handlers[IKS_NODE_STOP] = stop_node_handler;
};

xmpp_session* xmpp_session_create()
{
    xmpp_session* sess = malloc(sizeof(xmpp_session));
    sess->authorized = 0;
    sess->setup_done = 0;
    sess->stream = iks_stream_new("jabber:client", sess, xmpp_stream_hook); 
    iks_set_log_hook (sess->stream, (iksLogHook *) xmpp_log_hook);
    xmpp_session_setup_handlers(sess);
    return sess;
}

int xmpp_session_connect(xmpp_session* session, const char* server, int port)
{
    if (port == 0)
       port = IKS_JABBER_PORT;
    return iks_connect_tcp(session->stream, server, port);
};


int xmpp_session_destroy(xmpp_session* session)
{
    iks_parser_delete(session->stream);
    iks_filter_delete(session->filter);
    free(session);
};

int xmpp_disconnect(xmpp_session* session)
{
    iks_disconnect(session->stream);
    return TRUE;
}

int xmpp_do_start(xmpp_session* session, char* server, int port, char* username, char* password)
{
    int e;
    if (xmpp_session_connect(session, server, port) != IKS_OK)
    {
       printf("Failed to connect!\n");
    }
    session->account_name = username;
    session->account_password = password;
    setup_filter(session);
    while (!session->setup_done)
    {
        e = iks_recv (session->stream, 1);
        if (e != IKS_OK)
           break;
    }
    return TRUE;
   
};
