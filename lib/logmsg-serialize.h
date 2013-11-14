#ifndef _LOGMSG_SERIALIZE_H
#define _LOGMSG_SERIALIZE_H

#include "logmsg.h"

gboolean log_msg_write(LogMessage *self, SerializeArchive *sa);
gboolean log_msg_read(LogMessage *self, SerializeArchive *sa);

#endif
