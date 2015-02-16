/*
 * dstore_zht_component.c
 *
 *  Created on: Jan 30, 2015
 *      Author: kwang
 */

#include "opal_config.h"
#include "opal/constants.h"

#include "opal/mca/base/base.h"
#include "opal/util/error.h"

#include "opal/mca/dstore/dstore.h"
#include "opal/mca/dstore/base/base.h"
#include "dstore_zht.h"

static opal_dstore_base_module_t *component_create(opal_list_t *attrs);
static int dstore_zht_query(mca_base_module_t **module, int *priority);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
opal_dstore_base_component_t mca_dstore_zht_component = {
    {
        OPAL_DSTORE_BASE_VERSION_2_0_0,

        /* Component name and version */
        "zht",
        OPAL_MAJOR_VERSION,
        OPAL_MINOR_VERSION,
        OPAL_RELEASE_VERSION,

        /* Component open and close functions */
        NULL,
        NULL,
        dstore_zht_query,
        NULL
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    component_create,
    NULL,
    NULL
};

static int dstore_zht_query(mca_base_module_t **module, int *priority)
{
    /* we are always available, but only as storage */
    *priority = 80;
    *module = NULL;
    return OPAL_SUCCESS;
}

/* this component ignores any input attributes */
static opal_dstore_base_module_t *component_create(opal_list_t *attrs)
{
	opal_dstore_base_module_t *mod;

    mod = (opal_dstore_base_module_t*)malloc(sizeof(opal_dstore_base_module_t));
    if (NULL == mod) {
        OPAL_ERROR_LOG(OPAL_ERR_OUT_OF_RESOURCE);
        return NULL;
    }
    /* copy the APIs across */
    memcpy(mod, &opal_dstore_zht_module, sizeof(opal_dstore_base_module_t));
    /* let the module init itself
     * ZHT needs two parameters for initialization*/
    if (OPAL_SUCCESS != mod->init((struct opal_dstore_base_module_t*)mod)) {
        /* release the module and return the error */
        free(mod);
        return NULL;
    }
    return (opal_dstore_base_module_t*)mod;
}
