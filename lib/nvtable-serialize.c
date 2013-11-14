#include "nvtable-serialize.h"
#include "messages.h"
#include "stdlib.h"
#include "string.h"

#define NV_TABLE_MAGIC_V2  "NVT2"
#define NV_TABLE_OLD_SCALE 2
#define NVT_SF_BE           0x1
static const int SIZE_DIFF_OF_OLD_NVENTRY_AND_NEW_NVENTRY = 12;
typedef struct _OldNVEntry
{
  /* negative offset, counting from string table top, e.g. start of the string is at @top + ofs */
  union {
    struct {
      guint8 indirect:1, referenced:1;
    };  
    guint8 flags;
  };  

  guint8 name_len;
  guint16 alloc_len;
  union
  {
    struct
    {   
      guint16 value_len;
      /* variable data, first the name of this entry, then the value, both are NUL terminated */
      gchar data[0];
    } vdirect;
    struct
    {   
      guint16 handle;
      guint16 ofs;
      guint16 len;
      guint8 type;
      gchar name[0];
    } vindirect;
  };  
} OldNVEntry;

static gint
dyn_entry_cmp(const void *a, const void *b)
{
  NVDynValue entry_a = *(NVDynValue *)a;
  NVDynValue entry_b = *(NVDynValue *)b;
  NVHandle handle_a = NV_TABLE_DYNVALUE_HANDLE(entry_a);
  NVHandle handle_b = NV_TABLE_DYNVALUE_HANDLE(entry_b);
  return (handle_a - handle_b);
}

/**
 * serialize / unserialize functions
 **/
void
nv_table_update_ids(NVTable *self,NVRegistry *logmsg_registry, NVHandle *handles_to_update, guint8 num_handles_to_update)
{
  NVDynValue *dyn_entries = nv_table_get_dyn_entries(self);
  guint16 i,j;
  NVHandle *new_updated_handles = g_new0(NVHandle, num_handles_to_update);

  /*
     upgrade indirect entries:
     - get parent the handle from the entry
     - get the parent entry
     - if parent entry is static (predefined) skip the following steps
     - get the name of the parent entry handle
     - allocate the name -> get a handle
     - set the given handle for the entry
  */
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVEntry *entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));
      if (!entry)
        continue;
      if (entry->indirect)
        {
          NVHandle oh = entry->vindirect.handle;
          const gchar *name = nv_entry_get_name(entry);
          if (oh > self->num_static_entries)
            {
              /* Only do this refresh on non-static entries */
              NVDynValue *dyn_slot;
              NVEntry *oe = nv_table_get_entry(self,oh,&dyn_slot);
              const gchar *oname = nv_entry_get_name(oe);
              if (G_UNLIKELY(oname[0] == '\0'))
                 msg_debug ("nvtable: entry->indirect => name is zero length",
                            evt_tag_str("entry.name", name),
                            evt_tag_int("i", i),
                            NULL);
              else
                {
                  oh = nv_registry_alloc_handle(logmsg_registry,oname);
                  if (oh != 0)
                    entry->vindirect.handle = oh;
                }
            }
          nv_registry_alloc_handle(logmsg_registry, name);
        }
    }

  /*
     upgrade handles:
  */
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      NVHandle handle;
      NVHandle old_handle;
      NVEntry *entry;
      const gchar *name;
      guint16 ofs;

      entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));
      old_handle = NV_TABLE_DYNVALUE_HANDLE(dyn_entries[i]);
      name = nv_entry_get_name(entry);
      if (!entry)
        continue;
      handle = nv_registry_alloc_handle(logmsg_registry, name);
      ofs = nv_table_get_dyn_value_offset_from_nventry(self, entry);
      dyn_entries[i].handle = handle;
      dyn_entries[i].ofs = ofs;
      if (handles_to_update)
        {
          for (j = 0; j < num_handles_to_update; j++)
            {
              if (handles_to_update[j] == old_handle)
                {
                  new_updated_handles[j] = handle;
                }
            }
        }
    }
  for (i = 0; i < num_handles_to_update; i++)
    {
      handles_to_update[i] =  new_updated_handles[i];
    }
  qsort(dyn_entries, self->num_dyn_entries, sizeof(NVDynValue), dyn_entry_cmp);
  g_free(new_updated_handles);
  return;
}

static inline guint8 reverse(guint8 b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

static inline void
nv_table_swap_old_entry_flags(OldNVEntry *entry)
{
  entry->flags = reverse(entry->flags);
}

static void
nv_old_entry_swap_bytes(OldNVEntry *entry)
{
  nv_table_swap_old_entry_flags(entry);
  entry->alloc_len = GUINT16_SWAP_LE_BE(entry->alloc_len);
  if (!entry->indirect)
    {
      entry->vdirect.value_len = GUINT16_SWAP_LE_BE(entry->vdirect.value_len);
    }
  else
    {
      entry->vindirect.handle = GUINT16_SWAP_LE_BE(entry->vindirect.handle);
      entry->vindirect.ofs = GUINT16_SWAP_LE_BE(entry->vindirect.ofs);
      entry->vindirect.len = GUINT16_SWAP_LE_BE(entry->vindirect.len);
    }
}

static inline void
nv_table_swap_entry_flags(NVEntry *entry)
{
  entry->flags = reverse(entry->flags);
}

static void
nv_entry_swap_bytes(NVEntry *entry)
{
  nv_table_swap_entry_flags(entry);
  entry->alloc_len = GUINT32_SWAP_LE_BE(entry->alloc_len);
  if (!entry->indirect)
    {
      entry->vdirect.value_len = GUINT32_SWAP_LE_BE(entry->vdirect.value_len);
    }
  else
    {
      entry->vindirect.handle = GUINT32_SWAP_LE_BE(entry->vindirect.handle);
      entry->vindirect.ofs = GUINT32_SWAP_LE_BE(entry->vindirect.ofs);
      entry->vindirect.len = GUINT32_SWAP_LE_BE(entry->vindirect.len);
    }
}



static void
nv_table_dyn_value_swap_bytes(NVDynValue* self)
{
  self->handle = GUINT32_SWAP_LE_BE(self->handle);
  self->ofs = GUINT32_SWAP_LE_BE(self->handle);
};

void
nv_table_data_swap_bytes(NVTable *self)
{
  NVDynValue *dyn_entries;
  NVEntry *entry;
  gint i;
  for (i = 0; i < self->num_static_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, self->static_entries[i]);
      if (!entry)
        continue;
      nv_entry_swap_bytes(entry);
    }

  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      entry = nv_table_get_entry_at_ofs(self, NV_TABLE_DYNVALUE_OFS(dyn_entries[i]));

      if (!entry)
        continue;
      nv_entry_swap_bytes(entry);
    }
}

void
nv_table_struct_swap_bytes(NVTable *self)
{
  guint16 i;
  NVDynValue *dyn_entries;
  self->size = GUINT16_SWAP_LE_BE(self->size);
  self->used = GUINT16_SWAP_LE_BE(self->used);
  self->num_dyn_entries = GUINT16_SWAP_LE_BE(self->num_dyn_entries);
  for (i = 0; i < self->num_static_entries; i++)
    {
      self->static_entries[i] = GUINT16_SWAP_LE_BE(self->static_entries[i]);
    }
  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++)
    {
      nv_table_dyn_value_swap_bytes(&dyn_entries[i]);
    }
}

static gboolean
nv_table_unserialize_dyn_value(SerializeArchive *sa, NVDynValue* dyn_value)
{
   return serialize_read_uint32(sa, &(dyn_value->handle)) &&
          serialize_read_uint32(sa, &(dyn_value->ofs));
};

gboolean
nv_table_unserialize_struct(SerializeArchive *sa, NVTable *res)
{
  guint32 i;
  NVDynValue *dyn_entries;
  for (i = 0; i < res->num_static_entries; i++)
    {
      if (!serialize_read_uint32(sa, &res->static_entries[i]))
        return FALSE;
    }
  dyn_entries = nv_table_get_dyn_entries(res);
  for (i = 0; i < res->num_dyn_entries; i++)
    {
      if (!nv_table_unserialize_dyn_value(sa, &dyn_entries[i]))
        return FALSE;
    }
  return TRUE;
}

gboolean 
nv_table_unserialize_struct_22(SerializeArchive *sa, NVTable *res)
{
  guint16 i;
  NVDynValue *dyn_entries;
  guint32 old_dyn_entry;
  guint16 old_static_entry;
  for (i = 0; i < res->num_static_entries; i++)
    {
      if (!serialize_read_uint16(sa, &old_static_entry))
               return FALSE;
      res->static_entries[i] = old_static_entry << NV_TABLE_OLD_SCALE;

    }
  dyn_entries = nv_table_get_dyn_entries(res);
  for (i = 0; i < res->num_dyn_entries; i++)
    {
      if (!serialize_read_uint32(sa, &old_dyn_entry))
        return FALSE;
      dyn_entries[i].handle = old_dyn_entry >> 16;
      dyn_entries[i].ofs = (old_dyn_entry & 0xFFFF) << NV_TABLE_OLD_SCALE;
      
    }
  return TRUE;
}

static inline int
_calculate_new_alloc_len(OldNVEntry* old_entry)
{
  return (old_entry->alloc_len << NV_TABLE_OLD_SCALE) + SIZE_DIFF_OF_OLD_NVENTRY_AND_NEW_NVENTRY;
}

static inline NVEntry* 
nv_table_unserialize_entry(GString* old_nvtable_payload, guint32 old_offset, void* payload_start, gboolean different_endianness)
{
   OldNVEntry* old_entry = (OldNVEntry*) (old_nvtable_payload->str + old_nvtable_payload->len - old_offset);
   if (different_endianness)
   {
      nv_old_entry_swap_bytes(old_entry);
   }
   NVEntry* new_entry = (NVEntry*) (payload_start - _calculate_new_alloc_len(old_entry));
   new_entry->flags = old_entry->flags;
   new_entry->name_len = old_entry->name_len;
   new_entry->alloc_len = _calculate_new_alloc_len(old_entry);
   if (!new_entry->indirect)
     {
       new_entry->vdirect.value_len = old_entry->vdirect.value_len;
       memcpy(&new_entry->vdirect.data, &old_entry->vdirect.data, new_entry->name_len + new_entry->vdirect.value_len + 2);
     }
   else
     {
       new_entry->vindirect.handle = old_entry->vindirect.handle;
       new_entry->vindirect.type = old_entry->vindirect.type;
       new_entry->vindirect.ofs = old_entry->vindirect.ofs;
       new_entry->vindirect.len = old_entry->vindirect.len;
       memcpy(&new_entry->vindirect.name, &old_entry->vindirect.name, new_entry->name_len);
     }
   return new_entry;
}

gboolean
nv_table_unserialize_blob_v22(SerializeArchive *sa, NVTable* self, void* table_top, gboolean different_endianness)
{
   if (!self->used)
      return TRUE;
   GString* old_nvtable_payload = g_string_sized_new(self->used);
   old_nvtable_payload->len = self->used;
   void* current_payload_pointer = table_top;

   if (!serialize_read_blob(sa, old_nvtable_payload->str, self->used))
   {
     g_string_free(old_nvtable_payload, TRUE);
     return FALSE;
   }

   int i;
   for (i =0; i< self->num_static_entries; i++)
   {
     guint32 old_entry_offset = self->static_entries[i];
     if (old_entry_offset != 0)
     {
       NVEntry* new_entry = nv_table_unserialize_entry(old_nvtable_payload, old_entry_offset, current_payload_pointer, different_endianness);
       self->static_entries[i] = (guint32)(table_top - (void*)new_entry);
       current_payload_pointer = current_payload_pointer - new_entry->alloc_len;
     }
   }

   NVDynValue* dyn_entries = nv_table_get_dyn_entries(self);
   for (i =0; i< self->num_dyn_entries; i++)
   {
     NVDynValue *dynvalue = &dyn_entries[i];
     guint32 old_entry_offset = dynvalue->ofs;
     NVEntry* new_entry = nv_table_unserialize_entry(old_nvtable_payload, old_entry_offset, current_payload_pointer, different_endianness);
     dynvalue->ofs = (guint32) (table_top - (void*) new_entry);
     current_payload_pointer = current_payload_pointer - new_entry->alloc_len;
   }
 
   self->used = table_top - current_payload_pointer;
   // Iterate through all NVEntries and convert them. We should update their offset, too.
   // We do not need to iterate them in order, we will simply copy them to the new place linearly.
   // The only problem is that the indirect/direct use-case should be resolved correctly.
   g_string_free(old_nvtable_payload, TRUE);
   return TRUE;
      
}

static inline guint32 _calculate_new_size(NVTable* self)
{
  guint32 diff_of_old_used_and_new_used = (self->num_static_entries + self->num_dyn_entries) * SIZE_DIFF_OF_OLD_NVENTRY_AND_NEW_NVENTRY;

  return self->size + 4 + 2 * self->num_static_entries + 4 * self->num_dyn_entries +  diff_of_old_used_and_new_used;
}

NVTable *
nv_table_unserialize_22(SerializeArchive *sa)
{
  guint16 old_res;
  guint32 magic = 0;
  guint8 flags = 0;
  NVTable *res = NULL;
  gboolean is_big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  if (!serialize_read_uint32(sa, &magic))
    return NULL;
  if (!serialize_read_uint8(sa, &flags))
    return NULL;
  if (!!(flags & NVT_SF_BE) != is_big_endian)
    {
      magic = GUINT32_SWAP_LE_BE(magic);
    }
  if (memcmp((void *)&magic, (const void *)NV_TABLE_MAGIC_V2, 4)!=0)
    return NULL;
  res = (NVTable *)g_malloc(sizeof(NVTable));
  if (!serialize_read_uint16(sa, &old_res))
    {
      g_free(res);
      return NULL;
    }
  res->size = old_res << NV_TABLE_OLD_SCALE;
  if (!serialize_read_uint16(sa, &old_res))
    {
      g_free(res);
      return NULL;
    }
  res->used = old_res << NV_TABLE_OLD_SCALE;
  if (!serialize_read_uint16(sa, &res->num_dyn_entries))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint8(sa, &res->num_static_entries))
    {
      g_free(res);
      return NULL;
    }

  res->size = _calculate_new_size(res);
   
  res->ref_cnt = 1;
  res = (NVTable *)g_realloc(res, res->size);
  res->borrowed = FALSE;
  gboolean different_endianness = (is_big_endian != (flags & NVT_SF_BE));
  if(!res)
    {
      return NULL;
    }
  if (!nv_table_unserialize_struct_22(sa, res))
    {
      g_free(res);
      return NULL;
    }

  if (!nv_table_unserialize_blob_v22(sa, res, nv_table_get_top(res), different_endianness))
    {
      g_free(res);
      return NULL;
    }

  /*if (is_big_endian != (flags & NVT_SF_BE))
    {
      nv_table_data_swap_bytes(res);
    }*/
  return res;
}

NVTable *
nv_table_unserialize_24(SerializeArchive *sa)
{
  guint32 magic = 0;
  guint8 flags = 0;
  NVTable *res = NULL;
  gboolean is_big_endian = (G_BYTE_ORDER == G_BIG_ENDIAN);
  if (!serialize_read_uint32(sa, &magic))
    return NULL;
  if (!serialize_read_uint8(sa, &flags))
    return NULL;
  if (!!(flags & NVT_SF_BE) != is_big_endian)
    {
      magic = GUINT32_SWAP_LE_BE(magic);
    }
  if (memcmp((void *)&magic, (const void *)NV_TABLE_MAGIC_V2, 4)!=0)
    return NULL;
  res = (NVTable *)g_malloc(sizeof(NVTable));
  if (!serialize_read_uint32(sa, &res->size))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint32(sa, &res->used))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint16(sa, &res->num_dyn_entries))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_uint8(sa, &res->num_static_entries))
    {
      g_free(res);
      return NULL;
    }

  res->ref_cnt = 1;
  res = (NVTable *)g_realloc(res,res->size);
  res->borrowed = FALSE;
  if(!res)
    {
      return NULL;
    }
  if (!nv_table_unserialize_struct(sa, res))
    {
      g_free(res);
      return NULL;
    }
  if (!serialize_read_blob(sa, NV_TABLE_ADDR(res, res->size - res->used), res->used))
    {
      g_free(res);
      return NULL;
    }
  if (is_big_endian != (flags & NVT_SF_BE))
    {
      nv_table_data_swap_bytes(res);
    }
  return res;
}



NVTable *
nv_table_unserialize(SerializeArchive *sa, guint8 log_msg_version)
{
  guint32 header_len=0;
  guint32 calculated_header_len=0;
  guint32 calculated_used_len=0;
  guint32 used_len=0;
  NVTable *res;
  gboolean swap_bytes=FALSE;
  if (log_msg_version < 22)
    {
      if (!serialize_read_uint32(sa, &header_len))
        return NULL;
      res = (NVTable *)g_try_malloc(header_len);
      if (!res)
        return NULL;
      if (!serialize_read_blob(sa, res, header_len))
        {
          g_free(res);
          return NULL;
        }
      calculated_header_len = sizeof(NVTable) + res->num_static_entries * sizeof(res->static_entries[0]) + res->num_dyn_entries * sizeof(guint32);
      if (!serialize_read_uint32(sa, &used_len))
        {
          g_free(res);
          return NULL;
        }
      calculated_used_len = res->used << NV_TABLE_OLD_SCALE;
      if (calculated_used_len != used_len || calculated_header_len != header_len)
        swap_bytes=TRUE;

      if (swap_bytes)
        nv_table_struct_swap_bytes(res);

      res = (NVTable *)g_try_realloc(res,res->size << NV_TABLE_OLD_SCALE);
      if (!res)
        return NULL;
      res->borrowed = FALSE;
      res->ref_cnt = 1;
      if (!serialize_read_blob(sa, NV_TABLE_ADDR(res, res->size - res->used), used_len))
        {    
          g_free(res);
          return NULL;
        }    

      if (swap_bytes)
        nv_table_data_swap_bytes(res);
    }
  else if (log_msg_version < 24)
    {
      res = nv_table_unserialize_22(sa);
    }
  else 
    {
      res = nv_table_unserialize_24(sa);
    }
  return res;
}

static void
nv_table_serialize_nv_dyn_value(SerializeArchive *sa, NVDynValue *dyn_value)
{
  serialize_write_uint32(sa, dyn_value->handle);
  serialize_write_uint32(sa, dyn_value->ofs);
};

void
nv_table_write_struct(SerializeArchive *sa, NVTable *self)
{
  guint16 i;
  NVDynValue *dyn_entries;
  serialize_write_uint32(sa, self->size);
  serialize_write_uint32(sa, self->used);
  serialize_write_uint16(sa, self->num_dyn_entries);
  serialize_write_uint8(sa, self->num_static_entries);
  for (i = 0; i < self->num_static_entries; i++) 
    {    
      serialize_write_uint32(sa, self->static_entries[i]);
    }    
  dyn_entries = nv_table_get_dyn_entries(self);
  for (i = 0; i < self->num_dyn_entries; i++) 
    {    
       nv_table_serialize_nv_dyn_value(sa, &dyn_entries[i]);
    }    
}

gboolean
nv_table_serialize(SerializeArchive *sa,NVTable *self)
{
  guint8 serialized_flag = 0; 
  guint32 magic = 0; 
  guint32 used_len;
  memcpy((void *)&magic,(const void *) NV_TABLE_MAGIC_V2, 4);
  serialize_write_uint32(sa, magic);
  if (G_BYTE_ORDER == G_BIG_ENDIAN)
    serialized_flag |= NVT_SF_BE;
  serialize_write_uint8(sa, serialized_flag);
  nv_table_write_struct(sa, self);
  used_len = self->used;
  serialize_write_blob(sa, NV_TABLE_ADDR(self, self->size - self->used), used_len);
  return TRUE;
}
