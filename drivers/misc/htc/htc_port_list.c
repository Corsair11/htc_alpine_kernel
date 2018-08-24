/* arch/arm/mach-msm/htc_port_list.c
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/list.h>
#include <linux/htc_devices_dtb.h>
#include <net/tcp.h>

#define PORT_LIST_COUNT_MAX 128

static struct spinlock port_lock;
static struct wake_lock port_suspend_lock;
struct p_list {
	struct list_head list;
	int no;
};
static struct p_list curr_port_list;
#ifdef PACKET_FILTER_UDP
static struct p_list curr_port_list_udp;
#endif
struct class *p_class;
static struct miscdevice portlist_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "htc-portlist",
};

static int ril_debug_flag = 0;

#define PF_LOG_DBG(fmt, ...) do {                           \
		if (ril_debug_flag)                                 \
			printk(KERN_DEBUG "[K]" pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)

#define PF_LOG_INFO(fmt, ...) do {                         \
		if (ril_debug_flag)                                \
			printk(KERN_INFO "[K]" pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)

#define PF_LOG_ERR(fmt, ...) do {                     \
	if (ril_debug_flag)                               \
		printk(KERN_ERR "[K]" pr_fmt(fmt), ##__VA_ARGS__);  \
	} while (0)


#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

void port_list_dump_data(struct sock *sk)
{
	struct task_struct *task = current;
	struct inet_sock *inet = NULL;
	struct net *net = NULL;

	if ( ril_debug_flag == 0 ) {
		return;
	}

	if ( sk == NULL ) {
		return;
	}

	inet = inet_sk(sk);
	net = sock_net(sk);

	if ( net ) {
		PF_LOG_INFO("[Port list] %s: inuse=[%d]\n", __FUNCTION__, sock_prot_inuse_get(net, sk->sk_prot));
	} else {
		PF_LOG_INFO("[Port list] %s: net = null\n", __FUNCTION__);
	}

	if ( inet ) {
		PF_LOG_INFO("[Port list] %s: Local:%03d.%03d.%03d.%03d:%05d(0x%x) Remote:%03d.%03d.%03d.%03d:%05d(0x%x)\n", __FUNCTION__, NIPQUAD(inet->inet_rcv_saddr), ntohs(inet->inet_sport), inet->inet_rcv_saddr, NIPQUAD(inet->inet_daddr), ntohs(inet->inet_dport), inet->inet_daddr);
	} else {
		PF_LOG_INFO("[Port list] %s: inet = null\n", __FUNCTION__);
	}
	PF_LOG_INFO("[Port list] %s: sk->sk_shutdown = [%d], sock_flag(sk, SOCK_DONE)=[%d]\n", __FUNCTION__, sk->sk_shutdown, sock_flag(sk, SOCK_DONE));
	if ( sk->sk_socket )
		PF_LOG_INFO("[Port list] %s: sk->sk_socket->state=[%d]\n", __FUNCTION__, sk->sk_socket->state);
	PF_LOG_INFO("[Port list] %s: sk->sk_type=[%d]\n", __FUNCTION__, sk->sk_type);
	PF_LOG_INFO("[Port list] %s: sk->sk_family=[%d]\n", __FUNCTION__, sk->sk_family);
	PF_LOG_INFO("[Port list] %s: sk->sk_state=[%d]\n", __FUNCTION__, sk->sk_state);
	PF_LOG_INFO("[Port list] %s: sk->sk_reuse=[%d]\n", __FUNCTION__, sk->sk_reuse);
	PF_LOG_INFO("[Port list] %s: sk->sk_reuseport=[%d]\n", __FUNCTION__, sk->sk_reuseport);
	PF_LOG_INFO("[Port list] %s: sk->sk_flags=[%lu]\n", __FUNCTION__, sk->sk_flags);
	if (sock_flag(sk, SOCK_DEAD)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_DEAD]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_DONE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_DONE]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_URGINLINE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_URGINLINE]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_KEEPOPEN)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_KEEPOPEN]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_LINGER)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_LINGER]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_DESTROY)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_DESTROY]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_BROADCAST)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_BROADCAST]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_TIMESTAMP)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_TIMESTAMP]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_ZAPPED)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_ZAPPED]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_USE_WRITE_QUEUE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_USE_WRITE_QUEUE]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_DBG)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_DBG]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_RCVTSTAMP)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_RCVTSTAMP]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_RCVTSTAMPNS)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_RCVTSTAMPNS]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_LOCALROUTE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_LOCALROUTE]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_QUEUE_SHRUNK)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_QUEUE_SHRUNK]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_LOCALROUTE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_LOCALROUTE]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_MEMALLOC)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_MEMALLOC]\n", __FUNCTION__);
#if 0 
#endif
	if (sock_flag(sk, SOCK_TIMESTAMPING_RX_SOFTWARE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_TIMESTAMPING_RX_SOFTWARE]\n", __FUNCTION__);
#if 0 
#endif
	if (sock_flag(sk, SOCK_FASYNC)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_FASYNC]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_RXQ_OVFL)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_RXQ_OVFL]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_ZEROCOPY)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_ZEROCOPY]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_WIFI_STATUS)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_WIFI_STATUS]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_NOFCS)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_NOFCS]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_FILTER_LOCKED)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_FILTER_LOCKED]\n", __FUNCTION__);
	if (sock_flag(sk, SOCK_SELECT_ERR_QUEUE)) PF_LOG_INFO("[Port list] %s: sk->sk_flags[SOCK_SELECT_ERR_QUEUE]\n", __FUNCTION__);

	PF_LOG_INFO("[Port list] %s: task->state=[%d][%d]\n", __FUNCTION__, task->flags, task->flags & PF_EXITING);

	PF_LOG_INFO("=== Show stack ===\n");
	show_stack(0, 0);
	PF_LOG_INFO("=== Show stack end===\n");
}

int copy_port_list(uint16_t *gf_port_list, int max_count, int * count)
{
	int ret = 0;
	PF_LOG_INFO("[Port list][%s]\n", __func__);
	if ( gf_port_list == NULL ) {
		PF_LOG_INFO("[Port list][%s]copy_port_list = null\n", __func__);
		ret = -1;
		goto end;
	}

	if ( count == NULL ) {
		PF_LOG_INFO("[Port list][%s]count = null\n", __func__);
		ret = -1;
		goto end;
	}

{
	uint i = 0;
	struct list_head *listptr;
	struct p_list *entry;

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		PF_LOG_INFO("[Port list][%s][%d] = %d\n", __func__, i, entry->no);
		if ( i < max_count ) {
			gf_port_list[i] = entry->no;
			i++;
		}
	}
	*count = i;
}

	PF_LOG_INFO("[Port list][%s] count=[%d], r=[%d]\n", __func__, *count, ret);
end:
	return ret;
}
EXPORT_SYMBOL(copy_port_list);

int check_TCP_port_list_count(void)
{
	int ret = 0;
	uint count = 0;
	struct list_head *listptr;
	struct p_list *entry;
	unsigned long flags;

	spin_lock_irqsave(&port_lock, flags);

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		count++;
		PF_LOG_INFO("%s: [Port list] [%d] = %d\n", __func__, count, entry->no);
	}

	pr_info("%s: [Port list] count = [%d]\n", __func__, count);
	if ( count <= PORT_LIST_COUNT_MAX ) {
		ret = 1;
	}

	spin_unlock_irqrestore(&port_lock, flags);

	return ret;
}
EXPORT_SYMBOL(check_TCP_port_list_count);

static struct p_list *add_list(int no)
{
	struct p_list *ptr = NULL;
	struct list_head *listptr;
	struct p_list *entry;
	int get_list = 0;

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		if (entry->no == no) {
			PF_LOG_INFO("[Port list] TCP port[%d] is already in the list!", entry->no);
			get_list = 1;
			break;
		}
	}
	if (!get_list) {
		ptr = kmalloc(sizeof(struct p_list), GFP_ATOMIC);
		if (ptr) {
			ptr->no = no;
			list_add_tail(&ptr->list, &curr_port_list.list);
			PF_LOG_INFO("[Port list] TCP port[%d] added\n", no);
		}
	}
	return (ptr);
}
#ifdef PACKET_FILTER_UDP
static struct p_list *add_list_udp(int no)
{
	struct p_list *ptr = NULL;
	struct list_head *listptr;
	struct p_list *entry;
	int get_list = 0;

	list_for_each(listptr, &curr_port_list_udp.list) {
		entry = list_entry(listptr, struct p_list, list);
		if (entry->no == no) {
			PF_LOG_INFO("[Port list] UDP port[%d] is already in the list!", entry->no);
			get_list = 1;
			break;
		}
	}
	if (!get_list) {
		ptr = kmalloc(sizeof(struct p_list), GFP_ATOMIC);
		if (ptr) {
			ptr->no = no;
			list_add_tail(&ptr->list, &curr_port_list_udp.list);
			PF_LOG_INFO("[Port list] UDP port[%d] added\n", no);
		}
	}
	return (ptr);
}
#endif

static int remove_list(int no)
{
	struct list_head *listptr;
	struct p_list *entry;
	int get_list = 0;

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		if (entry->no == no) {
			PF_LOG_INFO("[Port list] TCP port[%d] removed\n", entry->no);
			list_del(&entry->list);
			kfree(entry);
			get_list = 1;
			break;
		}
	}
	if (!get_list) {
		PF_LOG_INFO("[Port list] TCP port[%d] failed to remove. Port number is not in list!\n", no);
		return -1;
	} else {
		return 0;
	}
}
#ifdef PACKET_FILTER_UDP
static void remove_list_udp(int no)
{
	struct list_head *listptr;
	struct p_list *entry;
	int get_list = 0;

	list_for_each(listptr, &curr_port_list_udp.list) {
		entry = list_entry(listptr, struct p_list, list);
		if (entry->no == no) {
			PF_LOG_INFO("[Port list] UDP port[%d] removed\n", entry->no);
			list_del(&entry->list);
			kfree(entry);
			get_list = 1;
			break;
		}
	}
}
#endif

int add_or_remove_port(struct sock *sk, int add_or_remove)
{
	struct inet_sock *inet = NULL;
	__be32 src = 0;
	__u16 srcp = 0;
	unsigned long flags;

	if ( sk == NULL ) {
		PF_LOG_INFO("[Port list] add_or_remove_port: sk = null\n");
		return 0;
	}

	inet = inet_sk(sk);
	if (!inet)
	{
		PF_LOG_INFO("[Port list] add_or_remove_port: inet = null\n");
		return 0;
	}

	src = inet->inet_rcv_saddr;
	srcp = ntohs(inet->inet_sport);

	
	if (sk->sk_protocol == IPPROTO_TCP && src != 0x0100007F && srcp != 0) {
		int err = 0;
		wake_lock(&port_suspend_lock);
		PF_LOG_INFO("[Port list] TCP port#: [%d]\n", srcp);
		spin_lock_irqsave(&port_lock, flags);

		if (add_or_remove) {
			struct p_list *plist = NULL;
			plist = add_list(srcp);

			if ( !plist ) {
				err = 1;
				port_list_dump_data(sk);
			}
		} else {
			int ret = 0;
			ret = remove_list(srcp);
			if ( ret != 0 ) {
				err = 1;
				port_list_dump_data(sk);
			}
		}

		spin_unlock_irqrestore(&port_lock, flags);
		wake_unlock(&port_suspend_lock);
	}

#ifdef PACKET_FILTER_UDP
	
	if (sk->sk_protocol == IPPROTO_UDP && src != 0x0100007F && srcp != 0) {
		wake_lock(&port_suspend_lock);
		spin_lock_irqsave(&port_lock, flags);
		if (add_or_remove)
			add_list_udp(srcp);
		else
			remove_list_udp(srcp);
		spin_unlock_irqrestore(&port_lock, flags);
		wake_unlock(&port_suspend_lock);
	}
#endif

	return 0;
}
EXPORT_SYMBOL(add_or_remove_port);

int get_port_list_debug_flag(void)
{
	return ril_debug_flag;
}
EXPORT_SYMBOL(get_port_list_debug_flag);

static int __init port_list_init(void)
{
	int ret;
	wake_lock_init(&port_suspend_lock, WAKE_LOCK_SUSPEND, "port_list");
	spin_lock_init(&port_lock);
	PF_LOG_INFO("[Port list] init()\n");

	
	if (get_kernel_flag() & KERNEL_FLAG_RIL_DBG_MEMCPY)
		ril_debug_flag = 1;

	
	memset(&curr_port_list, 0, sizeof(curr_port_list));
	INIT_LIST_HEAD(&curr_port_list.list);

	#ifdef PACKET_FILTER_UDP
	
	memset(&curr_port_list_udp, 0, sizeof(curr_port_list_udp));
	INIT_LIST_HEAD(&curr_port_list_udp.list);
	#endif

	ret = misc_register(&portlist_misc);
	if (ret < 0) {
		PF_LOG_ERR("[Port list] failed to register misc device!\n");
		goto err_misc_register;
	}

	p_class = class_create(THIS_MODULE, "htc_portlist");
	if (IS_ERR(p_class)) {
		ret = PTR_ERR(p_class);
		p_class = NULL;
		PF_LOG_ERR("[Port list] class_create failed!\n");
		goto err_class_create;
	}

	portlist_misc.this_device = device_create(p_class, NULL, 0 , NULL, "packet_filter");
	if (IS_ERR(portlist_misc.this_device)) {
		ret = PTR_ERR(portlist_misc.this_device);
		portlist_misc.this_device = NULL;
		PF_LOG_ERR("[Port list] device_create failed!\n");
		goto err_device_create;
	}

	return 0;

err_device_create:
	class_destroy(p_class);
err_class_create:
	misc_deregister(&portlist_misc);
err_misc_register:
	return ret;
}

static void __exit port_list_exit(void)
{
	int ret;
	struct list_head *listptr;
	struct p_list *entry;

	device_destroy(p_class, 0);
	class_destroy(p_class);

	ret = misc_deregister(&portlist_misc);
	if (ret < 0)
		PF_LOG_ERR("[Port list] failed to unregister misc device!\n");

	list_for_each(listptr, &curr_port_list.list) {
		entry = list_entry(listptr, struct p_list, list);
		kfree(entry);
	}
	#ifdef PACKET_FILTER_UDP
	list_for_each(listptr, &curr_port_list_udp.list) {
		entry = list_entry(listptr, struct p_list, list);
		kfree(entry);
	}
	#endif
}

late_initcall(port_list_init);
module_exit(port_list_exit);

MODULE_AUTHOR("Mio Su <Mio_Su@htc.com>");
MODULE_DESCRIPTION("HTC port list driver");
MODULE_LICENSE("GPL");
