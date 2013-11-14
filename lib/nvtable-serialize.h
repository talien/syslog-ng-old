#ifndef _NVTABLE_SERIALIZE_H
#define _NVTABLE_SERIALIZE_H

#include "nvtable.h"
#include "serialize.h"

NVTable *nv_table_unserialize(SerializeArchive *sa, guint8 log_msg_version);
gboolean nv_table_serialize(SerializeArchive *sa, NVTable *self);
void nv_table_update_ids(NVTable *self,NVRegistry *logmsg_registry, NVHandle *handles_to_update, guint8 num_handles_to_update);

static inline gboolean serialize_read_nvhandle(SerializeArchive *sa, NVHandle* handle)
{
   return serialize_read_uint32(sa, handle);
}

static inline gboolean serialize_write_nvhandle(SerializeArchive *sa, NVHandle handle)
{
   return serialize_write_uint32(sa, handle);
}

#endif
