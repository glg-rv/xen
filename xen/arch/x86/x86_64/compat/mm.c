#include <xen/event.h>
#include <xen/hypercall.h>
#include <xen/mem_access.h>
#include <xen/multicall.h>
#include <compat/memory.h>
#include <compat/xen.h>
#include <asm/mem_paging.h>
#include <asm/mem_sharing.h>

#include <asm/pv/mm.h>

int compat_arch_memory_op(unsigned long cmd, XEN_GUEST_HANDLE_PARAM(void) arg)
{
    struct compat_machphys_mfn_list xmml;
    l2_pgentry_t l2e;
    unsigned long v;
    compat_pfn_t mfn;
    unsigned int i;
    int rc = 0;

    switch ( cmd )
    {
    case XENMEM_set_memory_map:
    {
        struct compat_foreign_memory_map cmp;
        struct xen_foreign_memory_map *nat = COMPAT_ARG_XLAT_VIRT_BASE;

        if ( copy_from_guest(&cmp, arg, 1) )
            return -EFAULT;

#define XLAT_memory_map_HNDL_buffer(_d_, _s_) \
        guest_from_compat_handle((_d_)->buffer, (_s_)->buffer)
        XLAT_foreign_memory_map(nat, &cmp);
#undef XLAT_memory_map_HNDL_buffer

        rc = arch_memory_op(cmd, guest_handle_from_ptr(nat, void));

        break;
    }

    case XENMEM_memory_map:
    case XENMEM_machine_memory_map:
    {
        struct compat_memory_map cmp;
        struct xen_memory_map *nat = COMPAT_ARG_XLAT_VIRT_BASE;

        if ( copy_from_guest(&cmp, arg, 1) )
            return -EFAULT;

#define XLAT_memory_map_HNDL_buffer(_d_, _s_) \
        guest_from_compat_handle((_d_)->buffer, (_s_)->buffer)
        XLAT_memory_map(nat, &cmp);
#undef XLAT_memory_map_HNDL_buffer

        rc = arch_memory_op(cmd, guest_handle_from_ptr(nat, void));
        if ( rc < 0 )
            break;

#define XLAT_memory_map_HNDL_buffer(_d_, _s_) ((void)0)
        XLAT_memory_map(&cmp, nat);
#undef XLAT_memory_map_HNDL_buffer
        if ( __copy_to_guest(arg, &cmp, 1) )
            rc = -EFAULT;

        break;
    }

    case XENMEM_set_pod_target:
    case XENMEM_get_pod_target:
    {
        struct compat_pod_target cmp;
        struct xen_pod_target *nat = COMPAT_ARG_XLAT_VIRT_BASE;

        if ( copy_from_guest(&cmp, arg, 1) )
            return -EFAULT;

        XLAT_pod_target(nat, &cmp);

        rc = arch_memory_op(cmd, guest_handle_from_ptr(nat, void));
        if ( rc < 0 )
            break;

        if ( rc == __HYPERVISOR_memory_op )
            hypercall_xlat_continuation(NULL, 2, 0x2, nat, arg);

        XLAT_pod_target(&cmp, nat);

        if ( __copy_to_guest(arg, &cmp, 1) )
        {
            if ( rc == __HYPERVISOR_memory_op )
                hypercall_cancel_continuation(current);
            rc = -EFAULT;
        }

        break;
    }

    case XENMEM_machphys_mapping:
    {
        struct domain *d = current->domain;
        struct compat_machphys_mapping mapping = {
            .v_start = MACH2PHYS_COMPAT_VIRT_START(d),
            .v_end   = MACH2PHYS_COMPAT_VIRT_END,
            .max_mfn = MACH2PHYS_COMPAT_NR_ENTRIES(d) - 1
        };

        if ( !opt_pv32 )
            return -EOPNOTSUPP;

        if ( copy_to_guest(arg, &mapping, 1) )
            rc = -EFAULT;

        break;
    }

    case XENMEM_machphys_mfn_list:
    case XENMEM_machphys_compat_mfn_list:
    {
        unsigned long limit;
        compat_pfn_t last_mfn;

        if ( !opt_pv32 )
            return -EOPNOTSUPP;

        if ( copy_from_guest(&xmml, arg, 1) )
            return -EFAULT;

        limit = (unsigned long)(compat_machine_to_phys_mapping + max_page);
        if ( limit > RDWR_COMPAT_MPT_VIRT_END )
            limit = RDWR_COMPAT_MPT_VIRT_END;
        for ( i = 0, v = RDWR_COMPAT_MPT_VIRT_START, last_mfn = 0;
              (i != xmml.max_extents) && (v < limit);
              i++, v += 1 << L2_PAGETABLE_SHIFT )
        {
            l2e = compat_idle_pg_table_l2[l2_table_offset(v)];
            if ( l2e_get_flags(l2e) & _PAGE_PRESENT )
                mfn = l2e_get_pfn(l2e);
            else
                mfn = last_mfn;
            ASSERT(mfn);
            if ( copy_to_compat_offset(xmml.extent_start, i, &mfn, 1) )
                return -EFAULT;
            last_mfn = mfn;
        }

        xmml.nr_extents = i;
        if ( __copy_to_guest(arg, &xmml, 1) )
            rc = -EFAULT;

        break;
    }

    case XENMEM_get_sharing_freed_pages:
        return mem_sharing_get_nr_saved_mfns();

    case XENMEM_get_sharing_shared_pages:
        return mem_sharing_get_nr_shared_mfns();

#ifdef CONFIG_MEM_PAGING
    case XENMEM_paging_op:
        return mem_paging_memop(guest_handle_cast(arg, xen_mem_paging_op_t));
#endif

#ifdef CONFIG_MEM_SHARING
    case XENMEM_sharing_op:
        return mem_sharing_memop(guest_handle_cast(arg, xen_mem_sharing_op_t));
#endif

    default:
        rc = -ENOSYS;
        break;
    }

    return rc;
}

#ifdef CONFIG_PV
DEFINE_XEN_GUEST_HANDLE(mmuext_op_compat_t);

int cf_check compat_mmuext_op(
    XEN_GUEST_HANDLE_PARAM(void) arg, unsigned int count,
    XEN_GUEST_HANDLE_PARAM(uint) pdone, unsigned int foreigndom)
{
    unsigned int i, preempt_mask;
    int rc = 0;
    XEN_GUEST_HANDLE_PARAM(mmuext_op_compat_t) cmp_uops =
        guest_handle_cast(arg, mmuext_op_compat_t);
    XEN_GUEST_HANDLE_PARAM(mmuext_op_t) nat_ops;

    if ( unlikely(count == MMU_UPDATE_PREEMPTED) &&
         likely(guest_handle_is_null(cmp_uops)) )
    {
        set_xen_guest_handle(nat_ops, NULL);
        return do_mmuext_op(nat_ops, count, pdone, foreigndom);
    }

    preempt_mask = count & MMU_UPDATE_PREEMPTED;
    count ^= preempt_mask;

    if ( unlikely(!guest_handle_okay(cmp_uops, count)) )
        return -EFAULT;

    set_xen_guest_handle(nat_ops, COMPAT_ARG_XLAT_VIRT_BASE);

    for ( ; count; count -= i )
    {
        mmuext_op_t *nat_op = nat_ops.p;
        unsigned int limit = COMPAT_ARG_XLAT_SIZE / sizeof(*nat_op);
        int err;

        for ( i = 0; i < min(limit, count); ++i )
        {
            mmuext_op_compat_t cmp_op;
            enum XLAT_mmuext_op_arg1 arg1;
            enum XLAT_mmuext_op_arg2 arg2;

            if ( unlikely(__copy_from_guest(&cmp_op, cmp_uops, 1) != 0) )
            {
                rc = -EFAULT;
                break;
            }

            switch ( cmp_op.cmd )
            {
            case MMUEXT_PIN_L1_TABLE:
            case MMUEXT_PIN_L2_TABLE:
            case MMUEXT_PIN_L3_TABLE:
            case MMUEXT_PIN_L4_TABLE:
            case MMUEXT_UNPIN_TABLE:
            case MMUEXT_NEW_BASEPTR:
            case MMUEXT_CLEAR_PAGE:
            case MMUEXT_COPY_PAGE:
                arg1 = XLAT_mmuext_op_arg1_mfn;
                break;
            default:
                arg1 = XLAT_mmuext_op_arg1_linear_addr;
                break;
            case MMUEXT_NEW_USER_BASEPTR:
                rc = -EINVAL;
                /* fallthrough */
            case MMUEXT_TLB_FLUSH_LOCAL:
            case MMUEXT_TLB_FLUSH_MULTI:
            case MMUEXT_TLB_FLUSH_ALL:
            case MMUEXT_FLUSH_CACHE:
                arg1 = -1;
                break;
            }

            if ( rc )
                break;

            switch ( cmp_op.cmd )
            {
            case MMUEXT_SET_LDT:
                arg2 = XLAT_mmuext_op_arg2_nr_ents;
                break;
            case MMUEXT_TLB_FLUSH_MULTI:
            case MMUEXT_INVLPG_MULTI:
                arg2 = XLAT_mmuext_op_arg2_vcpumask;
                break;
            case MMUEXT_COPY_PAGE:
                arg2 = XLAT_mmuext_op_arg2_src_mfn;
                break;
            default:
                arg2 = -1;
                break;
            }

#define XLAT_mmuext_op_HNDL_arg2_vcpumask(_d_, _s_) \
        guest_from_compat_handle((_d_)->arg2.vcpumask, (_s_)->arg2.vcpumask)
            XLAT_mmuext_op(nat_op, &cmp_op);
#undef XLAT_mmuext_op_HNDL_arg2_vcpumask

            if ( rc || i >= limit )
                break;

            guest_handle_add_offset(cmp_uops, 1);
            ++nat_op;
        }

        err = do_mmuext_op(nat_ops, i | preempt_mask, pdone, foreigndom);

        if ( err )
        {
            BUILD_BUG_ON(__HYPERVISOR_mmuext_op <= 0);
            if ( err == __HYPERVISOR_mmuext_op )
            {
                struct cpu_user_regs *regs = guest_cpu_user_regs();
                struct mc_state *mcs = &current->mc_state;
                unsigned int arg1 = !(mcs->flags & MCSF_in_multicall)
                                    ? regs->ecx
                                    : mcs->call.args[1];
                unsigned int left = arg1 & ~MMU_UPDATE_PREEMPTED;

                BUG_ON(left == arg1 && left != i);
                BUG_ON(left > count);
                guest_handle_add_offset(nat_ops, i - left);
                guest_handle_subtract_offset(cmp_uops, left);
                left = 1;
                if ( arg1 != MMU_UPDATE_PREEMPTED )
                {
                    BUG_ON(!hypercall_xlat_continuation(&left, 4, 0x01, nat_ops,
                                                        cmp_uops));
                    if ( !(mcs->flags & MCSF_in_multicall) )
                        regs->ecx += count - i;
                    else
                        mcs->compat_call.args[1] += count - i;
                }
                else
                    BUG_ON(hypercall_xlat_continuation(&left, 4, 0));
                BUG_ON(left != arg1);
            }
            else
                BUG_ON(err > 0);
            rc = err;
        }

        if ( rc )
            break;

        /* Force do_mmuext_op() to not start counting from zero again. */
        preempt_mask = MMU_UPDATE_PREEMPTED;
    }

    return rc;
}
#endif /* CONFIG_PV */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
