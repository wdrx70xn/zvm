/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Yuhao Hu, Qingqiao Wang and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm.h>
#include <zephyr/zvm/vdev/vpl011.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vdev/vserial.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define PAUSE 0
#if PAUSE
K_SEM_DEFINE(thread_paused_sem, 0, 1);
#endif

K_SEM_DEFINE(connect_vm_sem, 0, 1);
K_THREAD_STACK_DEFINE(tx_it_emulator_thread_stack, 1024);

extern struct zvm_manage_info *zvm_overall_info;
static struct z_virt_serial_ctrl virt_serial_ctrl;
static struct k_thread tx_it_emulator_thread_data;
k_tid_t it_emulator_tid;

uint32_t virt_serial_count(void)
{
	uint32_t retval = 0;
	struct virt_serial *vs;
	sys_dnode_t *vserial_node ;

	SYS_DLIST_FOR_EACH_NODE(&virt_serial_ctrl.virt_serial_list, vserial_node) {
		vs = CONTAINER_OF(vserial_node, struct virt_serial, node);
		printk("[%d]serial name:%s ,vmid:%d\n",retval,vs->name,((struct z_vm *)vs->vm)->vmid);
		retval++;
	}

	return retval;
}


struct virt_serial* get_vserial(uint8_t vmid)
{
	struct virt_serial *serial = NULL;
	struct virt_serial *tmpserial ;
	sys_dnode_t *vserial_node;
	SYS_DLIST_FOR_EACH_NODE(&virt_serial_ctrl.virt_serial_list, vserial_node) {
		tmpserial = CONTAINER_OF(vserial_node, struct virt_serial , node);
		if (((struct z_vm *)(tmpserial->vm))->vmid == vmid) {
			serial = tmpserial;
		}
	}
    if (serial == NULL) {
        printk("No virtual serial devices[vmid:%d]\n",vmid);
        return NULL;
    }

	return serial;
}

struct virt_serial *virt_serial_create(const char *name,
				       int (*send) (struct virt_serial *, unsigned char *, int ),
				       void *priv)
{
	bool found = false;
	struct virt_serial *vserial = NULL;
	struct virt_pl011 *v_s;
	sys_dnode_t *vserial_node;

	if (!name) {
		return NULL;
	}
	SYS_DLIST_FOR_EACH_NODE(&virt_serial_ctrl.virt_serial_list, vserial_node) {
		vserial = CONTAINER_OF(vserial_node, struct virt_serial, node);
		if (strcmp(name, vserial->name) == 0) {
			found = true;
			break;
		}
	}

	if (found) {
		v_s = (struct virt_pl011 *)priv;
		vserial->send = send;
		vserial->vm = v_s->vm;
		vserial->priv = priv;
		vserial->count=0;
		return vserial;
	}

	vserial = (struct virt_serial *)k_malloc(sizeof(struct virt_serial));
	memset(vserial, 0, sizeof(struct virt_serial));
    memset(&vserial->send_buffer, 0, sizeof(vserial->send_buffer));

	if (!vserial) {
		return NULL;
	}
	vserial->count = 0;

	if (strlen(name) >= sizeof(vserial->name)) {
		k_free(vserial);
		return NULL;
	}else {
		strncpy(vserial->name, name, sizeof(vserial->name));
	}

	v_s = (struct virt_pl011 *)priv;
	vserial->send = send;
	vserial->vm = v_s->vm;
	vserial->priv = priv;
	sys_dnode_init(&vserial->node);
	sys_dlist_append(&virt_serial_ctrl.virt_serial_list, &vserial->node);

	return vserial;
}

int virt_serial_destroy(struct virt_serial *vserial)
{
	const struct shell *vs_shell = shell_backend_uart_get_ptr();
	vs_shell->ctx->bypass = NULL;
	sys_dlist_remove(&vserial->node);
	k_free(vserial);
	return 0;
}

static void vserial_it_emulator_thread(void *ctrl, void *arg2, void *arg3)
{
	struct virt_pl011 *vpl011 = NULL;
	while (1) {
		k_sem_take(&connect_vm_sem, K_MSEC(3000));
		if (virt_serial_ctrl.connecting) {
			vpl011 = (struct virt_pl011 *)virt_serial_ctrl.connecting_virt_serial->priv;
			while (virt_serial_ctrl.connecting) {
				if (vpl011->enabled & vpl011->level) {
					set_virq_to_vm(virt_serial_ctrl.connecting_virt_serial->vm, vpl011->irq);
				}
				k_msleep(200);
			}
			#if PAUSE
			k_sem_give(&thread_paused_sem);
			#endif
		}
	}
}

static void init_vserial_it_emulator_thread(void)
{
    it_emulator_tid = k_thread_create(&tx_it_emulator_thread_data,
                         tx_it_emulator_thread_stack,
                         K_THREAD_STACK_SIZEOF(tx_it_emulator_thread_stack),
                         vserial_it_emulator_thread, NULL, NULL, NULL,
                         CONFIG_SHELL_THREAD_PRIORITY-3,
                         0, K_FOREVER);

    k_thread_name_set(it_emulator_tid, "vserial_it_emulator");
    k_thread_cpu_mask_clear(it_emulator_tid);
    k_thread_cpu_mask_enable(it_emulator_tid, 0);
	k_thread_start(it_emulator_tid);
#if PAUSE
	k_thread_suspend(it_emulator_tid);
#endif
}

static int virt_serial_ctrl_init(void)
{
	memset(&virt_serial_ctrl, 0, sizeof(virt_serial_ctrl));

	sys_dlist_init(&virt_serial_ctrl.virt_serial_list);
	init_vserial_it_emulator_thread();

	return 0;
}

SYS_INIT(virt_serial_ctrl_init, POST_KERNEL, CONFIG_VIRT_SERIAL_CTRL_INIT_PRIORITY);

void uart_poll_out_to_host(unsigned char data)
{
	const struct shell *vs_shell = shell_backend_uart_get_ptr();
	const struct device *dev=((struct shell_uart_common *)vs_shell->iface->ctx)->dev;
	uart_poll_out(dev,data);
}

void virt_serial_disconnect_all(const struct shell *vs_shell)
{
	shell_set_bypass(vs_shell, NULL);
	((struct virt_pl011 *)(virt_serial_ctrl.connecting_virt_serial->priv))->connecting= false;
	virt_serial_ctrl.connecting = false;
	virt_serial_ctrl.connecting_vm_id = -1;
	uart_poll_out_to_host('\n');
#if PAUSE
	k_sem_take(&thread_paused_sem, K_MSEC(1000));
	k_thread_suspend(it_emulator_tid);
#endif
}


void transfer(const struct shell *vs_shell, uint8_t *data, size_t len)
{
	if (data[0] == EXIT_VSERIAL_KEY) {
		shell_fprintf(vs_shell,
			SHELL_VT100_COLOR_YELLOW,
			"disconnect\n");
		virt_serial_disconnect_all(vs_shell);
	}else {
		virt_serial_ctrl.connecting_virt_serial->send(virt_serial_ctrl.connecting_virt_serial, data, len);
	}
}

void virt_serial_connect(uint8_t vm_id)
{
	char data = '\r';
	struct virt_serial *serial;
	struct shell_uart_int_driven *shell_uart;
	const struct shell *vs_shell;
	vs_shell = shell_backend_uart_get_ptr();
#if PAUSE
	k_thread_resume(it_emulator_tid);
#endif
	serial = get_vserial(vm_id);
	virt_serial_ctrl.connecting = true;
	virt_serial_ctrl.connecting_vm_id = vm_id;
	virt_serial_ctrl.connecting_virt_serial = serial;
	((struct virt_pl011 *)(virt_serial_ctrl.connecting_virt_serial->priv))->connecting= true;
    barrier_dsync_fence_full();
	shell_set_bypass(vs_shell, transfer);
	shell_uart = (struct shell_uart_int_driven *)vs_shell->iface->ctx;
	ring_buf_put(&shell_uart->rx_ringbuf, (uint8_t *)&data, 1);
	shell_fprintf(vs_shell,
		SHELL_VT100_COLOR_YELLOW,
		"Connecting VM ID:%d\n", vm_id);
	barrier_dsync_fence_full();
	k_sem_give(&connect_vm_sem);
}

int switch_virtual_serial_handler(const struct shell *vs_shell, size_t argc, char **argv)
{
	uint16_t id;

    if (argc > 1) {
		if (argv[1][1]!='\0') {
			ZVM_LOG_WARN("Only supports VM ID with a length of 1.\n");
			return 0;
		}
		id = *argv[1];
		if (id > '9' || id < '0') {
			ZVM_LOG_WARN("Invalid VM ID %c\n", id);
			return 0;
		}
		id = id - 48;
		if (id > CONFIG_MAX_VM_NUM - 1) {
			ZVM_LOG_WARN("Max VM ID is %d\n", CONFIG_MAX_VM_NUM - 1);
			return 0;
		}
        if (!(BIT(id) & zvm_overall_info->alloced_vmid)) {
			ZVM_LOG_WARN("VM ID %d not allocated\n", id);
			return 0;
		} else if (get_vm_status(id)!=VM_STATE_RUNNING)
		{
			ZVM_LOG_WARN("VM %d is not running\n", id);
			return 0;
		} else {
			virt_serial_ctrl.connecting = true;
		}

		if (virt_serial_ctrl.connecting) {
			virt_serial_connect(id);
		}
    } else {
        ZVM_LOG_INFO("Reachable virtual serial:\n");
		virt_serial_count();
    }

    return 0;
}
