/*
 * Copyright 2021-2022 HNU
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_ZSHM_H_
#define ZEPHYR_INCLUDE_ZSHM_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <zephyr/sys/atomic.h>
#define ZSHM_START_PHY_ADDR DT_REG_ADDR(DT_NODELABEL(zshm))
#define ZSHM_NOTIFY_VIRQ_NUM    DT_IRQN(DT_NODELABEL(zshm))
#define ZSHM_UNIT_SIZE 0x200000 
#define ZSHM_NOTIFY_SPACE_SIZE 0x100000 
#define ZSHM_NOTIFY_INFO_SPACE_SIZE 0x100000

#define ZSHM_NOTIFY_UNIT_SIZE CONFIG_MMU_PAGE_SIZE
#define ZSHM_NOTIFY_INFO_UNIT_SIZE CONFIG_MMU_PAGE_SIZE
#define ZSHM_SIZE (DT_REG_SIZE(DT_NODELABEL(zshm)) - ZSHM_NOTIFY_SPACE_SIZE - ZSHM_NOTIFY_INFO_SPACE_SIZE)
#define ZSHM_UNIT_MAX_NUMS \
    (ZSHM_SIZE / ZSHM_UNIT_SIZE < ZSHM_NOTIFY_SPACE_SIZE / ZSHM_NOTIFY_UNIT_SIZE ? \
    ZSHM_SIZE / ZSHM_UNIT_SIZE : ZSHM_NOTIFY_SPACE_SIZE / ZSHM_NOTIFY_UNIT_SIZE)
#define ZSHM_NOTIFY_MAX_NUMS ZSHM_UNIT_MAX_NUMS
#define ZSHM_NOTIFY_MAX_VM_NUMS ZSHM_UNIT_MAX_NUMS
#define ZSHM_NOTIFY_INFO_SPACE_START_PHY_ADDR (ZSHM_START_PHY_ADDR + ZSHM_UNIT_MAX_NUMS * ZSHM_UNIT_SIZE)
#define ZSHM_NOTIFY_SPACE_START_PHY_ADDR (ZSHM_NOTIFY_INFO_SPACE_START_PHY_ADDR + ZSHM_NOTIFY_INFO_SPACE_SIZE)


#define ZSHM_VM_MAX_UNIT_NUMS ZSHM_UNIT_MAX_NUMS
#define ZSHM_VM_SHM_WR_START_PHY_ADDR ZSHM_START_PHY_ADDR
#define ZSHM_VM_NOTIFY_SPACE_START_PHY_ADDR (ZSHM_VM_SHM_WR_START_PHY_ADDR + ZSHM_UNIT_SIZE)
#define ZSHM_VM_NOTIFY_INFO_SPACE_START_PHY_ADDR (ZSHM_VM_NOTIFY_SPACE_START_PHY_ADDR + ZSHM_NOTIFY_UNIT_SIZE)
#define ZSHM_VM_SHM_RD_START_PHY_ADDR (ZSHM_VM_NOTIFY_INFO_SPACE_START_PHY_ADDR + ZSHM_NOTIFY_INFO_UNIT_SIZE)
#define ZSHM_VM_NOTIFY_OFFSET 10
#define ZSHM_VMID_MARKER_OFFSET  (ZSHM_UNIT_SIZE - 2)
#define ZSHM_MESSAGE_SLOT_COUNT   16      
#define ZSHM_SEND_SLOT_LIMIT      10   
#define ZSHM_SYSCALL_SLOT         10    
#define HEARTBEAT_FLAG 254
#define MAX_DATA_SIZE (1024)  
typedef struct zshm_message {
    atomic_t receivers;
    uint8_t data[MAX_DATA_SIZE];
    uint8_t priority;
}zshm_message;

#define PRIORITY_LEVELS 1
#define RING_SIZE 128    

typedef struct ring_entry {
    uint8_t sender_addr;      
    uint8_t msg_slot;    
} ring_entry;   

typedef struct priority_ring {
    uint8_t prod_tail;  
    atomic_t send_lock;   
    uint8_t cons_head;  
    struct ring_entry entries[RING_SIZE];  
}priority_ring;

typedef struct vm_priority_rings {
    struct priority_ring rings[PRIORITY_LEVELS];
}vm_priority_rings;   


typedef struct zshm_alloc_unit {
    uint32_t id;
    uint32_t vm_id;
    uint32_t zshm_paddr;
    uint32_t zshm_notify_addr;
    uint32_t zshm_notify_info_addr;
}zshm_alloc_unit;



typedef struct zshm_driver_api{
    int (*notify) (struct z_virt_dev *edev, uint64_t addr,uint64_t *data);
}zshm_driver_api;

int zshm_alloc_vm_shm(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc);

#ifdef CONFIG_HEARTBEAT
#define HEARTBEAT_TIMEOUT_MS 3000

int64_t get_vm_heartbeat(uint16_t vm_id);
void clear_vm_heartbeat(uint16_t vm_id);
#endif

#endif /* ZEPHYR_INCLUDE_ZSHM_H_ */