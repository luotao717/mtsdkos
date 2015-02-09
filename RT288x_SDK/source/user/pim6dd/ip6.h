/*
 */

#ifndef IP6_H
#define IP6_H

/* Generic extension header.  */
struct ip6_ext
  {
    uint8_t  ip6e_nxt;          /* next header.  */
    uint8_t  ip6e_len;          /* length in units of 8 octets.  */
  };

/* IPv6 options */
struct ip6_opt
  {
    uint8_t  ip6o_type;
    uint8_t  ip6o_len;
  };

/* The high-order 3 bits of the option type define the behavior
 * when processing an unknown option and whether or not the option
 * content changes in flight.
 */
#define IP6OPT_TYPE(o)		((o) & 0xc0)
#define IP6OPT_TYPE_SKIP	0x00
#define IP6OPT_TYPE_DISCARD	0x40
#define IP6OPT_TYPE_FORCEICMP	0x80
#define IP6OPT_TYPE_ICMP	0xc0
#define IP6OPT_TYPE_MUTABLE	0x20

/* Special option types for padding.  */
#define IP6OPT_PAD1	0
#define IP6OPT_PADN	1

#define IP6OPT_JUMBO		0xc2
#define IP6OPT_NSAP_ADDR	0xc3
#define IP6OPT_TUNNEL_LIMIT	0x04
#define IP6OPT_ROUTER_ALERT	0x05

/* Jumbo Payload Option */
struct ip6_opt_jumbo
  {
    uint8_t  ip6oj_type;
    uint8_t  ip6oj_len;
    uint8_t  ip6oj_jumbo_len[4];
  };
#define IP6OPT_JUMBO_LEN	6

/* NSAP Address Option */
struct ip6_opt_nsap
  {
    uint8_t  ip6on_type;
    uint8_t  ip6on_len;
    uint8_t  ip6on_src_nsap_len;
    uint8_t  ip6on_dst_nsap_len;
      /* followed by source NSAP */
      /* followed by destination NSAP */
  };

/* Tunnel Limit Option */
struct ip6_opt_tunnel
  {
    uint8_t  ip6ot_type;
    uint8_t  ip6ot_len;
    uint8_t  ip6ot_encap_limit;
  };

/* Router Alert Option */
struct ip6_opt_router
  {
    uint8_t  ip6or_type;
    uint8_t  ip6or_len;
    uint8_t  ip6or_value[2];
  };

/* Router alert values (in network byte order) */
#if BYTE_ORDER == BIG_ENDIAN
# define IP6_ALERT_MLD	0x0000
# define IP6_ALERT_RSVP	0x0001
# define IP6_ALERT_AN	0x0002
#else /* BYTE_ORDER == LITTLE_ENDING */
# define IP6_ALERT_MLD	0x0000
# define IP6_ALERT_RSVP	0x0100
# define IP6_ALERT_AN	0x0200
#endif



#endif
