#include <linux/mm.h>
#include <linux/module.h>
#include <net/tcp.h>
#include <linux/time.h>

const u32 update_interval = 2;
const u32 weight_old = 80;
const u32 weight_new = 20;
const u32 fast_inc_step = 2;
const u32 slow_inc_step = 10;

struct custom {
	u32 artt;
	u32 pkts_acked;
	u64 m_s;
	u32 ccwnd;
	u32 ccwnd_prio;
	u32 ccwnd_cnt;
	u8 fast_inc;
};

static void tcp_custom_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct custom *ca = inet_csk_ca(sk);
	
	ca->artt = 100000;
	ca->pkts_acked = 0;
	ca->m_s = 0;
	ca->ccwnd = 128;
	ca->ccwnd_prio = 128;
	ca->ccwnd_cnt = 0;
	ca->fast_inc = 1;
	tp->snd_cwnd = ca->ccwnd;
}

static void tcp_custom_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
}

static void tcp_custom_acked(struct sock *sk, const struct ack_sample *sample)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct custom *ca = inet_csk_ca(sk);
	s32 rtt_us = sample->rtt_us;	
	struct timespec ct = current_kernel_time();
	if (ca->m_s == 0) ca->m_s = ct.tv_sec;
	
	if (ct.tv_sec - ca->m_s > update_interval) {
		ca->ccwnd_prio = ca->ccwnd;
		ca->ccwnd = (weight_new * ca->pkts_acked
					* ca->artt / 1000000 / update_interval
					+ weight_old * ca->ccwnd) * 112 / 10000;
		if (ca->ccwnd * 10 > ca->ccwnd_prio * 12)
			ca->fast_inc = 1;
		else
			ca->fast_inc = 0;
		ca->m_s = ct.tv_sec;
		ca->pkts_acked = 0;
	}
	
	ca->pkts_acked += sample->pkts_acked;
	if (rtt_us <= 0) rtt_us = ca->artt;
	
	if (ca->artt == 100000)
		ca->artt = rtt_us;
	else if (rtt_us < ca->artt)
		ca->artt = rtt_us;
	else
		ca->artt = ca->artt * 9 / 10 + rtt_us * 1 / 10;
	
	if (rtt_us > ca->artt && ca->artt < ca->artt * 9 / 8) {
		if (ca->fast_inc)
			ca->ccwnd += fast_inc_step;
		else {
			ca->ccwnd_cnt += sample->pkts_acked * slow_inc_step;
			while (ca->ccwnd_cnt > ca->ccwnd) {
				ca->ccwnd_cnt -= ca->ccwnd;
				ca->ccwnd++;
			}
		}
	}
	else if (rtt_us > ca->artt * 2) {
		ca->ccwnd -= 2;
	}
	tp->snd_cwnd = ca->ccwnd;
}

static u32 tcp_custom_ssthresh(struct sock *sk)
{
	struct custom *ca = inet_csk_ca(sk);
	return ca->ccwnd;
}

static void tcp_custom_state(struct sock *sk, u8 new_state)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct custom *ca = inet_csk_ca(sk);
	if (new_state == TCP_CA_Loss)
		tp->snd_cwnd = ca->ccwnd;
}

static void tcp_custom_cwnd_event(struct sock *sk, enum tcp_ca_event ev)
{
}

static u32 tcp_custom_undo_cwnd(struct sock *sk)
{
	struct custom *ca = inet_csk_ca(sk);
	return ca->ccwnd;
}

static void tcp_custom_cong_control(struct sock *sk,
									const struct rate_sample *rs)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct custom *ca = inet_csk_ca(sk);
	tp->snd_cwnd = ca->ccwnd;
}

static struct tcp_congestion_ops tcp_custom = {
	.init			= tcp_custom_init,
	.ssthresh		= tcp_custom_ssthresh,
	.undo_cwnd		= tcp_custom_undo_cwnd,
	.cong_avoid		= tcp_custom_cong_avoid,
	.set_state		= tcp_custom_state,
	.pkts_acked 	= tcp_custom_acked,
	.cwnd_event 	= tcp_custom_cwnd_event,
	.cong_control	= tcp_custom_cong_control,
	.owner			= THIS_MODULE,
	.name			= "ccustom",
};

static int __init tcp_custom_register(void)
{
	BUILD_BUG_ON(sizeof(struct custom) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_custom);
}

static void __exit tcp_custom_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_custom);
}

module_init(tcp_custom_register);
module_exit(tcp_custom_unregister);

MODULE_AUTHOR("Reinforce-II");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP CCustom");
