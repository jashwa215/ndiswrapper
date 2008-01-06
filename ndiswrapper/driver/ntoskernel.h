/*
 *  Copyright (C) 2006-2007 Giridhar Pemmasani
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 */

#ifndef _NTOSKERNEL_H_
#define _NTOSKERNEL_H_

#ifndef CONFIG_WIRELESS_EXT
#warning "wirelss devices are not supported by this kernel "\
	"as CONFIG_WIRELESS_EXT is not enabled"
#endif

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/kmod.h>

#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/random.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/usb.h>
#include <linux/spinlock.h>
#include <asm/mman.h>
#include <linux/version.h>
#include <linux/etherdevice.h>
#include <net/iw_handler.h>
#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/highmem.h>
#include <linux/percpu.h>

#include "winnt_types.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
#include <linux/kthread.h>
#endif
/* Interrupt backwards compatibility stuff */
#include <linux/interrupt.h>
#ifndef IRQ_HANDLED
#define IRQ_HANDLED
#define IRQ_NONE
#define irqreturn_t void
#endif

/* Workqueue / task queue backwards compatibility stuff */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,41)
#include <linux/workqueue.h>
/* pci functions in 2.6 kernels have problems allocating dma buffers,
 * but seem to work fine with dma functions
 */
#include <asm/dma-mapping.h>

#define PCI_DMA_ALLOC_COHERENT(pci_dev,size,dma_handle)			\
	dma_alloc_coherent(&pci_dev->dev,size,dma_handle,		\
			   GFP_KERNEL | __GFP_REPEAT)
#define PCI_DMA_FREE_COHERENT(pci_dev,size,cpu_addr,dma_handle)		\
	dma_free_coherent(&pci_dev->dev,size,cpu_addr,dma_handle)
#define PCI_DMA_MAP_SINGLE(pci_dev,addr,size,direction)		\
	dma_map_single(&pci_dev->dev,addr,size,direction)
#define PCI_DMA_UNMAP_SINGLE(pci_dev,dma_handle,size,direction)		\
	dma_unmap_single(&pci_dev->dev,dma_handle,size,direction)
#define MAP_SG(pci_dev, sglist, nents, direction)		\
	dma_map_sg(&pci_dev->dev, sglist, nents, direction)
#define UNMAP_SG(pci_dev, sglist, nents, direction)		\
	dma_unmap_sg(&pci_dev->dev, sglist, nents, direction)

#else // linux version <= 2.5.41

#define PCI_DMA_ALLOC_COHERENT(dev,size,dma_handle)	\
	pci_alloc_consistent(dev,size,dma_handle)
#define PCI_DMA_FREE_COHERENT(dev,size,cpu_addr,dma_handle)	\
	pci_free_consistent(dev,size,cpu_addr,dma_handle)
#define PCI_DMA_MAP_SINGLE(dev,addr,size,direction)	\
	pci_map_single(dev,addr,size,direction)
#define PCI_DMA_UNMAP_SINGLE(dev,dma_handle,size,direction)	\
	pci_unmap_single(dev,dma_handle,size,direction)
#define MAP_SG(dev, sglist, nents, direction)		\
	pci_map_sg(dev, sglist, nents, direction)
#define UNMAP_SG(dev, sglist, nents, direction)		\
	pci_unmap_sg(dev, sglist, nents, direction)

#include <linux/smp_lock.h>

/* RedHat kernels define irqs_disabled this way */
#ifndef irqs_disabled
#define irqs_disabled()                \
({                                     \
	unsigned long flags;	       \
       __save_flags(flags);            \
       !(flags & (1<<9));              \
})
#endif

#endif // LINUX_VERSION_CODE

#define prepare_wait_condition(task, var, value)	\
do {							\
	var = value;					\
	task = current;					\
	mb();						\
} while (0)

/* Wait in wait_state (e.g., TASK_INTERRUPTIBLE) for condition to
 * become true; timeout is either jiffies (> 0) to wait or 0 to wait
 * forever.
 * When timeout == 0, return value is
 *    > 0 if condition becomes true, or
 *    < 0 if signal is pending on the thread.
 * When timeout > 0, return value is
 *    > 0 if condition becomes true before timeout,
 *    < 0 if signal is pending on the thread before timeout, or
 *    0 if timedout (condition may have become true at the same time)
 */

#define wait_condition(condition, timeout, wait_state)		\
({								\
	long ret = timeout ? timeout : 1;			\
	while (1) {						\
		if (signal_pending(current)) {			\
			ret = -ERESTARTSYS;			\
			break;					\
		}						\
		set_current_state(wait_state);			\
		if (condition) {				\
			__set_current_state(TASK_RUNNING);	\
			break;					\
		}						\
		if (timeout) {					\
			ret = schedule_timeout(ret);		\
			if (!ret)				\
				break;				\
		} else						\
			schedule();				\
	}							\
	ret;							\
})

#ifdef USE_OWN_WQ

typedef struct {
	spinlock_t lock;
	struct task_struct *task;
	struct completion *completion;
	char name[16];
	int pid;
	/* whether any work_structs pending? <0 implies quit */
	int pending;
	/* list of work_structs pending */
	struct list_head work_list;
} workqueue_struct_t;

typedef struct {
	struct list_head list;
	void (*func)(void *data);
	void *data;
	/* whether/on which workqueue scheduled */
	workqueue_struct_t *workq;
} work_struct_t;

#define initialize_work(work, pfunc, pdata)			\
	do {							\
		(work)->func = (pfunc);				\
		(work)->data = (pdata);				\
		(work)->workq = NULL;				\
	} while (0)

#undef create_singlethread_workqueue
#define create_singlethread_workqueue wrap_create_wq
#undef destroy_workqueue
#define destroy_workqueue wrap_destroy_wq
#undef queue_work
#define queue_work wrap_queue_work
#undef flush_workqueue
#define flush_workqueue wrap_flush_wq

workqueue_struct_t *wrap_create_wq(const char *name);
void wrap_destroy_wq(workqueue_struct_t *workq);
void wrap_queue_work(workqueue_struct_t *workq, work_struct_t *work) wfastcall;
void wrap_cancel_work(work_struct_t *work);
void wrap_flush_wq(workqueue_struct_t *workq);
typedef void *worker_param_t;
#define worker_param_data(param, type, member) param

#else // USE_OWN_WQ

typedef struct workqueue_struct workqueue_struct_t;
typedef struct work_struct work_struct_t;

#if defined(INIT_WORK_NAR) || defined(INIT_DELAYED_WORK_DEFERRABLE)
#define initialize_work(work, func, data) INIT_WORK(work, func)
typedef struct work_struct *worker_param_t;
#define worker_param_data(param, type, member)	\
	container_of(param, type, member)
#else
#define initialize_work(work, func, data) INIT_WORK(work, func, data)
typedef void *worker_param_t;
#define worker_param_data(param, type, member) param
#endif // INIT_WORK_NAR

#endif // USE_OWN_WQ

struct nt_thread *wrap_worker_init(workqueue_struct_t *wq);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
#define WRAP_MODULE_PARM_INT(name, perm) module_param(name, int, perm)
#define WRAP_MODULE_PARM_STRING(name, perm) module_param(name, charp, perm)
#else
#define WRAP_MODULE_PARM_INT(name, perm) MODULE_PARM(name, "i")
#define WRAP_MODULE_PARM_STRING(name, perm) MODULE_PARM(name, "s")
#endif

#ifndef LOCK_PREFIX
#ifdef LOCK
#define LOCK_PREFIX LOCK
#else
#ifdef CONFIG_SMP
#define LOCK_PREFIX "lock ; "
#else
#define LOCK_PREFIX ""
#endif
#endif
#endif

#ifndef NETDEV_TX_OK
#define NETDEV_TX_OK 0
#endif

#ifndef NETDEV_TX_BUSY
#define NETDEV_TX_BUSY 1
#endif

#ifndef CHECKSUM_HW
#define CHECKSUM_HW CHECKSUM_PARTIAL
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#ifndef container_of
#define container_of(ptr, type, member)					\
({									\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);		\
	(type *)( (char *)__mptr - offsetof(type,member) );		\
})
#endif

#ifndef virt_addr_valid
#define virt_addr_valid(addr) VALID_PAGE(virt_to_page(addr))
#endif

#ifndef SET_NETDEV_DEV
#define SET_NETDEV_DEV(net,pdev) do { } while (0)
#endif

#define usb_set_intfdata(intf, data) do { } while (0)

#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#ifndef offset_in_page
#define offset_in_page(p) ((unsigned long)(p) & ~PAGE_MASK)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,23)
#define HAVE_ETHTOOL 1
#endif

#ifndef PMSG_SUSPEND
#ifdef PM_SUSPEND
/* this is not correct - the value of PM_SUSPEND is different from
 * PMSG_SUSPEND, but ndiswrapper doesn't care about the value when
 * suspending */
#define PMSG_SUSPEND PM_SUSPEND
#define PSMG_ON PM_ON
#else
typedef u32 pm_message_t;
#define PMSG_SUSPEND 2
#define PMSG_ON 0
#endif
#endif

#ifndef PCI_D0
#define PCI_D0 0
#endif

#ifndef PCI_D3hot
#define PCI_D3hot 3
#endif

#ifndef PCI_D3cold
#define PCI_D3cold 3
#endif

#ifndef PM_EVENT_SUSPEND
#define PM_EVENT_SUSPEND 2
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define pci_choose_state(dev, state) (state)
#endif

#if !defined(HAVE_NETDEV_PRIV)
#define netdev_priv(dev)  ((dev)->priv)
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
#define ISR_PT_REGS_PARAM_DECL
#define ISR_PT_REGS_ARG
#else
#define ISR_PT_REGS_PARAM_DECL , struct pt_regs *regs
#define ISR_PT_REGS_ARG , NULL
#endif

#ifndef flush_icache_range
#define flush_icache_range(start, end) do { } while (0)
#endif

#ifndef CHECKSUM_PARTIAL
#define CHECKSUM_PARTIAL CHECKSUM_HW
#endif

#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ
#endif

#define memcpy_skb(skb, from, length)			\
	memcpy(skb_put(skb, length), from, length)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define thread_priority(thread) (thread)->nice
#define set_thread_priority(thread, prio) (thread)->nice = (prio)
#else
#define thread_priority(thread) task_nice(thread)
#define set_thread_priority(thread, prio) set_user_nice(thread, prio)
#endif

#ifndef DMA_24BIT_MASK
#define DMA_24BIT_MASK 0x0000000000ffffffULL
#endif

#ifndef DMA_30BIT_MASK
#define DMA_30BIT_MASK 0x000000003fffffffULL
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22)
#define wrap_kmem_cache_create(name, size, align, flags)	\
	kmem_cache_create(name, size, align, flags, NULL, NULL)
#else
#define wrap_kmem_cache_create(name, size, align, flags)	\
	kmem_cache_create(name, size, align, flags, NULL)
#endif

#include "ndiswrapper.h"
#include "pe_linker.h"
#include "wrapmem.h"
#include "lin2win.h"
#include "loader.h"

#include "compat.h"

#if !defined(CONFIG_USB) && defined(CONFIG_USB_MODULE)
#define CONFIG_USB 1
#endif

#if defined(DISABLE_USB)
#undef CONFIG_USB
#undef CONFIG_USB_MODULE
#endif

/* TICK is 100ns */
#define TICKSPERSEC		10000000
#define TICKSPERMSEC		10000
#define SECSPERDAY		86400
#define TICKSPERJIFFY		((TICKSPERSEC + HZ - 1) / HZ)

#define int_div_round(x, y)	(((x) + (y) - 1) / (y))

/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970	((369 * 365 + 89) * (u64)SECSPERDAY)
#define TICKS_1601_TO_1970	(SECS_1601_TO_1970 * TICKSPERSEC)

/* 100ns units to HZ; if sys_time is negative, relative to current
 * clock, otherwise from year 1601 */
#define SYSTEM_TIME_TO_HZ(sys_time)					\
	(((sys_time) <= 0) ? \
	 int_div_round(((u64)HZ * (-(sys_time))), TICKSPERSEC) :	\
	 int_div_round(((s64)HZ * ((sys_time) - ticks_1601())), TICKSPERSEC))

#define MSEC_TO_HZ(ms) int_div_round((ms * HZ), 1000)
#define USEC_TO_HZ(us) int_div_round((us * HZ), 1000000)

extern u64 wrap_ticks_to_boot;

static inline u64 ticks_1601(void)
{
	return wrap_ticks_to_boot + (u64)jiffies * TICKSPERJIFFY;
}

typedef void (*generic_func)(void);

struct wrap_export {
	const char *name;
	generic_func func;
};

#ifdef CONFIG_X86_64

#define WIN_SYMBOL(name, argc)						\
	{#name, (generic_func) win2lin_ ## name ## _ ## argc}
#define WIN_WIN_SYMBOL(name, argc)					\
	{#name, (generic_func) win2lin__win_ ## name ## _ ## argc}
#define WIN_FUNC_DECL(name, argc)			\
	typeof(name) win2lin_ ## name ## _ ## argc;
#define WIN_FUNC_PTR(name, argc) win2lin_ ## name ## _ ## argc

#else

#define WIN_SYMBOL(name, argc) {#name, (generic_func)name}
#define WIN_WIN_SYMBOL(name, argc) {#name, (generic_func)_win_ ## name}
#define WIN_FUNC_DECL(name, argc)
#define WIN_FUNC_PTR(name, argc) name

#endif

#define WIN_FUNC(name, argc) name
/* map name s to f - if f is different from s */
#define WIN_SYMBOL_MAP(s, f)

#define POOL_TAG(A, B, C, D)					\
	((ULONG)((A) + ((B) << 8) + ((C) << 16) + ((D) << 24)))

struct pe_image {
	char name[MAX_DRIVER_NAME_LEN];
	UINT (*entry)(struct driver_object *, struct unicode_string *) wstdcall;
	void *image;
	int size;
	int type;

	IMAGE_NT_HEADERS *nt_hdr;
	IMAGE_OPTIONAL_HEADER *opt_hdr;
};

struct ndis_miniport_block;

struct wrap_timer {
	struct nt_slist slist;
	struct timer_list timer;
	struct nt_timer *nt_timer;
	long repeat;
#ifdef TIMER_DEBUG
	unsigned long wrap_timer_magic;
#endif
};

struct ntos_work_item {
	struct nt_list list;
	void *arg1;
	void *arg2;
	NTOS_WORK_FUNC func;
};

struct wrap_device_setting {
	struct nt_list list;
	char name[MAX_SETTING_NAME_LEN];
	char value[MAX_SETTING_VALUE_LEN];
	void *encoded;
};

struct wrap_bin_file {
	char name[MAX_DRIVER_NAME_LEN];
	size_t size;
	void *data;
};

#define WRAP_DRIVER_CLIENT_ID 1

struct wrap_driver {
	struct nt_list list;
	struct driver_object *drv_obj;
	char name[MAX_DRIVER_NAME_LEN];
	char version[MAX_SETTING_VALUE_LEN];
	unsigned short num_pe_images;
	struct pe_image pe_images[MAX_DRIVER_PE_IMAGES];
	unsigned short num_bin_files;
	struct wrap_bin_file *bin_files;
	struct nt_list wrap_devices;
	struct nt_list settings;
	int dev_type;
	struct wrap_ndis_driver *ndis_driver;
};

enum hw_status {
	HW_INITIALIZED = 1, HW_SUSPENDED, HW_HALTED, HW_PRESENT,
};

struct wrap_device {
	/* first part is (de)initialized once by loader */
	struct nt_list list;
	int dev_bus;
	int vendor;
	int device;
	int subvendor;
	int subdevice;
	char conf_file_name[MAX_DRIVER_NAME_LEN];
	char driver_name[MAX_DRIVER_NAME_LEN];
	struct wrap_driver *driver;
	struct nt_list settings;

	/* rest should be (de)initialized when a device is
	 * (un)plugged */
	struct cm_resource_list *resource_list;
	unsigned long hw_status;
	struct device_object *pdo;
	union {
		struct {
			struct pci_dev *pdev;
			enum device_power_state wake_state;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,9)
			u32 pci_state[16];
#endif
		} pci;
		struct {
			struct usb_device *udev;
			struct usb_interface *intf;
			int num_alloc_urbs;
			struct nt_list wrap_urb_list;
		} usb;
	};
	union {
		struct wrap_ndis_device *wnd;
	};
};

#define wrap_is_pci_bus(dev_bus)			\
	(WRAP_BUS(dev_bus) == WRAP_PCI_BUS ||		\
	 WRAP_BUS(dev_bus) == WRAP_PCMCIA_BUS)
#ifdef CONFIG_USB
/* earlier versions of ndiswrapper used 0 as USB_BUS */
#define wrap_is_usb_bus(dev_bus)			\
	(WRAP_BUS(dev_bus) == WRAP_USB_BUS ||		\
	 WRAP_BUS(dev_bus) == WRAP_INTERNAL_BUS)
#else
#define wrap_is_usb_bus(dev_bus) 0
#endif
#define wrap_is_bluetooth_device(dev_bus)			\
	(WRAP_DEVICE(dev_bus) == WRAP_BLUETOOTH_DEVICE1 ||	\
	 WRAP_DEVICE(dev_bus) == WRAP_BLUETOOTH_DEVICE2)

extern workqueue_struct_t *ndis_wq;
#define schedule_ndis_work(work_struct) queue_work(ndis_wq, (work_struct))

extern workqueue_struct_t *wrapndis_wq;
#define schedule_wrapndis_work(work_struct)	\
	queue_work(wrapndis_wq, (work_struct))

#define USE_NTOS_WQ 1

#ifdef USE_NTOS_WQ
extern workqueue_struct_t *ntos_wq;
#define schedule_ntos_work(work_struct) queue_work(ntos_wq, (work_struct))
#define schedule_work(work_struct) queue_work(ntos_wq, (work_struct))
#else
#define schedule_ntos_work(work_struct) schedule_work(work_struct)
#endif

#define atomic_unary_op(var, size, oper)				\
do {									\
	if (size == 1)							\
		__asm__ __volatile__(					\
			LOCK_PREFIX oper "b %b0\n\t" : "+m" (var));	\
	else if (size == 2)						\
		__asm__ __volatile__(					\
			LOCK_PREFIX oper "w %w0\n\t" : "+m" (var));	\
	else if (size == 4)						\
		__asm__ __volatile__(					\
			LOCK_PREFIX oper "l %0\n\t" : "+m" (var));	\
	else if (size == 8)						\
		__asm__ __volatile__(					\
			LOCK_PREFIX oper "q %q0\n\t" : "+m" (var));	\
	else {								\
		extern void _invalid_op_size_(void);			\
		_invalid_op_size_();					\
	}								\
} while (0)

#define atomic_inc_var_size(var, size) atomic_unary_op(var, size, "inc")

#define atomic_inc_var(var) atomic_inc_var_size(var, sizeof(var))

#define atomic_dec_var_size(var, size) atomic_unary_op(var, size, "dec")

#define atomic_dec_var(var) atomic_dec_var_size(var, sizeof(var))

#define pre_atomic_add(var, i)					\
({								\
	typeof(var) pre;					\
	__asm__ __volatile__(					\
		LOCK_PREFIX "xadd %0, %1\n\t"			\
		: "=r"(pre), "+m"(var)				\
		: "0"(i));					\
	pre;							\
})

#define post_atomic_add(var, i) (pre_atomic_add(var, i) + i)

#ifndef in_atomic
#define in_atomic() in_interrupt()
#endif

//#define DEBUG_IRQL 1

#ifdef DEBUG_IRQL
#define assert_irql(cond)						\
do {									\
	KIRQL _irql_ = current_irql();					\
	if (!(cond)) {							\
		WARNING("assertion '%s' failed: %d", #cond, _irql_);	\
		DBG_BLOCK(1) {						\
			dump_stack();					\
		}							\
	}								\
} while (0)
#else
#define assert_irql(cond) do { } while (0)
#endif

/* When preempt is enabled, we should preempt_disable to raise IRQL to
 * DISPATCH_LEVEL, to be consistent with the semantics. However, using
 * a mutex instead, so that only ndiswrapper threads run one at a time
 * on a processor when at DISPATCH_LEVEL seems to be enough. So that
 * is what we will use until we learn otherwise. If
 * preempt_(en|dis)able is required for some reason, comment out
 * following #define. */

#define WRAP_PREEMPT 1

#if !defined(CONFIG_PREEMPT) || defined(CONFIG_PREEMPT_RT)
#ifndef WRAP_PREEMPT
#define WRAP_PREEMPT 1
#endif
#endif

//#undef WRAP_PREEMPT

#ifdef WRAP_PREEMPT

typedef struct {
	int count;
	struct mutex lock;
#ifdef CONFIG_SMP
	typeof(current->cpus_allowed) cpus_allowed;
#endif
	struct task_struct *task;
} irql_info_t;

DECLARE_PER_CPU(irql_info_t, irql_info);

static inline KIRQL raise_irql(KIRQL newirql)
{
	irql_info_t *info;

	assert(newirql == DISPATCH_LEVEL);
	info = &get_cpu_var(irql_info);
	if (info->task == current) {
		assert(info->count > 0);
		assert(mutex_is_locked(&info->lock));
#if defined(CONFIG_SMP) && defined(DEBUG)
		do {
			cpumask_t cpumask;
			cpumask = cpumask_of_cpu(smp_processor_id());
			cpus_xor(cpumask, cpumask, current->cpus_allowed);
			assert(cpus_empty(cpumask));
		} while (0);
#endif
		info->count++;
		put_cpu_var(irql_info);
		return DISPATCH_LEVEL;
	}
	/* TODO: is this enough to pin down to current cpu? */
#ifdef CONFIG_SMP
	assert(task_cpu(current) == smp_processor_id());
	info->cpus_allowed = current->cpus_allowed;
	current->cpus_allowed = cpumask_of_cpu(smp_processor_id());
#endif
	put_cpu_var(irql_info);
	mutex_lock(&info->lock);
	assert(info->count == 0);
	assert(info->task == NULL);
	info->count = 1;
	info->task = current;
	return PASSIVE_LEVEL;
}

static inline void lower_irql(KIRQL oldirql)
{									
	irql_info_t *info;

	assert(oldirql <= DISPATCH_LEVEL);
	info = &get_cpu_var(irql_info);
	assert(info->task == current);
	assert(mutex_is_locked(&info->lock));
	assert(info->count > 0);
	if (--info->count == 0) {
		info->task = NULL;
#ifdef CONFIG_SMP
		current->cpus_allowed = info->cpus_allowed;
#endif
		mutex_unlock(&info->lock);
	}
	put_cpu_var(irql_info);
}

static inline KIRQL current_irql(void)
{
	int count;
	if (in_irq() || irqs_disabled())
		EXIT4(return DIRQL);
	if (in_atomic() || in_interrupt())
		EXIT4(return SOFT_IRQL);
	count = get_cpu_var(irql_info).count;
	put_cpu_var(irql_info);
	if (count)
		EXIT6(return DISPATCH_LEVEL);
	else
		EXIT6(return PASSIVE_LEVEL);
}

#else

static inline KIRQL current_irql(void)
{
	if (in_irq() || irqs_disabled())
		EXIT4(return DIRQL);
	if (in_interrupt())
		EXIT4(return SOFT_IRQL);
	if (in_atomic())
		EXIT6(return DISPATCH_LEVEL);
	else
		EXIT6(return PASSIVE_LEVEL);
}

static inline KIRQL raise_irql(KIRQL newirql)
{
	KIRQL ret = in_atomic() ? DISPATCH_LEVEL : PASSIVE_LEVEL;
	assert(newirql == DISPATCH_LEVEL);
	assert(current_irql() <= DISPATCH_LEVEL);
	preempt_disable();
	return ret;
}

static inline void lower_irql(KIRQL oldirql)
{
	assert(current_irql() == DISPATCH_LEVEL);
	preempt_enable();
}

#endif

#define irql_gfp() (in_atomic() ? GFP_ATOMIC : GFP_KERNEL)

/* Windows spinlocks are of type ULONG_PTR which is not big enough to
 * store Linux spinlocks; so we implement Windows spinlocks using
 * ULONG_PTR space with our own functions/macros */

/* Windows seems to use 0 for unlocked state of spinlock - if Linux
 * convention of 1 for unlocked state is used, at least prism54 driver
 * crashes */

#define NT_SPIN_LOCK_UNLOCKED 0
#define NT_SPIN_LOCK_LOCKED 1

static inline void  nt_spin_lock_init(NT_SPIN_LOCK *lock)
{
	*lock = NT_SPIN_LOCK_UNLOCKED;
}

#ifdef CONFIG_SMP

static inline void nt_spin_lock(NT_SPIN_LOCK *lock)
{
	__asm__ __volatile__(
		"1:\t"
		"  xchgl %1, %0\n\t"
		"  testl %1, %1\n\t"
		"  jz 3f\n"
		"2:\t"
		"  rep; nop\n\t"
		"  cmpl %2, %0\n\t"
		"  je 1b\n\t"
		"  jmp 2b\n"
		"3:\n\t"
		: "+m" (*lock)
		: "r" (NT_SPIN_LOCK_LOCKED), "i" (NT_SPIN_LOCK_UNLOCKED));
}

static inline void nt_spin_unlock(NT_SPIN_LOCK *lock)
{
	*lock = NT_SPIN_LOCK_UNLOCKED;
}

#else // CONFIG_SMP

#define nt_spin_lock(lock) do { } while (0)

#define nt_spin_unlock(lock)  do { } while (0)

#endif // CONFIG_SMP

/* When kernel would've disabled preempt (e.g., in interrupt
 * handlers), we need to fake preempt so driver thinks it is running
 * at right IRQL */

/* raise IRQL to given (higher) IRQL if necessary before locking */
static inline KIRQL nt_spin_lock_irql(NT_SPIN_LOCK *lock, KIRQL newirql)
{
	KIRQL oldirql = raise_irql(newirql);
	nt_spin_lock(lock);
	return oldirql;
}

/* lower IRQL to given (lower) IRQL if necessary after unlocking */
static inline void nt_spin_unlock_irql(NT_SPIN_LOCK *lock, KIRQL oldirql)
{
	nt_spin_unlock(lock);
	lower_irql(oldirql);
}

#define nt_spin_lock_irqsave(lock, flags)				\
do {									\
	local_irq_save(flags);						\
	preempt_disable();						\
	nt_spin_lock(lock);						\
} while (0)

#define nt_spin_unlock_irqrestore(lock, flags)				\
do {									\
	nt_spin_unlock(lock);						\
	preempt_enable_no_resched();					\
	local_irq_restore(flags);					\
	preempt_check_resched();					\
} while (0)

#define atomic_insert_list_head(oldhead, head, newhead)			\
do {									\
	oldhead = (typeof(oldhead))head;				\
} while (cmpxchg(&(head), oldhead, newhead) != (typeof(head))oldhead)

#define atomic_remove_list_head(head, newhead)				\
({									\
	typeof(head) oldhead;						\
	do {								\
		oldhead = head;						\
		if (!oldhead)						\
			break;						\
	} while (cmpxchg(&(head), oldhead, newhead) != oldhead);	\
	oldhead;							\
})

static inline ULONG SPAN_PAGES(void *ptr, SIZE_T length)
{
	return PAGE_ALIGN(((unsigned long)ptr & (PAGE_SIZE - 1)) + length)
		>> PAGE_SHIFT;
}

#ifdef CONFIG_X86_64

/* TODO: can these be implemented without using spinlock? */

static inline struct nt_slist *PushEntrySList(nt_slist_header *head,
					      struct nt_slist *entry,
					      NT_SPIN_LOCK *lock)
{
	KIRQL irql = nt_spin_lock_irql(lock, DISPATCH_LEVEL);
	entry->next = head->next;
	head->next = entry;
	head->depth++;
	nt_spin_unlock_irql(lock, irql);
	TRACE4("%p, %p, %p", head, entry, entry->next);
	return entry->next;
}

static inline struct nt_slist *PopEntrySList(nt_slist_header *head,
					     NT_SPIN_LOCK *lock)
{
	struct nt_slist *entry;
	KIRQL irql = nt_spin_lock_irql(lock, DISPATCH_LEVEL);
	entry = head->next;
	if (entry) {
		head->next = entry->next;
		head->depth--;
	}
	nt_spin_unlock_irql(lock, irql);
	TRACE4("%p, %p", head, entry);
	return entry;
}

#else

#define u64_low_32(x) ((u32)x)
#define u64_high_32(x) ((u32)(x >> 32))

static inline u64 cmpxchg8b(volatile u64 *ptr, u64 old, u64 new)
{
	u64 prev;

	__asm__ __volatile__(
		"\n"
		LOCK_PREFIX "cmpxchg8b %0\n"
		: "+m" (*ptr), "=A" (prev)
		: "A" (old), "b" (u64_low_32(new)), "c" (u64_high_32(new)));
	return prev;
}

/* slist routines below update slist atomically - no need for
 * spinlocks */

static inline struct nt_slist *PushEntrySList(nt_slist_header *head,
					      struct nt_slist *entry,
					      NT_SPIN_LOCK *lock)
{
	nt_slist_header old, new;
	do {
		old.align = head->align;
		entry->next = old.next;
		new.next = entry;
		new.depth = old.depth + 1;
	} while (cmpxchg8b(&head->align, old.align, new.align) != old.align);
	TRACE4("%p, %p, %p", head, entry, old.next);
	return old.next;
}

static inline struct nt_slist *PopEntrySList(nt_slist_header *head,
					     NT_SPIN_LOCK *lock)
{
	struct nt_slist *entry;
	nt_slist_header old, new;
	do {
		old.align = head->align;
		entry = old.next;
		if (!entry)
			break;
		new.next = entry->next;
		new.depth = old.depth - 1;
	} while (cmpxchg8b(&head->align, old.align, new.align) != old.align);
	TRACE4("%p, %p", head, entry);
	return entry;
}

#endif

#define sleep_hz(n)					\
do {							\
	set_current_state(TASK_INTERRUPTIBLE);		\
	schedule_timeout(n);				\
} while (0)

int ntoskernel_init(void);
void ntoskernel_exit(void);
int ntoskernel_init_device(struct wrap_device *wd);
void ntoskernel_exit_device(struct wrap_device *wd);
void *allocate_object(ULONG size, enum common_object_type type,
		      struct unicode_string *name);
void free_object(void *object);

int usb_init(void);
void usb_exit(void);
int usb_init_device(struct wrap_device *wd);
void usb_exit_device(struct wrap_device *wd);
void usb_cancel_pending_urbs(void);

int crt_init(void);
void crt_exit(void);
int rtl_init(void);
void rtl_exit(void);
int wrap_procfs_init(void);
void wrap_procfs_remove(void);

int link_pe_images(struct pe_image *pe_image, unsigned short n);

int stricmp(const char *s1, const char *s2);
void dump_bytes(const char *name, const u8 *from, int len);
struct mdl *allocate_init_mdl(void *virt, ULONG length);
void free_mdl(struct mdl *mdl);
struct driver_object *find_bus_driver(const char *name);
void free_custom_extensions(struct driver_extension *drv_obj_ext);
struct nt_thread *get_current_nt_thread(void);
u64 ticks_1601(void);
int schedule_ntos_work_item(NTOS_WORK_FUNC func, void *arg1, void *arg2);
void wrap_init_timer(struct nt_timer *nt_timer, enum timer_type type,
		     struct ndis_miniport_block *nmb);
BOOLEAN wrap_set_timer(struct nt_timer *nt_timer, unsigned long expires_hz,
		       unsigned long repeat_hz, struct kdpc *kdpc);
void wrap_free_timer(struct nt_timer *timer);

LONG InterlockedDecrement(LONG volatile *val) wfastcall;
LONG InterlockedIncrement(LONG volatile *val) wfastcall;
struct nt_list *ExInterlockedInsertHeadList
	(struct nt_list *head, struct nt_list *entry,
	 NT_SPIN_LOCK *lock) wfastcall;
struct nt_list *ExInterlockedInsertTailList
	(struct nt_list *head, struct nt_list *entry,
	 NT_SPIN_LOCK *lock) wfastcall;
struct nt_list *ExInterlockedRemoveHeadList
	(struct nt_list *head, NT_SPIN_LOCK *lock) wfastcall;
NTSTATUS IofCallDriver(struct device_object *dev_obj, struct irp *irp) wfastcall;
KIRQL KfRaiseIrql(KIRQL newirql) wfastcall;
void KfLowerIrql(KIRQL oldirql) wfastcall;
KIRQL KfAcquireSpinLock(NT_SPIN_LOCK *lock) wfastcall;
void KfReleaseSpinLock(NT_SPIN_LOCK *lock, KIRQL oldirql) wfastcall;
void IofCompleteRequest(struct irp *irp, CHAR prio_boost) wfastcall;
void KefReleaseSpinLockFromDpcLevel(NT_SPIN_LOCK *lock) wfastcall;

LONG ObfReferenceObject(void *object) wfastcall;
void ObfDereferenceObject(void *object) wfastcall;

#define ObReferenceObject(object) ObfReferenceObject(object)
#define ObDereferenceObject(object) ObfDereferenceObject(object)

void WRITE_PORT_UCHAR(ULONG_PTR port, UCHAR value) wstdcall;
UCHAR READ_PORT_UCHAR(ULONG_PTR port) wstdcall;

#undef ExAllocatePoolWithTag
void *ExAllocatePoolWithTag(enum pool_type pool_type, SIZE_T size,
			    ULONG tag) wstdcall;
#if defined(ALLOC_DEBUG) && ALLOC_DEBUG > 1
#define ExAllocatePoolWithTag(pool_type, size, tag)			\
	wrap_ExAllocatePoolWithTag(pool_type, size, tag, __FILE__, __LINE__)
#endif

void ExFreePool(void *p) wstdcall;
ULONG MmSizeOfMdl(void *base, ULONG length) wstdcall;
void *MmMapIoSpace(PHYSICAL_ADDRESS phys_addr, SIZE_T size,
		   enum memory_caching_type cache) wstdcall;
void MmUnmapIoSpace(void *addr, SIZE_T size) wstdcall;
void MmProbeAndLockPages(struct mdl *mdl, KPROCESSOR_MODE access_mode,
			 enum lock_operation operation) wstdcall;
void MmUnlockPages(struct mdl *mdl) wstdcall;
void KeInitializeEvent(struct nt_event *nt_event,
		       enum event_type type, BOOLEAN state) wstdcall;
LONG KeSetEvent(struct nt_event *nt_event, KPRIORITY incr,
		BOOLEAN wait) wstdcall;
LONG KeResetEvent(struct nt_event *nt_event) wstdcall;
void KeClearEvent(struct nt_event *nt_event) wstdcall;
void KeInitializeDpc(struct kdpc *kdpc, void *func, void *ctx) wstdcall;
BOOLEAN queue_kdpc(struct kdpc *kdpc);
BOOLEAN dequeue_kdpc(struct kdpc *kdpc);

void KeFlushQueuedDpcs(void) wstdcall;
NTSTATUS IoConnectInterrupt(struct kinterrupt **kinterrupt,
			    PKSERVICE_ROUTINE service_routine,
			    void *service_context, NT_SPIN_LOCK *lock,
			    ULONG vector, KIRQL irql, KIRQL synch_irql,
			    enum kinterrupt_mode interrupt_mode,
			    BOOLEAN shareable, KAFFINITY processor_enable_mask,
			    BOOLEAN floating_save) wstdcall;
void IoDisconnectInterrupt(struct kinterrupt *interrupt) wstdcall;
BOOLEAN KeSynchronizeExecution(struct kinterrupt *interrupt,
			       PKSYNCHRONIZE_ROUTINE synch_routine,
			       void *ctx) wstdcall;

NTSTATUS KeWaitForSingleObject(void *object, KWAIT_REASON reason,
			       KPROCESSOR_MODE waitmode, BOOLEAN alertable,
			       LARGE_INTEGER *timeout) wstdcall;
struct mdl *IoAllocateMdl(void *virt, ULONG length, BOOLEAN second_buf,
			  BOOLEAN charge_quota, struct irp *irp) wstdcall;
void MmBuildMdlForNonPagedPool(struct mdl *mdl) wstdcall;
void IoFreeMdl(struct mdl *mdl) wstdcall;
struct io_workitem *IoAllocateWorkItem(struct device_object *dev_obj) wstdcall;
void IoFreeWorkItem(struct io_workitem *io_workitem) wstdcall;
void IoQueueWorkItem(struct io_workitem *io_workitem, void *func,
		     enum work_queue_type queue_type, void *context) wstdcall;
NTSTATUS IoCreateDevice(struct driver_object *driver, ULONG dev_ext_length,
			struct unicode_string *dev_name, DEVICE_TYPE dev_type,
			ULONG dev_chars, BOOLEAN exclusive,
			struct device_object **dev_obj) wstdcall;
NTSTATUS IoCreateSymbolicLink(struct unicode_string *link,
			      struct unicode_string *dev_name) wstdcall;
void IoDeleteDevice(struct device_object *dev) wstdcall;
void IoDetachDevice(struct device_object *topdev) wstdcall;
struct device_object *IoGetAttachedDevice(struct device_object *dev) wstdcall;
struct device_object *IoGetAttachedDeviceReference
	(struct device_object *dev) wstdcall;
NTSTATUS IoAllocateDriverObjectExtension
	(struct driver_object *drv_obj, void *client_id, ULONG extlen,
	 void **ext) wstdcall;
void *IoGetDriverObjectExtension(struct driver_object *drv,
				 void *client_id) wstdcall;
void IoFreeDriverObjectExtension(struct driver_object *drv_obj,
				 void *client_id) wstdcall;
struct device_object *IoAttachDeviceToDeviceStack
	(struct device_object *src, struct device_object *dst) wstdcall;
void KeInitializeEvent(struct nt_event *nt_event, enum event_type type,
		       BOOLEAN state) wstdcall;
struct irp *IoAllocateIrp(char stack_count, BOOLEAN charge_quota) wstdcall;
void IoFreeIrp(struct irp *irp) wstdcall;
BOOLEAN IoCancelIrp(struct irp *irp) wstdcall;
struct irp *IoBuildSynchronousFsdRequest
	(ULONG major_func, struct device_object *dev_obj, void *buf,
	 ULONG length, LARGE_INTEGER *offset, struct nt_event *event,
	 struct io_status_block *status) wstdcall;
struct irp *IoBuildAsynchronousFsdRequest
	(ULONG major_func, struct device_object *dev_obj, void *buf,
	 ULONG length, LARGE_INTEGER *offset,
	 struct io_status_block *status) wstdcall;
NTSTATUS PoCallDriver(struct device_object *dev_obj, struct irp *irp) wstdcall;

NTSTATUS IoPassIrpDown(struct device_object *dev_obj, struct irp *irp) wstdcall;
WIN_FUNC_DECL(IoPassIrpDown,2);
NTSTATUS IoSyncForwardIrp(struct device_object *dev_obj,
			  struct irp *irp) wstdcall;
NTSTATUS IoAsyncForwardIrp(struct device_object *dev_obj,
			   struct irp *irp) wstdcall;
NTSTATUS IoInvalidDeviceRequest(struct device_object *dev_obj,
				struct irp *irp) wstdcall;

KIRQL KeGetCurrentIrql(void) wstdcall;
void KeInitializeSpinLock(NT_SPIN_LOCK *lock) wstdcall;
void KeAcquireSpinLock(NT_SPIN_LOCK *lock, KIRQL *irql) wstdcall;
void KeReleaseSpinLock(NT_SPIN_LOCK *lock, KIRQL oldirql) wstdcall;
KIRQL KeAcquireSpinLockRaiseToDpc(NT_SPIN_LOCK *lock) wstdcall;

void IoAcquireCancelSpinLock(KIRQL *irql) wstdcall;
void IoReleaseCancelSpinLock(KIRQL irql) wstdcall;

void RtlCopyMemory(void *dst, const void *src, SIZE_T length) wstdcall;
NTSTATUS RtlUnicodeStringToAnsiString
	(struct ansi_string *dst, const struct unicode_string *src,
	 BOOLEAN dup) wstdcall;
NTSTATUS RtlAnsiStringToUnicodeString
	(struct unicode_string *dst, const struct ansi_string *src,
	 BOOLEAN dup) wstdcall;
void RtlInitAnsiString(struct ansi_string *dst, const char *src) wstdcall;
void RtlInitString(struct ansi_string *dst, const char *src) wstdcall;
void RtlInitUnicodeString(struct unicode_string *dest,
			  const wchar_t *src) wstdcall;
void RtlFreeUnicodeString(struct unicode_string *string) wstdcall;
void RtlFreeAnsiString(struct ansi_string *string) wstdcall;
LONG RtlCompareUnicodeString(const struct unicode_string *s1,
			     const struct unicode_string *s2,
			     BOOLEAN case_insensitive) wstdcall;
void RtlCopyUnicodeString(struct unicode_string *dst,
			  struct unicode_string *src) wstdcall;
void KeInitializeTimer(struct nt_timer *nt_timer) wstdcall;
void KeInitializeTimerEx(struct nt_timer *nt_timer,
			 enum timer_type type) wstdcall;
BOOLEAN KeSetTimerEx(struct nt_timer *nt_timer, LARGE_INTEGER duetime_ticks,
		     LONG period_ms, struct kdpc *kdpc) wstdcall;
BOOLEAN KeSetTimer(struct nt_timer *nt_timer, LARGE_INTEGER duetime_ticks,
		   struct kdpc *kdpc) wstdcall;
BOOLEAN KeCancelTimer(struct nt_timer *nt_timer) wstdcall;
void KeInitializeDpc(struct kdpc *kdpc, void *func, void *ctx) wstdcall;
struct nt_thread *KeGetCurrentThread(void) wstdcall;
NTSTATUS ObReferenceObjectByHandle(void *handle, ACCESS_MASK desired_access,
				   void *obj_type, KPROCESSOR_MODE access_mode,
				   void **object, void *handle_info) wstdcall;

void adjust_user_shared_data_addr(char *driver, unsigned long length);

#define IoCompleteRequest(irp, prio) IofCompleteRequest(irp, prio)
#define IoCallDriver(dev, irp) IofCallDriver(dev, irp)

#if defined(IO_DEBUG)
#define DUMP_IRP(irp)							\
do {									\
	struct io_stack_location *irp_sl;				\
	irp_sl = IoGetCurrentIrpStackLocation(irp);			\
	IOTRACE("irp: %p, stack size: %d, cl: %d, sl: %p, dev_obj: %p, " \
		"mj_fn: %d, minor_fn: %d, nt_urb: %p, event: %p",	\
		irp, irp->stack_count, (irp)->current_location,		\
		irp_sl, irp_sl->dev_obj, irp_sl->major_fn,		\
		irp_sl->minor_fn, IRP_URB(irp),				\
		(irp)->user_event);					\
} while (0)
#else
#define DUMP_IRP(irp) do { } while (0)
#endif

#endif // _NTOSKERNEL_H_
