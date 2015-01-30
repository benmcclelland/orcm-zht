/*
 * dstore_zht.h
 *
 *  Created on: Jan 30, 2015
 *      Author: kwang
 */

#ifndef ORCM_ZHT_OPAL_MCA_DSTORE_ZHT_DSTORE_ZHT_H_
#define ORCM_ZHT_OPAL_MCA_DSTORE_ZHT_DSTORE_ZHT_H_

#include "opal/class/opal_hash_table.h"
#include "opal/mca/dstore/dstore.h"
#include "ZHT/src/c_zhtclient.h"

#define MSG_SIZE 1024

BEGIN_C_DECLS

OPAL_MODULE_DECLSPEC extern opal_dstore_base_component_t mca_dstore_zht_component;

OPAL_MODULE_DECLSPEC extern opal_dstore_base_module_t opal_dstore_zht_module;

END_C_DECLS

#endif /* ORCM_ZHT_OPAL_MCA_DSTORE_ZHT_DSTORE_ZHT_H_ */
