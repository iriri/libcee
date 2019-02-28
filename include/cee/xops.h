#ifndef CEE_XOPS_H
#define CEE_XOPS_H
#include <stdatomic.h>

#define xget_rlx(p) atomic_load_explicit(p, memory_order_relaxed)
#define xget_con(p) atomic_load_explicit(p, memory_order_consume)
#define xget_acq(p) atomic_load_explicit(p, memory_order_acquire)
#define xget_seq(p) atomic_load_explicit(p, memory_order_seq_cst)

#define xset_rlx(p, v) atomic_store_explicit(p, v, memory_order_relaxed)
#define xset_rel(p, v) atomic_store_explicit(p, v, memory_order_release)
#define xset_seq(p, v) atomic_store_explicit(p, v, memory_order_seq_cst)

#define xadd_rlx(p, v) atomic_fetch_add_explicit(p, v, memory_order_relaxed)
#define xadd_con(p, v) atomic_fetch_add_explicit(p, v, memory_order_consume)
#define xadd_acq(p, v) atomic_fetch_add_explicit(p, v, memory_order_acquire)
#define xadd_rel(p, v) atomic_fetch_add_explicit(p, v, memory_order_release)
#define xadd_acr(p, v) atomic_fetch_add_explicit(p, v, memory_order_acq_rel)
#define xadd_seq(p, v) atomic_fetch_add_explicit(p, v, memory_order_seq_cst)

#define xsub_rlx(p, v) atomic_fetch_sub_explicit(p, v, memory_order_relaxed)
#define xsub_con(p, v) atomic_fetch_sub_explicit(p, v, memory_order_consume)
#define xsub_acq(p, v) atomic_fetch_sub_explicit(p, v, memory_order_acquire)
#define xsub_rel(p, v) atomic_fetch_sub_explicit(p, v, memory_order_release)
#define xsub_acr(p, v) atomic_fetch_sub_explicit(p, v, memory_order_acq_rel)
#define xsub_seq(p, v) atomic_fetch_sub_explicit(p, v, memory_order_seq_cst)

#define xchg_rlx(p, v) atomic_exchange_explicit(p, v, memory_order_relaxed)
#define xchg_con(p, v) atomic_exchange_explicit(p, v, memory_order_consume)
#define xchg_acq(p, v) atomic_exchange_explicit(p, v, memory_order_acquire)
#define xchg_rel(p, v) atomic_exchange_explicit(p, v, memory_order_release)
#define xchg_acr(p, v) atomic_exchange_explicit(p, v, memory_order_acq_rel)
#define xchg_seq(p, v) atomic_exchange_explicit(p, v, memory_order_seq_cst)

#define xcas_w_rlx_rlx(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_relaxed, memory_order_relaxed)

#define xcas_w_con_rlx(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_consume, memory_order_relaxed)
#define xcas_w_con_con(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_consume, memory_order_consume)

#define xcas_w_acq_rlx(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_acquire, memory_order_relaxed)
#define xcas_w_acq_con(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_acquire, memory_order_consume)
#define xcas_w_acq_acq(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_acquire, memory_order_acquire)

#define xcas_w_rel_rlx(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_release, memory_order_relaxed)
#define xcas_w_rel_con(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_release, memory_order_consume)
#define xcas_w_rel_acq(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_release, memory_order_acquire)

#define xcas_w_acr_rlx(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_acq_rel, memory_order_relaxed)
#define xcas_w_acr_con(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_acq_rel, memory_order_consume)
#define xcas_w_acr_acq(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_acq_rel, memory_order_acquire)

#define xcas_w_seq_rlx(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_relaxed)
#define xcas_w_seq_con(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_consume)
#define xcas_w_seq_acq(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_acquire)
#define xcas_w_seq_seq(p, e, v) \
    atomic_compare_exchange_weak_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_seq_cst)

#define xcas_s_rlx_rlx(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_relaxed, memory_order_relaxed)

#define xcas_s_con_rlx(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_consume, memory_order_relaxed)
#define xcas_s_con_con(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_consume, memory_order_consume)

#define xcas_s_acq_rlx(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_acquire, memory_order_relaxed)
#define xcas_s_acq_con(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_acquire, memory_order_consume)
#define xcas_s_acq_acq(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_acquire, memory_order_acquire)

#define xcas_s_rel_rlx(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_release, memory_order_relaxed)
#define xcas_s_rel_con(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_release, memory_order_consume)
#define xcas_s_rel_acq(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_release, memory_order_acquire)

#define xcas_s_acr_rlx(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_acq_rel, memory_order_relaxed)
#define xcas_s_acr_con(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_acq_rel, memory_order_consume)
#define xcas_s_acr_acq(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_acq_rel, memory_order_acquire)

#define xcas_s_seq_rlx(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_relaxed)
#define xcas_s_seq_con(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_consume)
#define xcas_s_seq_acq(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_acquire)
#define xcas_s_seq_seq(p, e, v) \
    atomic_compare_exchange_strong_explicit( \
        p, e, v, memory_order_seq_cst, memory_order_seq_cst)
#endif
