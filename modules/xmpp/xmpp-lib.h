#ifndef XMPP_LIB_H_INCLUDED
#define XMPP_LIB_H_INCLUDED

typedef struct _xmpp_session xmpp_session;
int xmpp_do_start(xmpp_session* session, char* server, int port, char* username, char* password);
xmpp_session* xmpp_session_create();
int xmpp_send_msg(xmpp_session *sess, char* to, char* msg);
int xmpp_disconnect(xmpp_session* session);
int xmpp_session_destroy(xmpp_session* session);

#endif
