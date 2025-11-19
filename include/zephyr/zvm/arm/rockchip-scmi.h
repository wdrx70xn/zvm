#ifndef __ROCKCHIP_SCMI_H__
#define __ROCKCHIP_SCMI_H__

#include <zephyr/logging/log.h>
#include <zephyr/arch/arm64/arm-smccc.h>
#include <zephyr/zvm/arm/rockchip-smt.h>

#define SIP_SCMI_MSG		0x82000010

/* scmi-clocks indices */
#define SCMI_CLK_CPUL			0
#define SCMI_CLK_DSU			1
#define SCMI_CLK_CPUB01			2
#define SCMI_CLK_CPUB23			3
#define SCMI_CLK_DDR			4
#define SCMI_CLK_GPU			5
#define SCMI_CLK_NPU			6
#define SCMI_CLK_SBUS			7
#define SCMI_PCLK_SBUS			8
#define SCMI_CCLK_SD			9
#define SCMI_DCLK_SD			10
#define SCMI_ACLK_SECURE_NS		11
#define SCMI_HCLK_SECURE_NS		12
#define SCMI_TCLK_WDT			13
#define SCMI_KEYLADDER_CORE		14
#define SCMI_KEYLADDER_RNG		15
#define SCMI_ACLK_SECURE_S		16
#define SCMI_HCLK_SECURE_S		17
#define SCMI_PCLK_SECURE_S		18
#define SCMI_CRYPTO_RNG			19
#define SCMI_CRYPTO_CORE		20
#define SCMI_CRYPTO_PKA			21
#define SCMI_SPLL			    22
#define SCMI_HCLK_SD			23

/** SCMI Structures */

/**
 * struct scmi_clk_state_in - Message payload for CLOCK_CONFIG_SET command
 * @clock_id:	SCMI clock ID
 * @attributes:	Attributes of the targets clock state
 */
struct scmi_clk_state_in {
	uint32_t clock_id;
	uint32_t attributes;
};
/**
 * struct scmi_clk_state_out - Response payload for CLOCK_CONFIG_SET command
 * @status:	SCMI command status
 */
struct scmi_clk_state_out {
	int status;
};
/**
 * struct scmi_clk_state_in - Message payload for CLOCK_RATE_GET command
 * @clock_id:	SCMI clock ID
 * @attributes:	Attributes of the targets clock state
 */
struct scmi_clk_rate_get_in {
	uint32_t clock_id;
};

/**
 * struct scmi_clk_rate_get_out - Response payload for CLOCK_RATE_GET command
 * @status:	SCMI command status
 * @rate_lsb:	32bit LSB of the clock rate in Hertz
 * @rate_msb:	32bit MSB of the clock rate in Hertz
 */
struct scmi_clk_rate_get_out {
	uint32_t status;
	uint32_t rate_lsb;
	uint32_t rate_msb;
};

/**
 * struct scmi_clk_state_in - Message payload for CLOCK_RATE_SET command
 * @clock_id:	SCMI clock ID
 * @flags:	Flags for the clock rate set request
 * @rate_lsb:	32bit LSB of the clock rate in Hertz
 * @rate_msb:	32bit MSB of the clock rate in Hertz
 */
struct scmi_clk_rate_set_in {
	uint32_t flags;
	uint32_t clock_id;
	uint32_t rate_lsb;
	uint32_t rate_msb;
};

/**
 * struct scmi_clk_rate_set_out - Response payload for CLOCK_RATE_SET command
 * @status:	SCMI command status
 */
struct scmi_clk_rate_set_out {
	int status;
};


/*
 * struct scmi_msg - Context of a SCMI message sent and the response received
 *
 * @protocol_id:	SCMI protocol ID
 * @message_id:		SCMI message ID for a defined protocol ID
 * @in_msg:		Pointer to the message payload sent by the driver
 * @in_msg_sz:		Byte size of the message payload sent
 * @out_msg:		Pointer to buffer to store response message payload
 * @out_msg_sz:		Byte size of the response buffer and response payload
 */
struct scmi_msg {
	unsigned int protocol_id;
	unsigned int message_id;
	uint8_t *in_msg;
	size_t in_msg_sz;
	uint8_t *out_msg;
	size_t out_msg_sz;
};

/* Helper macro to match a message on input/output array references */
#define SCMI_MSG_IN(_protocol, _message, _in_array, _out_array) \
	(struct scmi_msg){			\
		.protocol_id = (_protocol),	\
		.message_id = (_message),	\
		.in_msg = (uint8_t *)&(_in_array),	\
		.in_msg_sz = sizeof(_in_array),	\
		.out_msg = (uint8_t *)&(_out_array),	\
		.out_msg_sz = sizeof(_out_array),	\
	}

/** SCMI msg Func*/
/**
 * Write SCMI message @msg into a SMT shared buffer @smt.
 * Return 0 on success and with a negative errno in case of error.
 */
int scmi_write_msg_to_smt(struct scmi_msg *msg){

	struct scmi_smt_header *hdr = (void *)(0x0010f000);

	if ((!msg->in_msg && msg->in_msg_sz) ||
	    (!msg->out_msg && msg->out_msg_sz))
		return -1;

	if (!(hdr->channel_status & SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE)) {
		return -1;
	}

	/* Load message in shared memory */
	hdr->channel_status &= ~SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE;
	hdr->length = msg->in_msg_sz + sizeof(hdr->msg_header);
	hdr->msg_header = SMT_HEADER_TOKEN(0) |
			  SMT_HEADER_MESSAGE_TYPE(0) |
			  SMT_HEADER_PROTOCOL_ID(msg->protocol_id) |
			  SMT_HEADER_MESSAGE_ID(msg->message_id);

	memcpy_toio(hdr->msg_payload, msg->in_msg, msg->in_msg_sz);

	return 0;
}

/**
 * Read SCMI message from a SMT shared buffer @smt and copy it into @msg.
 * Return 0 on success and with a negative errno in case of error.
 */
int scmi_read_resp_from_smt(struct scmi_msg *msg){

	struct scmi_smt_header *hdr = (void *)(0x0010f000);

	if (!(hdr->channel_status & SCMI_SHMEM_CHAN_STAT_CHANNEL_FREE)) {
		return -1;
	}

	if (hdr->channel_status & SCMI_SHMEM_CHAN_STAT_CHANNEL_ERROR) {
		return -1;
	}

	if (hdr->length > msg->out_msg_sz + sizeof(hdr->msg_header)) {
		return -1;
	}

	/* Get the data */
	msg->out_msg_sz = hdr->length - sizeof(hdr->msg_header);
	memcpy_fromio(msg->out_msg, hdr->msg_payload, msg->out_msg_sz);

	return 0;
}

/**
 * Clear SMT flags in shared buffer to allow further message exchange
 */
void scmi_clear_smt_channel()
{
	struct scmi_smt_header *hdr = (void *)(0x0010f000);

	hdr->channel_status &= ~SCMI_SHMEM_CHAN_STAT_CHANNEL_ERROR;
}


static int scmi_smccc_process_msg(struct scmi_msg *msg)
{
	struct arm_smccc_res res;
	int ret;

	ret = scmi_write_msg_to_smt(msg);
	if (ret)
		return ret;

	arm_smccc_smc(SIP_SCMI_MSG, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == -1)
		ret = -1;
	else
		ret = scmi_read_resp_from_smt(msg);

	scmi_clear_smt_channel();

	return ret;
}



/** SCMI Functions */
static int scmi_clk_gate(int clkid, int enable)
{
	struct scmi_clk_state_in in = {
		.clock_id = clkid,
		.attributes = enable,
	};
	struct scmi_clk_state_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_CONFIG_SET,
					  in, out);
	int ret;

	ret = scmi_smccc_process_msg(&msg);
	if (ret)
		return ret;

    /* scmi return status is not done*/
    return 0;
}


static uint64_t scmi_clk_get_rate(int clkid)
{
	struct scmi_clk_rate_get_in in = {
		.clock_id = clkid,
	};
	struct scmi_clk_rate_get_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_RATE_GET,
					  in, out);
	int ret;

	ret = scmi_smccc_process_msg(&msg);
	if (ret < 0)
		return ret;

	return (uint64_t)(((uint64_t)out.rate_msb << 32) | out.rate_lsb);
}

static uint64_t scmi_clk_set_rate(int clkid, uint64_t rate)
{
	struct scmi_clk_rate_set_in in = {
		.clock_id = clkid,
		.flags = SCMI_CLK_RATE_ROUND_CLOSEST,
		.rate_lsb = (uint32_t)rate,
		.rate_msb = (uint32_t)((uint64_t)rate >> 32),
	};
	struct scmi_clk_rate_set_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_RATE_SET,
					  in, out);
	int ret;

	ret = scmi_smccc_process_msg(&msg);
	if (ret < 0)
		return ret;

	return scmi_clk_get_rate(clkid);
}




#endif
