#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#define arch_lock_acquire_barrier() smp_mb()
#define arch_lock_release_barrier() smp_mb()

#define arch_lock_relax() cpu_relax()
#define arch_lock_signal() do { \
} while(0)

#define arch_lock_signal_wmb()  arch_lock_signal()

#endif /* __ASM_SPINLOCK_H */
