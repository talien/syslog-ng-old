#ifndef XMPP_H_INCLUDED
#define XMPP_H_INCLUDED

#include "driver.h"
#include "value-pairs.h"

LogDriver *xmpp_dd_new(void);
void xmpp_dd_set_server(LogDriver* s, char* server);
void xmpp_dd_set_port(LogDriver* s, int port);
void xmpp_dd_set_user(LogDriver* s, char* user);
void xmpp_dd_set_password(LogDriver* s, char* password);
void xmpp_dd_set_to(LogDriver* s, char* to);
void xmpp_dd_set_msg(LogDriver* s, char* msg);
#endif
