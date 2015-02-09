#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <linux/version.h>
#include <getopt.h>

#include "linux/autoconf.h"  //kernel config
#include "config/autoconf.h" //user config

#define u32	unsigned int

unsigned int rareg(int mode, unsigned int addr, long long int new_value);

#define READMODE        0x0
#define WRITEMODE       0x1
#define WRITE_DELAY     100                     /* ms */

#define TCH_ARB_BASE			0x10000308
#define DMA_ARB_BASE			0x10000400

#define ARB_CFG_OFFSET			(0x0UL)
#define ARB_BW_OFFSET			(0x4UL)

#define TCH_AG_NUM_CPU			0
#define TCH_AG_NUM_DMA			1

#define DMA_AG_NUM_UTIF			0
#define DMA_AG_NUM_GDMA			1
#define DMA_AG_NUM_PPE			2
#define DMA_AG_NUM_GSW_PDMA		3
#define DMA_AG_NUM_WPDMA		4
#define DMA_AG_NUM_PCIe			5
#define DMA_AG_NUM_USB			6
#define DMA_AG_NUM_CRYPTO_ENGINE	7

#define TCH_ARB_QOS_UNIT		8	/* Mbits */
#define DMA_ARB_QOS_UNIT		4	/* Mbits */

#define PREEMPTION_BIT			26
#define TRTC_BIT			25
#define CLASS_BIT			24
#define CLSPRIORITY_BIT			0
#define CLSPRIORITY_MASK		0x00ffffff

#define GETPREEMPTION(x)	((x & (0x1UL << PREEMPTION_BIT)  ) >> PREEMPTION_BIT)
#define GETTRTC(x)		((x & (0x1UL << TRTC_BIT)  ) >> TRTC_BIT)
#define GETCLASS(x)		((x & (0x1UL << CLASS_BIT)  ) >> CLASS_BIT)
#define GETCLSPRIORITY(x)	(((x & (CLSPRIORITY_MASK << CLSPRIORITY_BIT)) >> CLSPRIORITY_BIT) & CLSPRIORITY_MASK )

#define QOSTYPE_BIT			24
#define QOSTYPE_MASK			0x03
#define DUEDAT_BIT			16
#define DUEDAT_MASK			0xff
#define EIR_BIT				8
#define EIR_MASK			0xff
#define CIR_BIT				0
#define CIR_MASK			0xff

#define GETQOSTYPE(x)		((x & (QOSTYPE_MASK << QOSTYPE_BIT)  ) >> QOSTYPE_BIT)
#define GETDUEDAT(x)		((x & (DUEDAT_MASK << DUEDAT_BIT)  ) >> DUEDAT_BIT)
#define GETEIR(x)		((x & (EIR_MASK << EIR_BIT)  ) >> EIR_BIT)
#define GETCIR(x)		((x & (CIR_MASK << CIR_BIT)  ) >> CIR_BIT)

/* enum */
#define QOSTYPEFIELD			0
#define DUEDATFIELD			1
#define EIRFIELD			2
#define CIRFIELD			3

/* enum */
#define PREEMPTIONFIELD			0
#define TRTCFIELD			1
#define CLASSFIELD			2
#define CLSPRIOFIELD			3

const char *qostype[] = {
"Latency Critical(0)",
"Latency Sensitive(1)",
"Bandwidth Sensitive(2)",
"Best Effort(3)",
};

const char *dmatype[] = {
"UTIF", 
"GDMA",
"PPE",
"GSW PDMA",
"WPDMA",
"PCIe",
"USB",
"Crypto Engine",
};

void dump_qos_bw(u32 pos, unsigned long mode)
{
	u32 nop = 0, cur;
	u32 base = pos;
	u32 bw_unit = (pos == TCH_ARB_BASE) ? TCH_ARB_QOS_UNIT : DMA_ARB_QOS_UNIT;

	pos = pos + ARB_BW_OFFSET;

	cur = (mode & 0xf) << 28;
	rareg(WRITEMODE, pos, cur);

	usleep(100 * 1000);

	cur = rareg(READMODE, pos, nop);

	// display result
	printf("\t= %s channel =\n", (base == TCH_ARB_BASE) ? ( mode == TCH_AG_NUM_CPU ? "CPU" : "DMA" ) : dmatype[mode]);
	printf("\tQoS Type:\t%s\n",  qostype[GETQOSTYPE(cur)]);
	printf("\tDueDat:\t\t%d\n", GETDUEDAT(cur));
	printf("\tEIR/CIR:\t%d/%d Mbits\n", GETEIR(cur) * bw_unit,  GETCIR(cur) * bw_unit);
	printf("\n");
}


void dump_cfg(u32 pos)
{
	pos = pos + ARB_CFG_OFFSET;
	u32 nop = 0, cfg = rareg(READMODE, pos, nop);

	printf("Preemption:\t\t%s\n", GETPREEMPTION(cfg) ? "Yes" : "No");
	printf("TRTC:\t\t\t%s\n", GETTRTC(cfg) ? "Yes" : "No");
	printf("QoS Classification:\t%s\n", GETCLASS(cfg) ? "Yes" : "No");
	printf("Class Priority:\t\t0x%08X\n", GETCLSPRIORITY(cfg));
}


void set_cfg(u32 pos, int field, u32 en)
{
	u32 base = pos;
	pos = pos + ARB_CFG_OFFSET;
	u32 nop = 0, cur = rareg(READMODE, pos, nop);
	if(field == PREEMPTIONFIELD)
		cur = en ? cur | ((0x1UL) << PREEMPTION_BIT) : cur & ~((0x1UL) << PREEMPTION_BIT);
	else if(field == TRTCFIELD)
		cur = en ? cur | ((0x1UL) << TRTC_BIT) : cur & ~((0x1UL) << TRTC_BIT);
	else if(field == CLASSFIELD)
		cur = en ? cur | ((0x1UL) << CLASS_BIT) : cur & ~((0x1UL) << CLASS_BIT);
	else if(field == CLSPRIOFIELD)
		cur = (cur & (~CLSPRIORITY_MASK)) | en;
	else
		exit(-1);
	rareg(WRITEMODE, pos, cur);
	dump_cfg(base);
}

/* AG  */
void set_qos(u32 pos, int num, int field, u32 value)
{
	u32 base = pos;
	pos = pos + ARB_BW_OFFSET;
	u32 nop = 0, cur = 0; //rareg(READMODE, pos, nop);

        cur = (num & 0x7) << 28;

        rareg(WRITEMODE, pos, cur);	// read the qos info from window
        usleep(100 * 1000);
        cur = rareg(READMODE, pos, nop);

//	printf("1. cur= 0x%08x\n", cur);
	cur = cur | 0x80000000;

	if(field == QOSTYPEFIELD)
		cur = (cur & ~(QOSTYPE_MASK << QOSTYPE_BIT)) | ( (value & QOSTYPE_MASK) << QOSTYPE_BIT);
	else if(field == DUEDATFIELD)
		cur = (cur & ~(DUEDAT_MASK << DUEDAT_BIT)) | ( (value & DUEDAT_MASK) << DUEDAT_BIT);
	else if(field == EIRFIELD)
		cur = (cur & ~(EIR_MASK << EIR_BIT)) | ( (value & EIR_MASK) << EIR_BIT);
	else if(field == CIRFIELD)
		cur = (cur & ~(CIR_MASK << CIR_BIT)) | ( (value & CIR_MASK) << CIR_BIT);
	else
		exit(-1);

//	printf("2. cur= 0x%08x\n", cur);
	rareg(WRITEMODE, pos, cur);

	dump_qos_bw(base, num);
}

void dump_all(void)
{
	int i;
	printf("======== 2CH Arbiter Config ========\n");
	dump_cfg(TCH_ARB_BASE);
	printf("\n");

	dump_qos_bw(TCH_ARB_BASE, TCH_AG_NUM_CPU);
	printf("\n");

	dump_qos_bw(TCH_ARB_BASE, TCH_AG_NUM_DMA);
	printf("\n");

	printf("======== DMA Arbiter Config ========\n");
	dump_cfg(DMA_ARB_BASE);
	printf("\n");

	for(i=0; i< sizeof(dmatype)/sizeof(char *); i++){
		dump_qos_bw(DMA_ARB_BASE, i);
	}
}


void usage()
{

#if 0
/*

====================================================
Global settings:

[-m 2ch|peri]	assiged the channel in global
[-p 1|0]	change Preemption
[-t 1|0]	change TRTC
[-c 1|0]	change class
[-d]		display all infomation

ex1:
disable the TRTC on 2ch
$ busarb -m 2ch -p 0

ex2:
enable the Classfication on peripheral
$ busarb -m peri -c 1

ex3:
dump all thing
$ busarb -d

=====================================================
Channel QoS settings:

[-n location]	assign the channel
-n cpu		CPU channel(at 2ch position)
-n dma		DMA channel(at 2ch position)
-n utif		UTFI channel(at peripheral position)
-n gdma		GDMA channel(at peripheral position)
-n ppe		PPE channel(at peripheral position)
-n gswpdma	GSW PDMA channel(at peripheral position)
-n wpdma	WPDMA channel(at peripheral position)
-n pcie		PCIE channel(at peripheral position)
-n usb		USB channel(at peripheral position)
-n crypto	Crypto Engine channel(at peripheral position)

[-Q qos_type]	change the qos type
-Q LC		Latency Critical
-Q LS		Latency Sensitive
-Q BS		Bandwidth Sensitive
-Q BE		Best Effort

[-U duedat]	change the due date
[-E eir]	change the EIR
[-C cir]	change the CIR


ex1:
change the qos type to "Best Effort" of USB channel
$ busarb -n usb -Q BE

ex2:
change the EIR of GDMA channel
$ busarb -n gdma -E 10

ex3:
change the DueDat of CPU channel
$ busarb -n cpu -U 50
*/
#endif
}

extern int optind;

int main(int argc, char *argv[])
{
	int opt;
	u32 base = TCH_ARB_BASE;
	u32 value, num = 0;			/* CPU or UTIF by default */
	char options[] = "m:p:t:c:r:Q:n:U:E:C:d";

	/* get mode first */
	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
			case 'm':
				if(!strcmp(optarg, "0")){
					base = TCH_ARB_BASE;
				}else if(!strcmp(optarg, "1")){
					base = DMA_ARB_BASE;
				}else if(!strcasecmp(optarg, "2ch")){
					base = TCH_ARB_BASE;
				}else if(!strcasecmp(optarg, "peri")){
					base = DMA_ARB_BASE;
				}else if(!strcasecmp(optarg, "dma")){
					base = DMA_ARB_BASE;
				}else{
					usage();
					exit(-1);
				}
                                break;
		}
	}

	/* then get the num */
	optind = 1;
	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
                        case 'n':
				if(optarg[0] > '0' && optarg[0] < '9'){
					num = atoi(optarg);
					if(num > 8){
						usage();
						exit(-1);
					}
				}else{
					if(!strcasecmp(optarg, "cpu")){
						base = TCH_ARB_BASE;
						num = 0;
					}else if(!strcasecmp(optarg, "dma")){
						base = TCH_ARB_BASE;
						num = 1;
					}else if(!strcasecmp(optarg, "utif")){
						base = DMA_ARB_BASE;
						num = 0;
					}else if(!strcasecmp(optarg, "gdma")){
						base = DMA_ARB_BASE;
						num = 1;
					}else if(!strcasecmp(optarg, "ppe")){
						base = DMA_ARB_BASE;
						num = 2;
					}else if(!strcasecmp(optarg, "gswpdma")){
						base = DMA_ARB_BASE;
						num = 3;
					}else if(!strcasecmp(optarg, "wpdma")){
						base = DMA_ARB_BASE;
						num = 4;
					}else if(!strcasecmp(optarg, "pcie")){
						base = DMA_ARB_BASE;
						num = 5;
					}else if(!strcasecmp(optarg, "usb")){
						base = DMA_ARB_BASE;
						num = 6;
					}else if(!strcasecmp(optarg, "crypto")){
						base = DMA_ARB_BASE;
						num = 7;
					}else{
						usage();
						exit(-1);
					}
				}
				break;
		}
	}

	/* reset the getopt() */
	optind = 1;
	while ((opt = getopt(argc, argv, options)) != -1) {
		switch (opt) {
			case 'p':
				set_cfg(base, PREEMPTIONFIELD, atoi(optarg));
                                break;
			case 't':
				set_cfg(base, TRTCFIELD, atoi(optarg));
                                break;
			case 'c':
				set_cfg(base, CLASSFIELD, atoi(optarg));
				break;
			case 'r':
				set_cfg(base, CLSPRIOFIELD, atoi(optarg));
				break;
			case 'd':
				dump_all();
				break;
			case 'Q':
				if(!strcasecmp(optarg, "LC"))
					value = 0;
				else if(!strcasecmp(optarg, "LS"))
					value = 1;
				else if(!strcasecmp(optarg, "BS"))
					value = 2;
				else if(!strcasecmp(optarg, "BE"))
					value = 3;
				else
					value = atoi(optarg);
				set_qos(base, num, QOSTYPEFIELD, value);
				break;
			case 'U':
				set_qos(base, num, DUEDATFIELD, atoi(optarg));
				break;
			case 'E':
				set_qos(base, num, EIRFIELD, atoi(optarg));
				break;
			case 'C':
				set_qos(base, num, CIRFIELD, atoi(optarg));
				break;
			
			case 'm':
			case 'n':
				continue;
                        default:
                                usage();
                                return 0;
                }
        }
        return 0;
}
