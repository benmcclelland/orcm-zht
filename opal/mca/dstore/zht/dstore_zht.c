/*
 * dstore_zht.c
 *
 *  Created on: Jan 30, 2015
 *      Author: kwang
 */
#include <time.h>
#include <string.h>

#include "dstore_zht.h"
#include "opal/constants.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/dss/dss_types.h"
#include "opal/util/error.h"
#include "opal/util/output.h"
#include "opal/util/proc.h"
#include "opal/util/show_help.h"
#include "opal/mca/dstore/base/base.h"

static int init(struct opal_dstore_base_module_t *imod);
static void finalize(struct opal_dstore_base_module_t *imod);
static int store(struct opal_dstore_base_module_t *imod,
                 const opal_process_name_t *proc,
                 opal_value_t *val);
static int fetch(struct opal_dstore_base_module_t *imod,
                 const opal_process_name_t *proc,
                 const char *key,
                 opal_list_t *kvs);
static int remove_data(struct opal_dstore_base_module_t *imod,
                       const opal_process_name_t *proc, const char *key);

/* pack the important information of the kvs value to string */
static char* pack_kvs(opal_buffer_t *buffer, opal_value_t *val);

opal_dstore_base_module_t opal_dstore_zht_module = {
    {
        init,
        finalize,
        store,
        fetch,
        remove_data
    }
};

/* Initialize as a ZHT client, need two
 * parameters: zht_config and member_list
 * */
static int init(struct opal_dstore_base_module_t *imod)
{
    /* need to change the interface a little
     * bit to include the two parameters */
	//return c_zht_init(zht_config, member_list);
    return OPAL_SUCCESS;
}

/* disconnect from zht server */
static void finalize(struct opal_dstore_base_module_t *imod)
{
	c_zht_teardown();
}

/* insert a (key, value) record to ZHT server */
static int store(struct opal_dstore_base_module_t *imod,
                 const opal_process_name_t *id,
                 opal_value_t *val)
{
    int rc;

    opal_output_verbose(1, opal_dstore_base_framework.framework_output,
                        "%s dstore:zht:kvs storing data for proc %s",
                        OPAL_NAME_PRINT(OPAL_PROC_MY_NAME), OPAL_NAME_PRINT(*id));
    /* to be continued */
    return OPAL_SUCCESS;
}


/* pack the kvs record to string, in order to put it to ZHT */
static char* pack_kvs(opal_buffer_t *buffer, opal_value_t *val)
{
	if (NULL == buffer || NULL == val) {
		opal_output_verbose(1, -1, "either the "
		                    "buffer or the value to be packed is NULL");
		return NULL;
	}

	opal_dss.pack(buffer, &(val->key), 1, OPAL_STRING);
	opal_dss.pack(buffer, &(val->data.flag), 1, OPAL_BOOL);
	opal_dss.pack(buffer, &(val->data.byte), 1, OPAL_BYTE);
	opal_dss.pack(buffer, &(val->data.string), 1, OPAL_STRING);
	opal_dss.pack(buffer, &(val->data.size), 1, OPAL_SIZE);
	opal_dss.pack(buffer, &(val->data.pid), 1, OPAL_PID);
	opal_dss.pack(buffer, &(val->data.integer), 1, OPAL_INT);
	opal_dss.pack(buffer, &(val->data.int8), 1, OPAL_INT8);
	opal_dss.pack(buffer, &(val->data.int16), 1, OPAL_INT16);
	opal_dss.pack(buffer, &(val->data.int32), 1, OPAL_INT32);
	opal_dss.pack(buffer, &(val->data.int64), 1, OPAL_INT64);
	opal_dss.pack(buffer, &(val->data.uint), 1, OPAL_UINT);
	opal_dss.pack(buffer, &(val->data.uint8), 1, OPAL_UINT8);
	opal_dss.pack(buffer, &(val->data.uint16), 1, OPAL_UINT16);
	opal_dss.pack(buffer, &(val->data.uint32), 1, OPAL_UINT32);
	opal_dss.pack(buffer, &(val->data.uint64), 1, OPAL_UINT64);
	opal_dss.pack(buffer, &(val->data.bo), 1, OPAL_BYTE_OBJECT);
	opal_dss.pack(buffer, &(val->data.fval), 1, OPAL_FLOAT);
	opal_dss.pack(buffer, &(val->data.dval), 1, OPAL_DOUBLE);
	opal_dss.pack(buffer, &(val->data.tv), 1, OPAL_TIMEVAL);
	opal_dss.pack(buffer, &(val->data.name), 1, OPAL_NAME);
	opal_dss.pack(buffer, &(val->data.flag_array), 1, OPAL_BOOL_ARRAY);
	opal_dss.pack(buffer, &(val->data.byte_array), 1, OPAL_UINT8_ARRAY);
	opal_dss.pack(buffer, &(val->data.string_array), 1, OPAL_STRING_ARRAY);
	opal_dss.pack(buffer, &(val->data.size_array), 1, OPAL_SIZE_ARRAY);
	opal_dss.pack(buffer, &(val->data.integer_array), 1, OPAL_INT_ARRAY);
	opal_dss.pack(buffer, &(val->data.int8_array), 1, OPAL_INT8_ARRAY);
	opal_dss.pack(buffer, &(val->data.int16_array), 1, OPAL_INT16_ARRAY);
	opal_dss.pack(buffer, &(val->data.int32_array), 1, OPAL_INT32_ARRAY);
	opal_dss.pack(buffer, &(val->data.int64_array), 1, OPAL_INT64_ARRAY);
	opal_dss.pack(buffer, &(val->data.uint_array), 1, OPAL_UINT_ARRAY);
	opal_dss.pack(buffer, &(val->data.uint8_array), 1, OPAL_UINT8_ARRAY);
	opal_dss.pack(buffer, &(val->data.uint16_array), 1, OPAL_UINT16_ARRAY);
	opal_dss.pack(buffer, &(val->data.uint32_array), 1, OPAL_UINT32_ARRAY);
	opal_dss.pack(buffer, &(val->data.uint64_array), 1, OPAL_UINT64_ARRAY);
	opal_dss.pack(buffer, &(val->data.bo_array), 1, OPAL_BYTE_OBJECT_ARRAY);
	opal_dss.pack(buffer, &(val->data.fval_array), 1, OPAL_FLOAT_ARRAY);
	/* to be continued */

}
