/*
 * 
 */
#ifndef SCMI_SMT_H
#define SCMI_SMT_H

#include <zephyr/logging/log.h>

/**
 * struct scmi_smt_header - Description of the shared memory message buffer
 *
 * SMT stands for Shared Memory based Transport.
 * SMT uses 28 byte header prior message payload to handle the state of
 * the communication channel realized by the shared memory area and
 * to define SCMI protocol information the payload relates to.
 */
struct scmi_smt_header {
	uint32_t reserved;
	uint32_t channel_status;
#define SCMI_SHMEM_CHAN_STAT_CHANNEL_ERROR	BIT(1)
#define SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE	BIT(0)
	uint32_t reserved1[2];
	uint32_t flags;
#define SCMI_SHMEM_FLAG_INTR_ENABLED		BIT(0)
	uint32_t length;
	uint32_t msg_header;
	uint8_t msg_payload[0];
};

#define SMT_HEADER_TOKEN(token)		(((token) << 18) & GENMASK(31, 18))
#define SMT_HEADER_PROTOCOL_ID(proto)	(((proto) << 10) & GENMASK(17, 10))
#define SMT_HEADER_MESSAGE_TYPE(type)	(((type) << 18) & GENMASK(9, 8))
#define SMT_HEADER_MESSAGE_ID(id)	((id) & GENMASK(7, 0))

/**
 * struct scmi_smt - Description of a SMT memory buffer
 * @buf:	Shared memory base address
 * @size:	Shared memory byte size
 */
struct scmi_smt {
	uint8_t *buf;
	size_t size;
};

#define memset_io(a, b, c)	memset((void *)(a), (b), (c))
#define memcpy_fromio(a, b, c)	memcpy((a), (void *)(b), (c))
#define memcpy_toio(a, b, c)	memcpy((void *)(a), (b), (c))

#define SCMI_PROTOCOL_ID_BASE           0x10
#define SCMI_PROTOCOL_ID_POWER_DOMAIN   0x11
#define SCMI_PROTOCOL_ID_SYSTEM         0x12
#define SCMI_PROTOCOL_ID_PERF           0x13
#define SCMI_PROTOCOL_ID_CLOCK          0x14
#define SCMI_PROTOCOL_ID_SENSOR         0x15
#define SCMI_PROTOCOL_ID_RESET_DOMAIN   0x16

#define SCMI_CLOCK_RATE_SET             0x5
#define SCMI_CLOCK_RATE_GET             0x6
#define SCMI_CLOCK_CONFIG_SET           0x7

#define SCMI_CLK_RATE_ASYNC_NOTIFY	BIT(0)
#define SCMI_CLK_RATE_ASYNC_NORESP	(BIT(0) | BIT(1))
#define SCMI_CLK_RATE_ROUND_DOWN	0
#define SCMI_CLK_RATE_ROUND_UP		BIT(2)
#define SCMI_CLK_RATE_ROUND_CLOSEST	BIT(3)

#endif /* SCMI_SMT_H */
