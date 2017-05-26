/*
 * COPYRIGHT 2015 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Rajesh Nambiar <rajesh.nambiar@seagate.com>
 * Original author:  Kaustubh Deorukhkar <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Dec-2015
 */

#pragma once

#ifndef __S3_SERVER_S3_CLOVIS_WRAPPER_H__
#define __S3_SERVER_S3_CLOVIS_WRAPPER_H__

#include <functional>
#include <iostream>
#include "s3_clovis_rw_common.h"
#include "s3_post_to_main_loop.h"

#include "clovis_helpers.h"
#include "s3_fi_common.h"
#include "s3_log.h"
#include "s3_option.h"

enum class ClovisOpType {
  unknown,
  createobj,
  writeobj,
  deleteobj,
  createidx,
  deleteidx,
  getkv,
  putkv,
  deletekv,
};

class ClovisAPI {
 public:
  virtual int init_clovis_api() = 0;

  virtual void clovis_idx_init(struct m0_clovis_idx *idx,
                               struct m0_clovis_realm *parent,
                               const struct m0_uint128 *id) = 0;

  virtual int clovis_sync_op_init(struct m0_clovis_op **sync_op) = 0;

  virtual int clovis_sync_entity_add(struct m0_clovis_op *sync_op,
                                     struct m0_clovis_entity *entity) = 0;

  virtual int clovis_sync_op_add(struct m0_clovis_op *sync_op,
                                 struct m0_clovis_op *op) = 0;

  virtual void clovis_obj_init(struct m0_clovis_obj *obj,
                               struct m0_clovis_realm *parent,
                               const struct m0_uint128 *id) = 0;

  virtual int clovis_entity_create(struct m0_clovis_entity *entity,
                                   struct m0_clovis_op **op) = 0;

  virtual int clovis_entity_delete(struct m0_clovis_entity *entity,
                                   struct m0_clovis_op **op) = 0;

  virtual void clovis_op_setup(struct m0_clovis_op *op,
                               const struct m0_clovis_op_ops *ops,
                               m0_time_t linger) = 0;

  virtual int clovis_idx_op(struct m0_clovis_idx *idx,
                            enum m0_clovis_idx_opcode opcode,
                            struct m0_bufvec *keys, struct m0_bufvec *vals,
                            int *rcs, unsigned int flags,
                            struct m0_clovis_op **op) = 0;

  virtual void clovis_obj_op(struct m0_clovis_obj *obj,
                             enum m0_clovis_obj_opcode opcode,
                             struct m0_indexvec *ext, struct m0_bufvec *data,
                             struct m0_bufvec *attr, uint64_t mask,
                             struct m0_clovis_op **op) = 0;

  virtual void clovis_op_launch(struct m0_clovis_op **op, uint32_t nr,
                                ClovisOpType type = ClovisOpType::unknown) = 0;
};

class ConcreteClovisAPI : public ClovisAPI {
 private:
  // xxx This currently assumes only one fake operation is invoked.
  void clovis_fake_op_launch(struct m0_clovis_op **op, uint32_t nr) {
    s3_log(S3_LOG_DEBUG, "Called\n");
    struct user_event_context *user_ctx = (struct user_event_context *)calloc(
        1, sizeof(struct user_event_context));
    user_ctx->app_ctx = op[0];

    S3PostToMainLoop((void *)user_ctx)(s3_clovis_dummy_op_stable);
  }

  void clovis_fi_op_launch(struct m0_clovis_op **op, uint32_t nr) {
    s3_log(S3_LOG_DEBUG, "Called\n");
    for (uint32_t i = 0; i < nr; ++i) {
      struct user_event_context *user_ctx = (struct user_event_context *)calloc(
          1, sizeof(struct user_event_context));
      user_ctx->app_ctx = op[i];

      S3PostToMainLoop((void *)user_ctx)(s3_clovis_dummy_op_failed);
    }
  }

 public:
  int init_clovis_api() { return init_clovis(); }

  void clovis_idx_init(struct m0_clovis_idx *idx,
                       struct m0_clovis_realm *parent,
                       const struct m0_uint128 *id) {
    m0_clovis_idx_init(idx, parent, id);
  }

  void clovis_obj_init(struct m0_clovis_obj *obj,
                       struct m0_clovis_realm *parent,
                       const struct m0_uint128 *id) {
    S3Option *option_instance = S3Option::get_instance();
    m0_clovis_obj_init(obj, parent, id,
                       option_instance->get_clovis_layout_id());
  }

  int clovis_sync_op_init(struct m0_clovis_op **sync_op) {
    return m0_clovis_sync_op_init(sync_op);
  }

  int clovis_sync_entity_add(struct m0_clovis_op *sync_op,
                             struct m0_clovis_entity *entity) {
    return m0_clovis_sync_entity_add(sync_op, entity);
  }

  int clovis_sync_op_add(struct m0_clovis_op *sync_op,
                         struct m0_clovis_op *op) {
    return m0_clovis_sync_op_add(sync_op, op);
  }

  int clovis_entity_create(struct m0_clovis_entity *entity,
                           struct m0_clovis_op **op) {
    return m0_clovis_entity_create(entity, op);
  }

  int clovis_entity_delete(struct m0_clovis_entity *entity,
                           struct m0_clovis_op **op) {
    return m0_clovis_entity_delete(entity, op);
  }

  void clovis_op_setup(struct m0_clovis_op *op,
                       const struct m0_clovis_op_ops *ops, m0_time_t linger) {
    m0_clovis_op_setup(op, ops, linger);
  }

  int clovis_idx_op(struct m0_clovis_idx *idx, enum m0_clovis_idx_opcode opcode,
                    struct m0_bufvec *keys, struct m0_bufvec *vals, int *rcs,
                    unsigned int flags, struct m0_clovis_op **op) {
    return m0_clovis_idx_op(idx, opcode, keys, vals, rcs, flags, op);
  }

  void clovis_obj_op(struct m0_clovis_obj *obj,
                     enum m0_clovis_obj_opcode opcode, struct m0_indexvec *ext,
                     struct m0_bufvec *data, struct m0_bufvec *attr,
                     uint64_t mask, struct m0_clovis_op **op) {
    return m0_clovis_obj_op(obj, opcode, ext, data, attr, mask, op);
  }

  void clovis_op_launch(struct m0_clovis_op **op, uint32_t nr,
                        ClovisOpType type = ClovisOpType::unknown) {
    S3Option *config = S3Option::get_instance();
    if ((config->is_fake_clovis_createobj() &&
         type == ClovisOpType::createobj) ||
        (config->is_fake_clovis_writeobj() && type == ClovisOpType::writeobj) ||
        (config->is_fake_clovis_deleteobj() &&
         type == ClovisOpType::deleteobj) ||
        (config->is_fake_clovis_createidx() &&
         type == ClovisOpType::createidx) ||
        (config->is_fake_clovis_deleteidx() &&
         type == ClovisOpType::deleteidx) ||
        (config->is_fake_clovis_getkv() && type == ClovisOpType::getkv) ||
        (config->is_fake_clovis_putkv() && type == ClovisOpType::putkv) ||
        (config->is_fake_clovis_deletekv() && type == ClovisOpType::deletekv)) {
      clovis_fake_op_launch(op, nr);
    } else if ((type == ClovisOpType::createobj &&
                s3_fi_is_enabled("clovis_obj_create_fail")) ||
               (type == ClovisOpType::writeobj &&
                s3_fi_is_enabled("clovis_obj_write_fail")) ||
               (type == ClovisOpType::deleteobj &&
                s3_fi_is_enabled("clovis_obj_delete_fail")) ||
               (type == ClovisOpType::createidx &&
                s3_fi_is_enabled("clovis_idx_create_fail")) ||
               (type == ClovisOpType::deleteidx &&
                s3_fi_is_enabled("clovis_idx_delete_fail")) ||
               (type == ClovisOpType::deletekv &&
                s3_fi_is_enabled("clovis_kv_delete_fail")) ||
               (type == ClovisOpType::putkv &&
                s3_fi_is_enabled("clovis_kv_put_fail")) ||
               (type == ClovisOpType::getkv &&
                s3_fi_is_enabled("clovis_kv_get_fail"))) {
      clovis_fi_op_launch(op, nr);
    } else {
      m0_clovis_op_launch(op, nr);
    }
  }
};
#endif
