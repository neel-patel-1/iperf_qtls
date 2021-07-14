/*---------------------------------------------------------------
 * Copyright (c) 2020
 * Broadcom Corporation
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the name of Broadcom Coporation,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 *
 * reporter output routines
 *
 * by Robert J. McMahon (rjmcmahon@rjmcmahon.com, bob.mcmahon@broadcom.com)
 * -------------------------------------------------------------------
 */
#include <math.h>
#include "headers.h"
#include "Settings.hpp"
#include "Reporter.h"
#include "Locale.h"
#include "SocketAddr.h"

// These static variables are not thread safe but ok to use becase only
// the repoter thread usses them
#define SNBUFFERSIZE 256
#define SNBUFFEREXTENDSIZE 512
static char outbuffer[SNBUFFERSIZE]; // Buffer for printing
static char outbufferext[SNBUFFEREXTENDSIZE]; // Buffer for printing
static char outbufferext2[SNBUFFEREXTENDSIZE]; // Buffer for printing
static char llaw_buf[100];
static char netpower_buf[100];

static int HEADING_FLAG(report_bw) = 0;
static int HEADING_FLAG(report_bw_jitter_loss) = 0;
static int HEADING_FLAG(report_bw_read_enhanced) = 0;
static int HEADING_FLAG(report_bw_read_enhanced_netpwr) = 0;
static int HEADING_FLAG(report_bw_write_enhanced) = 0;
static int HEADING_FLAG(report_bw_write_enhanced_netpwr) = 0;
static int HEADING_FLAG(report_bw_pps_enhanced) = 0;
static int HEADING_FLAG(report_bw_pps_enhanced_isoch) = 0;
static int HEADING_FLAG(report_bw_jitter_loss_pps) = 0;
static int HEADING_FLAG(report_bw_jitter_loss_enhanced) = 0;
static int HEADING_FLAG(report_bw_jitter_loss_enhanced_isoch) = 0;
static int HEADING_FLAG(report_write_enhanced_isoch) = 0;
static int HEADING_FLAG(report_frame_jitter_loss_enhanced) = 0;
static int HEADING_FLAG(report_frame_tcp_enhanced) = 0;
static int HEADING_FLAG(report_frame_read_tcp_enhanced_triptime) = 0;
static int HEADING_FLAG(report_udp_fullduplex) = 0;
static int HEADING_FLAG(report_sumcnt_bw) = 0;
static int HEADING_FLAG(report_sumcnt_udp_fullduplex) = 0;
static int HEADING_FLAG(report_sumcnt_bw_read_enhanced) = 0;
static int HEADING_FLAG(report_sumcnt_bw_write_enhanced) = 0;
static int HEADING_FLAG(report_sumcnt_bw_pps_enhanced) = 0;
static int HEADING_FLAG(report_bw_jitter_loss_enhanced_triptime) = 0;
static int HEADING_FLAG(report_bw_jitter_loss_enhanced_isoch_triptime) = 0;
static int HEADING_FLAG(report_sumcnt_bw_jitter_loss) = 0;
static int HEADING_FLAG(report_burst_read_tcp) = 0;

void reporter_default_heading_flags (int flag) {
    HEADING_FLAG(report_bw) = flag;
    HEADING_FLAG(report_sumcnt_bw) = flag;
    HEADING_FLAG(report_sumcnt_udp_fullduplex) = flag;
    HEADING_FLAG(report_bw_jitter_loss) = flag;
    HEADING_FLAG(report_bw_read_enhanced) = flag;
    HEADING_FLAG(report_bw_read_enhanced_netpwr) = flag;
    HEADING_FLAG(report_bw_write_enhanced) = flag;
    HEADING_FLAG(report_write_enhanced_isoch) = flag;
    HEADING_FLAG(report_bw_write_enhanced_netpwr) = flag;
    HEADING_FLAG(report_bw_pps_enhanced) = flag;
    HEADING_FLAG(report_bw_pps_enhanced_isoch) = flag;
    HEADING_FLAG(report_bw_jitter_loss_pps) = flag;
    HEADING_FLAG(report_bw_jitter_loss_enhanced) = flag;
    HEADING_FLAG(report_bw_jitter_loss_enhanced_isoch) = flag;
    HEADING_FLAG(report_frame_jitter_loss_enhanced) = flag;
    HEADING_FLAG(report_frame_tcp_enhanced) = flag;
    HEADING_FLAG(report_frame_read_tcp_enhanced_triptime) = flag;
    HEADING_FLAG(report_sumcnt_bw_read_enhanced) = flag;
    HEADING_FLAG(report_sumcnt_bw_write_enhanced) = flag;
    HEADING_FLAG(report_udp_fullduplex) = flag;
    HEADING_FLAG(report_sumcnt_bw_jitter_loss) = flag;
    HEADING_FLAG(report_sumcnt_bw_pps_enhanced) = flag;
    HEADING_FLAG(report_burst_read_tcp) = flag;
}
static inline void _print_stats_common (struct TransferInfo *stats) {
    assert(stats!=NULL);
    outbuffer[0] = '\0';
    outbufferext[0] = '\0';
    byte_snprintf(outbuffer, sizeof(outbuffer), (double) stats->cntBytes, toupper((int)stats->common->Format));
    if (stats->ts.iEnd < SMALLEST_INTERVAL_SEC) {
        stats->cntBytes = 0;
    }
    byte_snprintf(outbufferext, sizeof(outbufferext), (double)stats->cntBytes / (stats->ts.iEnd - stats->ts.iStart), \
		  stats->common->Format);
    outbuffer[sizeof(outbuffer)-1]='\0';
    outbufferext[sizeof(outbufferext)-1]='\0';
}

static inline void _output_outoforder(struct TransferInfo *stats) {
    if (stats->cntOutofOrder > 0) {
	printf(report_outoforder,
	       stats->common->transferIDStr, stats->ts.iStart,
	       stats->ts.iEnd, stats->cntOutofOrder);
    }
    if (stats->l2counts.cnt) {
	printf(report_l2statistics,
	       stats->common->transferIDStr, stats->ts.iStart,
	       stats->ts.iEnd, stats->l2counts.cnt, stats->l2counts.lengtherr,
	       stats->l2counts.udpcsumerr, stats->l2counts.unknown);
    }
}

//
//  Little's law is L = lambda * W, where L is queue depth,
//  lambda the arrival rate and W is the processing time
//
#define LLAW_LOWERBOUNDS -1e7
static inline void set_llawbuf(double lambda, double meantransit, struct TransferInfo *stats) {
    double L  = lambda * meantransit;
    if (L < LLAW_LOWERBOUNDS) {
	strcpy(llaw_buf, "OBL");
    } else {
        //force to adpative bytes for human readable
        byte_snprintf(llaw_buf, sizeof(llaw_buf), L, 'A');
	llaw_buf[sizeof(llaw_buf)-1] = '\0';
    }
}
static inline void set_llawbuf_udp (int lambda, double meantransit, double variance, struct TransferInfo *stats) {
    int Lvar = 0;
    int L  = round(lambda * meantransit);
    if (variance > 0.0) {
	Lvar  = round(lambda * variance);
    } else {
	Lvar = 0;
    }
    snprintf(llaw_buf, sizeof(llaw_buf), "%" PRIdMAX "/%d(%d) pkts", stats->cntIPG, L, Lvar);
    llaw_buf[sizeof(llaw_buf) - 1] = '\0';
}

#define NETPWR_LOWERBOUNDS -1e7
static inline void set_netpowerbuf(double meantransit, struct TransferInfo *stats) {
  if (meantransit == 0.0) {
      strcpy(netpower_buf, "NAN");
  } else {
      double netpwr = (NETPOWERCONSTANT * ((double) stats->cntBytes) / (stats->ts.iEnd - stats->ts.iStart) / meantransit);
      if (netpwr <  NETPWR_LOWERBOUNDS) {
	  strcpy(netpower_buf, "OBL");
      } else {
	  snprintf(netpower_buf, sizeof(netpower_buf), "%.0f", netpwr);
      }
  }
}

//TCP Output
void tcp_output_fullduplex (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_bw_sum_fullduplex_format, stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext);
    fflush(stdout);
}

void tcp_output_fullduplex_sum (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_sum_bw_format, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext);
    fflush(stdout);
}

void tcp_output_fullduplex_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_bw_sum_fullduplex_enhanced_format, stats->common->transferID, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext);
    fflush(stdout);
}

void tcp_output_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_bw_format, stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext);
    fflush(stdout);
}
//TCP read or server output
void tcp_output_read_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_read_enhanced);
    _print_stats_common(stats);
    printf(report_bw_read_enhanced_format,
	   stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.read.cntRead,
	   stats->sock_callstats.read.bins[0],
	   stats->sock_callstats.read.bins[1],
	   stats->sock_callstats.read.bins[2],
	   stats->sock_callstats.read.bins[3],
	   stats->sock_callstats.read.bins[4],
	   stats->sock_callstats.read.bins[5],
	   stats->sock_callstats.read.bins[6],
	   stats->sock_callstats.read.bins[7]);
    fflush(stdout);
}
void tcp_output_read_enhanced_triptime (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_read_enhanced_netpwr);
    double meantransit = (stats->transit.cntTransit > 0) ? (stats->transit.sumTransit / stats->transit.cntTransit) : 0;
    double lambda = (stats->IPGsum > 0.0) ? ((double)stats->cntBytes / stats->IPGsum) : 0.0;
    set_llawbuf(lambda, meantransit, stats);
    _print_stats_common(stats);
    if (stats->cntBytes) {
        set_netpowerbuf(meantransit, stats);
	printf(report_bw_read_enhanced_netpwr_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       (meantransit * 1e3),
	       stats->transit.minTransit*1e3,
	       stats->transit.maxTransit*1e3,
	       (stats->transit.cntTransit < 2) ? 0 : sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e3,
	       stats->transit.cntTransit,
	       stats->transit.cntTransit ? (long) ((double)stats->cntBytes / (double) stats->transit.cntTransit) : 0,
	       llaw_buf,
	       netpower_buf,
	       stats->sock_callstats.read.cntRead,
	       stats->sock_callstats.read.bins[0],
	       stats->sock_callstats.read.bins[1],
	       stats->sock_callstats.read.bins[2],
	       stats->sock_callstats.read.bins[3],
	       stats->sock_callstats.read.bins[4],
	       stats->sock_callstats.read.bins[5],
	       stats->sock_callstats.read.bins[6],
	       stats->sock_callstats.read.bins[7]);
    } else {
        set_netpowerbuf(meantransit, stats);
	printf(report_bw_read_enhanced_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       (meantransit * 1e3),
	       stats->transit.minTransit*1e3,
	       stats->transit.maxTransit*1e3,
	       (stats->transit.cntTransit < 2) ? 0 : sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e3,
	       stats->transit.cntTransit,
	       stats->transit.cntTransit ? (long) ((double)stats->cntBytes / (double) stats->transit.cntTransit) : 0,
	       llaw_buf,
	       netpower_buf,
	       stats->sock_callstats.read.cntRead,
	       stats->sock_callstats.read.bins[0],
	       stats->sock_callstats.read.bins[1],
	       stats->sock_callstats.read.bins[2],
	       stats->sock_callstats.read.bins[3],
	       stats->sock_callstats.read.bins[4],
	       stats->sock_callstats.read.bins[5],
	       stats->sock_callstats.read.bins[6],
	       stats->sock_callstats.read.bins[7]);
    }
    if (stats->framelatency_histogram) {
	histogram_print(stats->framelatency_histogram, stats->ts.iStart, stats->ts.iEnd);
    }
    fflush(stdout);
}
void tcp_output_frame_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_frame_tcp_enhanced);
    _print_stats_common(stats);
    printf(report_bw_read_enhanced_format,
	   stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.read.cntRead,
	   stats->sock_callstats.read.bins[0],
	   stats->sock_callstats.read.bins[1],
	   stats->sock_callstats.read.bins[2],
	   stats->sock_callstats.read.bins[3],
	   stats->sock_callstats.read.bins[4],
	   stats->sock_callstats.read.bins[5],
	   stats->sock_callstats.read.bins[6],
	   stats->sock_callstats.read.bins[7]);
    fflush(stdout);
}
void tcp_output_frame_read_triptime (struct TransferInfo *stats) {
    fprintf(stderr, "FIXME\n");
}
void tcp_output_burst_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_burst_read_tcp);
    _print_stats_common(stats);
    if (!stats->final) {
	set_netpowerbuf(stats->tripTime, stats);
	printf(report_burst_read_tcp_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       stats->tripTime,
	       stats->sock_callstats.read.cntRead,
	       stats->sock_callstats.read.bins[0],
	       stats->sock_callstats.read.bins[1],
	       stats->sock_callstats.read.bins[2],
	       stats->sock_callstats.read.bins[3],
	       stats->sock_callstats.read.bins[4],
	       stats->sock_callstats.read.bins[5],
	       stats->sock_callstats.read.bins[6],
	       stats->sock_callstats.read.bins[7],
	       (stats->tripTime * stats->common->FPS) / 10.0, // (1e3 / 100%)
	       netpower_buf);
    } else {
	printf(report_burst_read_tcp_final_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       stats->sock_callstats.read.cntRead,
	       stats->sock_callstats.read.bins[0],
	       stats->sock_callstats.read.bins[1],
	       stats->sock_callstats.read.bins[2],
	       stats->sock_callstats.read.bins[3],
	       stats->sock_callstats.read.bins[4],
	       stats->sock_callstats.read.bins[5],
	       stats->sock_callstats.read.bins[6],
	       stats->sock_callstats.read.bins[7]);
    }
    fflush(stdout);
}

//TCP write or client output
void tcp_output_write (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_bw_format, stats->common->transferIDStr,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}

void tcp_output_write_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_write_enhanced);
    _print_stats_common(stats);
#ifndef HAVE_STRUCT_TCP_INFO_TCPI_TOTAL_RETRANS
    printf(report_bw_write_enhanced_format,
	   stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.write.WriteCnt,
	   stats->sock_callstats.write.WriteErr);
#else
    set_netpowerbuf(stats->sock_callstats.write.rtt * 1e-6, stats);
    if (stats->sock_callstats.write.cwnd > 0) {
	printf(report_bw_write_enhanced_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       stats->sock_callstats.write.WriteCnt,
	       stats->sock_callstats.write.WriteErr,
	       stats->sock_callstats.write.TCPretry,
	       stats->sock_callstats.write.cwnd,
	       stats->sock_callstats.write.rtt,
	       netpower_buf);
    } else {
	printf(report_bw_write_enhanced_nocwnd_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       stats->sock_callstats.write.WriteCnt,
	       stats->sock_callstats.write.WriteErr,
	       stats->sock_callstats.write.TCPretry,
	       stats->sock_callstats.write.rtt,
	       netpower_buf);
    }
#endif
    fflush(stdout);
}

void tcp_output_write_enhanced_isoch (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_write_enhanced_isoch);
    _print_stats_common(stats);
#ifndef HAVE_STRUCT_TCP_INFO_TCPI_TOTAL_RETRANS
    printf(report_write_enhanced_isoch_format,
	   stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.write.WriteCnt,
	   stats->sock_callstats.write.WriteErr,
	   stats->isochstats.cntFrames, stats->isochstats.cntFramesMissed, stats->isochstats.cntSlips);
#else
    set_netpowerbuf(stats->sock_callstats.write.rtt * 1e-6, stats);
    if (stats->sock_callstats.write.cwnd > 0) {
	printf(report_write_enhanced_isoch_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       stats->sock_callstats.write.WriteCnt,
	       stats->sock_callstats.write.WriteErr,
	       stats->sock_callstats.write.TCPretry,
	       stats->sock_callstats.write.cwnd,
	       stats->sock_callstats.write.rtt,
	       stats->isochstats.cntFrames, stats->isochstats.cntFramesMissed, stats->isochstats.cntSlips,
	       netpower_buf);
    } else {
	printf(report_write_enhanced_isoch_nocwnd_format,
	       stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       stats->sock_callstats.write.WriteCnt,
	       stats->sock_callstats.write.WriteErr,
	       stats->sock_callstats.write.TCPretry,
	       stats->sock_callstats.write.rtt,
	       stats->isochstats.cntFrames, stats->isochstats.cntFramesMissed, stats->isochstats.cntSlips,
	       netpower_buf);
    }
#endif
    fflush(stdout);
}


//UDP output
void udp_output_fullduplex (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_udp_fullduplex);
    _print_stats_common(stats);
    printf(report_udp_fullduplex_format, stats->common->transferIDStr, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext, \
	   stats->cntDatagrams, (stats->cntIPG && (stats->IPGsum > 0.0) ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}

void udp_output_fullduplex_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_udp_fullduplex);
    _print_stats_common(stats);
    printf(report_udp_fullduplex_enhanced_format, stats->common->transferID, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext, \
	   stats->cntDatagrams, (stats->cntIPG && (stats->IPGsum > 0.0) ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}

void udp_output_fullduplex_sum (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_udp_fullduplex);
    _print_stats_common(stats);
    printf(report_udp_fullduplex_sum_format, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext, \
	   stats->cntDatagrams, (stats->cntIPG && (stats->IPGsum > 0.0) ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}


void udp_output_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_jitter_loss);
    _print_stats_common(stats);
    printf(report_bw_jitter_loss_format, stats->common->transferIDStr,
	    stats->ts.iStart, stats->ts.iEnd,
	    outbuffer, outbufferext,
	    stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
	    (100.0 * stats->cntError) / stats->cntDatagrams);
    _output_outoforder(stats);
    fflush(stdout);
}

void udp_output_read_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_jitter_loss_enhanced);
    _print_stats_common(stats);
    if (!stats->cntIPG) {
	printf(report_bw_jitter_loss_suppress_enhanced_format, stats->common->transferIDStr,
	       stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       0.0, stats->cntError,
	       stats->cntDatagrams,
	       0.0,0.0,0.0,0.0,0.0,0.0);
    } else {
	if ((stats->transit.minTransit > UNREALISTIC_LATENCYMINMAX) ||
	    (stats->transit.minTransit < UNREALISTIC_LATENCYMINMIN)) {
	    printf(report_bw_jitter_loss_suppress_enhanced_format, stats->common->transferIDStr,
		   stats->ts.iStart, stats->ts.iEnd,
		   outbuffer, outbufferext,
		   stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
		   (100.0 * stats->cntError) / stats->cntDatagrams,
		   (stats->cntIPG / stats->IPGsum));
	} else {
	    double meantransit = (stats->transit.cntTransit > 0) ? (stats->transit.sumTransit / stats->transit.cntTransit) : 0;
	    set_netpowerbuf(meantransit, stats);
	    printf(report_bw_jitter_loss_enhanced_format, stats->common->transferIDStr,
		   stats->ts.iStart, stats->ts.iEnd,
		   outbuffer, outbufferext,
		   stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
		   (100.0 * stats->cntError) / stats->cntDatagrams,
		   (meantransit * 1e3),
		   stats->transit.minTransit*1e3,
		   stats->transit.maxTransit*1e3,
		   (stats->transit.cntTransit < 2) ? 0 : sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e3,
		   (stats->cntIPG / stats->IPGsum),
		   netpower_buf);
	}
    }
    if (stats->latency_histogram) {
	histogram_print(stats->latency_histogram, stats->ts.iStart, stats->ts.iEnd);
    }
    _output_outoforder(stats);
    fflush(stdout);
}

void udp_output_read_enhanced_triptime (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_jitter_loss_enhanced_triptime);
    _print_stats_common(stats);
    if (!stats->cntIPG) {
	printf(report_bw_jitter_loss_suppress_enhanced_format, stats->common->transferIDStr,
	       stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       0.0, stats->cntError,
	       stats->cntDatagrams,
	       0.0,0.0,0.0,0.0,0.0,0.0);
    } else {
	if ((stats->transit.minTransit > UNREALISTIC_LATENCYMINMAX) ||
	    (stats->transit.minTransit < UNREALISTIC_LATENCYMINMIN)) {
	    printf(report_bw_jitter_loss_suppress_enhanced_format, stats->common->transferIDStr,
		   stats->ts.iStart, stats->ts.iEnd,
		   outbuffer, outbufferext,
		   stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
		   (100.0 * stats->cntError) / stats->cntDatagrams,
		   (stats->cntIPG / stats->IPGsum));
	} else {
	    double meantransit = (stats->transit.cntTransit > 0) ? (stats->transit.sumTransit / stats->transit.cntTransit) : 0;
	    int lambda =  ((stats->IPGsum > 0.0) ? (round (stats->cntIPG / stats->IPGsum)) : 0.0);
	    double variance = (stats->transit.cntTransit < 2) ? 0 : (sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e6);
	    set_llawbuf_udp(lambda, meantransit, variance, stats);
	    set_netpowerbuf(meantransit, stats);
	    printf(report_bw_jitter_loss_enhanced_triptime_format, stats->common->transferIDStr,
		   stats->ts.iStart, stats->ts.iEnd,
		   outbuffer, outbufferext,
		   stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
		   (100.0 * stats->cntError) / stats->cntDatagrams,
		   (meantransit * 1e3),
		   stats->transit.minTransit*1e3,
		   stats->transit.maxTransit*1e3,
		   (stats->transit.cntTransit < 2) ? 0 : sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e3,
		   (stats->cntIPG / stats->IPGsum),
		   llaw_buf,
		   netpower_buf);
	}
    }
    if (stats->latency_histogram) {
	histogram_print(stats->latency_histogram, stats->ts.iStart, stats->ts.iEnd);
    }
    _output_outoforder(stats);
    fflush(stdout);
}
void udp_output_read_enhanced_triptime_isoch (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_jitter_loss_enhanced_isoch_triptime);
    _print_stats_common(stats);
    if (!stats->cntIPG) {
	printf(report_bw_jitter_loss_suppress_enhanced_format, stats->common->transferIDStr,
	       stats->ts.iStart, stats->ts.iEnd,
	       outbuffer, outbufferext,
	       0.0, stats->cntError,
	       stats->cntDatagrams,
	       0.0,0.0,0.0,0.0,0.0,0.0);

    } else {
	// If the min latency is out of bounds of a realistic value
	// assume the clocks are not synched and suppress the
	// latency output
	if ((stats->transit.minTransit > UNREALISTIC_LATENCYMINMAX) ||
	    (stats->transit.minTransit < UNREALISTIC_LATENCYMINMIN)) {
	    printf(report_bw_jitter_loss_suppress_enhanced_format, stats->common->transferIDStr,
		   stats->ts.iStart, stats->ts.iEnd,
		   outbuffer, outbufferext,
		   stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
		   (100.0 * stats->cntError) / stats->cntDatagrams,
		   (stats->cntIPG / stats->IPGsum));
	} else {
	    double meantransit = (stats->transit.cntTransit > 0) ? (stats->transit.sumTransit / stats->transit.cntTransit) : 0;
	    int lambda =  ((stats->IPGsum > 0.0) ? (round (stats->cntIPG / stats->IPGsum)) : 0.0);
	    double variance = (stats->transit.cntTransit < 2) ? 0 : (sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e6);
	    set_llawbuf_udp(lambda, meantransit, variance, stats);
	    set_netpowerbuf(meantransit, stats);
	    printf(report_bw_jitter_loss_enhanced_isoch_format, stats->common->transferIDStr,
		   stats->ts.iStart, stats->ts.iEnd,
		   outbuffer, outbufferext,
		   stats->jitter*1e3, stats->cntError, stats->cntDatagrams,
		   (100.0 * stats->cntError) / stats->cntDatagrams,
		   (meantransit * 1e3),
		   stats->transit.minTransit*1e3,
		   stats->transit.maxTransit*1e3,
		   (stats->transit.cntTransit < 2) ? 0 : sqrt(stats->transit.m2Transit / (stats->transit.cntTransit - 1)) / 1e3,
		   (stats->cntIPG / stats->IPGsum),
		   llaw_buf,
		   netpower_buf,
		   stats->isochstats.cntFrames, stats->isochstats.cntFramesMissed);
	}
    }
    if (stats->latency_histogram) {
	histogram_print(stats->latency_histogram, stats->ts.iStart, stats->ts.iEnd);
    }
    if (stats->framelatency_histogram) {
	histogram_print(stats->framelatency_histogram, stats->ts.iStart, stats->ts.iEnd);
    }
    _output_outoforder(stats);
    fflush(stdout);
}
void udp_output_write (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_bw_format, stats->common->transferIDStr,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}

void udp_output_write_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_pps_enhanced);
    _print_stats_common(stats);
    printf(report_bw_pps_enhanced_format, stats->common->transferIDStr,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.write.WriteCnt,
	   stats->sock_callstats.write.WriteErr,
	   (stats->cntIPG ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}
void udp_output_write_enhanced_isoch (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_pps_enhanced_isoch);
    _print_stats_common(stats);
    printf(report_bw_pps_enhanced_isoch_format, stats->common->transferIDStr,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.write.WriteCnt,
	   stats->sock_callstats.write.WriteErr,
	   (stats->cntIPG ? (stats->cntIPG / stats->IPGsum) : 0.0),
	   stats->isochstats.cntFrames, stats->isochstats.cntFramesMissed, stats->isochstats.cntSlips);
    fflush(stdout);
}

// Sum reports
void udp_output_sum_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_sum_bw_jitter_loss_format,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->cntError, stats->cntDatagrams,
	   (100.0 * stats->cntError) / stats->cntDatagrams);
    if (stats->cntOutofOrder > 0) {
	printf(report_sum_outoforder,
	       stats->ts.iStart,
	       stats->ts.iEnd, stats->cntOutofOrder);
    }
    fflush(stdout);
}
void udp_output_sumcnt (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    if (stats->cntOutofOrder > 0) {
	if (isSumOnly(stats->common)) {
	    printf(report_sumcnt_outoforder,
		   stats->threadcnt,
		   stats->ts.iStart,
		   stats->ts.iEnd, stats->cntOutofOrder);
	} else {
	    printf(report_outoforder,
		   stats->common->transferIDStr, stats->ts.iStart,
		   stats->ts.iEnd, stats->cntOutofOrder);
	}
    }
    fflush(stdout);
}
void udp_output_sumcnt_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw_jitter_loss);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_jitter_loss_format, stats->threadcnt, stats->ts.iStart, stats->ts.iEnd, outbuffer, outbufferext, \
	   stats->cntError, stats->cntDatagrams, (stats->cntIPG && (stats->IPGsum > 0.0) ? (stats->cntIPG / stats->IPGsum) : 0.0));
    if (stats->cntOutofOrder > 0) {
	if (isSumOnly(stats->common)) {
	    printf(report_sumcnt_outoforder,
		   stats->threadcnt,
		   stats->ts.iStart,
		   stats->ts.iEnd, stats->cntOutofOrder);
	} else {
	    printf(report_outoforder,
		   stats->common->transferIDStr, stats->ts.iStart,
		   stats->ts.iEnd, stats->cntOutofOrder);
	}
    }
    fflush(stdout);
}

void udp_output_sumcnt_read_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw_read_enhanced);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_read_enhanced_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->jitter*1000.0, stats->cntError, stats->cntDatagrams,
	   (100.0 * stats->cntError) / stats->cntDatagrams);
    if (stats->cntOutofOrder > 0) {
	if (isSumOnly(stats->common)) {
	    printf(report_sumcnt_outoforder,
		   stats->threadcnt,
		   stats->ts.iStart,
		   stats->ts.iEnd, stats->cntOutofOrder);
	}
    }
    fflush(stdout);
}

void udp_output_sum_write (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_sum_bw_format, stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}
void udp_output_sumcnt_write (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}
void udp_output_sum_read_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_pps_enhanced);
    _print_stats_common(stats);
    printf(report_sum_bw_pps_enhanced_format,
	    stats->ts.iStart, stats->ts.iEnd,
	    outbuffer, outbufferext,
	    stats->cntError, stats->cntDatagrams,
	    (stats->cntIPG ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}
void udp_output_sum_write_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_pps_enhanced);
    _print_stats_common(stats);
    printf(report_sum_bw_pps_enhanced_format,
	    stats->ts.iStart, stats->ts.iEnd,
	    outbuffer, outbufferext,
	    stats->sock_callstats.write.WriteCnt,
	    stats->sock_callstats.write.WriteErr,
	   ((stats->cntIPG && (stats->IPGsum > 0.0)) ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}
void udp_output_sumcnt_write_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw_pps_enhanced);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_pps_enhanced_format, stats->threadcnt,
	    stats->ts.iStart, stats->ts.iEnd,
	    outbuffer, outbufferext,
	    stats->sock_callstats.write.WriteCnt,
	    stats->sock_callstats.write.WriteErr,
	   ((stats->cntIPG && (stats->IPGsum > 0.0)) ? (stats->cntIPG / stats->IPGsum) : 0.0));
    fflush(stdout);
}

void tcp_output_sum_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_sum_bw_format,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}
void tcp_output_sum_read_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_read_enhanced);
    _print_stats_common(stats);
    printf(report_sum_bw_read_enhanced_format,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.read.cntRead,
	   stats->sock_callstats.read.bins[0],
	   stats->sock_callstats.read.bins[1],
	   stats->sock_callstats.read.bins[2],
	   stats->sock_callstats.read.bins[3],
	   stats->sock_callstats.read.bins[4],
	   stats->sock_callstats.read.bins[5],
	   stats->sock_callstats.read.bins[6],
	   stats->sock_callstats.read.bins[7]);
    fflush(stdout);
}
void tcp_output_sumcnt_read (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}
void tcp_output_sumcnt_read_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw_read_enhanced);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_read_enhanced_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.read.cntRead,
	   stats->sock_callstats.read.bins[0],
	   stats->sock_callstats.read.bins[1],
	   stats->sock_callstats.read.bins[2],
	   stats->sock_callstats.read.bins[3],
	   stats->sock_callstats.read.bins[4],
	   stats->sock_callstats.read.bins[5],
	   stats->sock_callstats.read.bins[6],
	   stats->sock_callstats.read.bins[7]);
    fflush(stdout);
}

void tcp_output_sum_write (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw);
    _print_stats_common(stats);
    printf(report_sum_bw_format,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}
void tcp_output_sumcnt_write (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext);
    fflush(stdout);
}
void tcp_output_sum_write_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_bw_write_enhanced);
    _print_stats_common(stats);
    printf(report_sum_bw_write_enhanced_format,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.write.WriteCnt,
	   stats->sock_callstats.write.WriteErr
#ifdef HAVE_STRUCT_TCP_INFO_TCPI_TOTAL_RETRANS
	   ,stats->sock_callstats.write.TCPretry
#endif
    );
    fflush(stdout);
}
void tcp_output_sumcnt_write_enhanced (struct TransferInfo *stats) {
    HEADING_PRINT_COND(report_sumcnt_bw_write_enhanced);
    _print_stats_common(stats);
    printf(report_sumcnt_bw_write_enhanced_format, stats->threadcnt,
	   stats->ts.iStart, stats->ts.iEnd,
	   outbuffer, outbufferext,
	   stats->sock_callstats.write.WriteCnt,
	   stats->sock_callstats.write.WriteErr
#ifdef HAVE_STRUCT_TCP_INFO_TCPI_TOTAL_RETRANS
	   ,stats->sock_callstats.write.TCPretry
#endif
    );
    fflush(stdout);
}

// CSV output (note, not thread safe, only reporter can call these)
static char __timestring[200];
static void format_timestamp (struct timeval *timestamp, int enhanced) {
    if (!enhanced) {
	strftime(__timestring, 80, "%Y%m%d%H%M%S", localtime(&timestamp->tv_sec));
    } else {
	strftime(__timestring, 80, "%Y%m%d%H%M%S", localtime(&timestamp->tv_sec));
	snprintf((__timestring + strlen(__timestring)), 160, ".%.3d", (int) (timestamp->tv_usec/1000));
    }
}

static char __ips_ports_string[CSVPEERLIMIT];
static void format_ips_ports_string (struct TransferInfo *stats) {
    char local_addr[REPORT_ADDRLEN];
    char remote_addr[REPORT_ADDRLEN];
    int reverse = (stats->common->ThreadMode == kMode_Server);
    struct sockaddr *local = (reverse ? (struct sockaddr*)&stats->common->peer : (struct sockaddr*)&stats->common->local);
    struct sockaddr *peer = (reverse ? (struct sockaddr*)&stats->common->local : (struct sockaddr*)&stats->common->peer);
    __ips_ports_string[0] = '\0';
    if (local->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in*)local)->sin_addr,
		  local_addr, REPORT_ADDRLEN);
    }
#ifdef HAVE_IPV6
    else {
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)local)->sin6_addr,
		  local_addr, REPORT_ADDRLEN);
    }
#endif
    if (peer->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in*)peer)->sin_addr,
		  remote_addr, REPORT_ADDRLEN);
    }
#ifdef HAVE_IPV6
    else {
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)peer)->sin6_addr,
		  remote_addr, REPORT_ADDRLEN);
    }
#endif
    snprintf(__ips_ports_string, REPORT_ADDRLEN*2+10, reportCSV_peer,
	     local_addr, (local->sa_family == AF_INET ?
			  ntohs(((struct sockaddr_in*)local)->sin_port) :
#ifdef HAVE_IPV6
			  ntohs(((struct sockaddr_in6*)local)->sin6_port)),
#else
	     0),
#endif
	remote_addr, (peer->sa_family == AF_INET ?
		      ntohs(((struct sockaddr_in*)peer)->sin_port) :
#ifdef HAVE_IPV6
		      ntohs(((struct sockaddr_in6*)peer)->sin6_port)));
#else
                          0));
#endif
}

void udp_output_basic_csv (struct TransferInfo *stats) {
    format_timestamp(&stats->ts.nextTime, isEnhanced(stats->common));
    if (stats->csv_peer[0] == '\0') {
	format_ips_ports_string(stats);
	strncpy(&stats->csv_peer[0], &__ips_ports_string[0], CSVPEERLIMIT);
	stats->csv_peer[(CSVPEERLIMIT - 1)] = '\0';
    }
    intmax_t speed = (intmax_t) (((stats->cntBytes > 0) && (stats->ts.iEnd -  stats->ts.iStart) > 0.0) ? \
				 (((double)stats->cntBytes * 8.0) / (stats->ts.iEnd -  stats->ts.iStart)) : 0);
    printf(reportCSV_bw_jitter_loss_format,
	   __timestring,
	    stats->csv_peer,
	    stats->common->transferID,
	    stats->ts.iStart,
	    stats->ts.iEnd,
	    stats->cntBytes,
	    speed,
	    stats->jitter*1000.0,
	    stats->cntError,
	    stats->cntDatagrams,
	    (100.0 * stats->cntError) / stats->cntDatagrams, stats->cntOutofOrder );
}
void tcp_output_basic_csv (struct TransferInfo *stats) {
    format_timestamp(&stats->ts.nextTime, isEnhanced(stats->common));
    if (stats->csv_peer[0] == '\0') {
	format_ips_ports_string(stats);
	strncpy(&stats->csv_peer[0], &__ips_ports_string[0], CSVPEERLIMIT);
	stats->csv_peer[(CSVPEERLIMIT - 1)] = '\0';
    }
    intmax_t speed = (intmax_t) (((stats->cntBytes > 0) && (stats->ts.iEnd -  stats->ts.iStart) > 0.0) ? \
				 (((double)stats->cntBytes * 8.0) / (stats->ts.iEnd -  stats->ts.iStart)) : 0);
    printf(reportCSV_bw_format,
	   __timestring,
	   stats->csv_peer,
	   stats->common->transferID,
	   stats->ts.iStart,
	   stats->ts.iEnd,
	   stats->cntBytes,
	   speed);
}

/*
 * Report the client or listener Settings in default style
 */
static void output_window_size (struct ReportSettings *report) {
    int winsize = getsock_tcp_windowsize(report->common->socket, (report->common->ThreadMode != kMode_Client ? 0 : 1));
    byte_snprintf(outbuffer, sizeof(outbuffer), winsize, \
		  ((toupper(report->common->Format) == 'B') ? 'B' : 'A'));
    outbuffer[(sizeof(outbuffer)-1)] = '\0';
    printf("%s: %s", (isUDP(report->common) ? udp_buffer_size : tcp_window_size), outbuffer);
    if (report->common->winsize_requested == 0) {
        printf(" %s", window_default);
    } else if (winsize != report->common->winsize_requested) {
        byte_snprintf(outbuffer, sizeof(outbuffer), report->common->winsize_requested,
                       toupper((int)report->common->Format));
	outbuffer[(sizeof(outbuffer)-1)] = '\0';
	printf(warn_window_requested, outbuffer);
    }
    fflush(stdout);
}
static void reporter_output_listener_settings (struct ReportSettings *report) {
    if (report->common->PortLast > report->common->Port) {
	printf(server_pid_portrange, (isUDP(report->common) ? "UDP" : "TCP"), \
	       report->common->Port, report->common->PortLast, report->pid);
    } else {
	printf(isEnhanced(report->common) ? server_pid_port : server_port,
	       (isUDP(report->common) ? "UDP" : "TCP"), report->common->Port, report->pid);
    }
    if (report->common->Localhost != NULL) {
	if (isEnhanced(report->common) && !SockAddr_isMulticast(&report->local)) {
	    if (report->common->Ifrname)
		printf(bind_address_iface, report->common->Localhost, report->common->Ifrname);
	    else {
		char *host_ip = (char *) malloc(REPORT_ADDRLEN);
		if (host_ip != NULL) {
		    if (((struct sockaddr*)(&report->common->local))->sa_family == AF_INET) {
			inet_ntop(AF_INET, &((struct sockaddr_in*)(&report->common->local))->sin_addr,
				  host_ip, REPORT_ADDRLEN);
		    }
#ifdef HAVE_IPV6
		    else {
			inet_ntop(AF_INET6, &((struct sockaddr_in6*)(&report->common->local))->sin6_addr,
				  host_ip, REPORT_ADDRLEN);
		    }
#endif
		    printf(bind_address, host_ip);
		    free(host_ip);
		}
	    }
	}
	if (SockAddr_isMulticast(&report->local)) {
	    if(!report->common->SSMMulticastStr)
		if (!report->common->Ifrname)
		    printf(join_multicast, report->common->Localhost);
		else
		    printf(join_multicast_starg_dev, report->common->Localhost,report->common->Ifrname);
	    else if(!report->common->Ifrname)
		printf(join_multicast_sg, report->common->SSMMulticastStr, report->common->Localhost);
	    else
		printf(join_multicast_sg_dev, report->common->SSMMulticastStr, report->common->Localhost, report->common->Ifrname);
        }
    }
    if (isEnhanced(report->common)) {
	byte_snprintf(outbuffer, sizeof(outbuffer), report->common->BufLen, toupper((int)report->common->Format));
	byte_snprintf(outbufferext, sizeof(outbufferext), report->common->BufLen / 8, 'A');
	outbuffer[(sizeof(outbuffer)-1)] = '\0';
	outbufferext[(sizeof(outbufferext)-1)] = '\0';
	printf("%s: %s (Dist bin width=%s)\n", server_read_size, outbuffer, outbufferext);
    }
    if (isCongestionControl(report->common) && report->common->Congestion) {
	fprintf(stdout, "TCP congestion control set to %s\n", report->common->Congestion);
    }
    if (isUDP(report->common)) {
	if (isSingleClient(report->common)) {
	    fprintf(stdout, "WARN: Suggested to use lower case -u instead of -U (to avoid serialize & bypass of reporter thread)\n");
	} else if (isSingleClient(report->common)) {
	    fprintf(stdout, "Server set to single client traffic mode per -U (serialize traffic tests)\n");
	}
    } else if (isSingleClient(report->common)) {
	fprintf(stdout, "Server set to single client traffic mode (serialize traffic tests)\n");
    }
    if (isMulticast(report->common)) {
	fprintf(stdout, "Server set to single client traffic mode (per multicast receive)\n");
    }
    if (isRxHistogram(report->common)) {
	fprintf(stdout, "Enabled rx-histograms bin-width=%0.3f ms, bins=%d (clients must use --trip-times)\n", \
		((1e3 * report->common->RXbinsize) / pow(10,report->common->RXunits)), report->common->RXbins);
    }
    if (isFrameInterval(report->common)) {
#if HAVE_FASTSAMPLING
	fprintf(stdout, "Frame or burst interval reporting (feature is experimental)\n");
#else
	fprintf(stdout, "Frame or burst interval reporting (feature is experimental, ./configure --enable-fastsampling suggested)\n");
#endif
    }
    output_window_size(report);
    printf("\n");
    if (isPermitKey(report->common) && report->common->PermitKey) {
	if (report->common->ListenerTimeout > 0) {
	    fprintf(stdout, "Permit key is '%s' (timeout in %0.1f seconds)\n", report->common->PermitKey, report->common->ListenerTimeout);
	} else {
	    fprintf(stdout, "Permit key is '%s' (WARN: no timeout)\n", report->common->PermitKey);
	}
    }
    fflush(stdout);
}
static void reporter_output_client_settings (struct ReportSettings *report) {
    if (!report->common->Ifrnametx) {
	printf(isEnhanced(report->common) ? client_pid_port : client_port, report->common->Host,
	       (isUDP(report->common) ? "UDP" : "TCP"), report->common->Port, report->pid, \
	       (!report->common->threads ? 1 : report->common->threads));
    } else {
	printf(client_pid_port_dev, report->common->Host,
	       (isUDP(report->common) ? "UDP" : "TCP"), report->common->Port, report->pid, \
	       report->common->Ifrnametx, (!report->common->threads ? 1 : report->common->threads));
    }
    if ((isEnhanced(report->common) || isNearCongest(report->common)) && !isUDP(report->common)) {
	byte_snprintf(outbuffer, sizeof(outbuffer), report->common->BufLen, 'B');
	outbuffer[(sizeof(outbuffer)-1)] = '\0';
	printf("%s: %s\n", client_write_size, outbuffer);
    }
    if (isIsochronous(report->common)) {
	char meanbuf[40];
	char variancebuf[40];
	byte_snprintf(meanbuf, sizeof(meanbuf), report->isochstats.mMean, 'a');
	byte_snprintf(variancebuf, sizeof(variancebuf), report->isochstats.mVariance, 'a');
	meanbuf[39]='\0'; variancebuf[39]='\0';
	printf(client_isochronous, report->isochstats.mFPS, meanbuf, variancebuf, (report->isochstats.mBurstInterval/1000.0), (report->isochstats.mBurstIPG/1000.0));
    }
    if (isPeriodicBurst(report->common)) {
	char tmpbuf[40];
	byte_snprintf(tmpbuf, sizeof(tmpbuf), report->common->BurstSize, 'A');
	tmpbuf[39]='\0';
	printf(client_burstperiod, (1.0 / report->common->FPS), tmpbuf);
    }
    if (isFQPacing(report->common)) {
	byte_snprintf(outbuffer, sizeof(outbuffer), report->common->FQPacingRate, 'a');
	outbuffer[(sizeof(outbuffer)-1)] = '\0';
        printf(client_fq_pacing,outbuffer);
    }
    if (isCongestionControl(report->common) && report->common->Congestion) {
	fprintf(stdout, "TCP congestion control set to %s\n", report->common->Congestion);
    }
    if (isNearCongest(report->common)) {
	if (report->common->rtt_weight == NEARCONGEST_DEFAULT) {
	    fprintf(stdout, "TCP near-congestion delay weight set to %2.4f (use --near-congestion=<value> to change)\n", report->common->rtt_weight);
	} else {
	    fprintf(stdout, "TCP near-congestion delay weight set to %2.4f\n", report->common->rtt_weight);
	}
    }
    if (isSingleClient(report->common)) {
	fprintf(stdout, "WARN: Client set to bypass reporter thread per -U (suggest use lower case -u instead)\n");
    }
    if ((isIPG(report->common) || isUDP(report->common)) && !isIsochronous(report->common)) {
	byte_snprintf(outbuffer, sizeof(outbuffer), report->common->pktIPG, 'a');
	outbuffer[(sizeof(outbuffer)-1)] = '\0';
#ifdef HAVE_KALMAN
        printf(client_datagram_size_kalman, report->common->BufLen, report->common->pktIPG);
#else
        printf(client_datagram_size, report->common->BufLen, report->common->pktIPG);
#endif
    }
    if (isConnectOnly(report->common)) {
	fprintf(stdout, "TCP three-way-handshake (3WHS) only\n");
    } else {
	output_window_size(report);
	printf("\n");
    }
    fflush(stdout);
}

void reporter_connect_printf_tcp_final (struct ConnectionInfo * report) {
    if (report->connect_times.cnt > 1) {
        double variance = (report->connect_times.cnt < 2) ? 0 : sqrt(report->connect_times.m2 / (report->connect_times.cnt - 1));
        fprintf(stdout, "[ CT] final connect times (min/avg/max/stdev) = %0.3f/%0.3f/%0.3f/%0.3f ms (tot/err) = %d/%d\n", \
		report->connect_times.min,  \
	        (report->connect_times.sum / report->connect_times.cnt), \
		report->connect_times.max, variance,  \
		(report->connect_times.cnt + report->connect_times.err), \
		report->connect_times.err);
    }
    fflush(stdout);
}

void reporter_print_connection_report (struct ConnectionInfo *report) {
    assert(report->common);
    if (!(report->connecttime < 0)) {
	// copy the inet_ntop into temp buffers, to avoid overwriting
	char local_addr[REPORT_ADDRLEN];
	char remote_addr[REPORT_ADDRLEN];
	struct sockaddr *local = ((struct sockaddr*)&report->common->local);
	struct sockaddr *peer = ((struct sockaddr*)&report->common->peer);
	outbuffer[0]='\0';
	outbufferext[0]='\0';
	outbufferext2[0]='\0';
	char *b = &outbuffer[0];
	if (!isUDP(report->common) && (report->common->socket > 0) && (isPrintMSS(report->common) || isEnhanced(report->common)))  {
	    if (isPrintMSS(report->common) && (report->MSS <= 0)) {
		printf(report_mss_unsupported, report->MSS);
	    } else {
		snprintf(b, SNBUFFERSIZE-strlen(b), " (%s%d)", "MSS=", report->MSS);
		b += strlen(b);
	    }
	}
	if (isIsochronous(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (isoch)");
	    b += strlen(b);
	}
	if (isPeriodicBurst(report->common) && (report->common->ThreadMode != kMode_Client) && !isServerReverse(report->common)) {
#if HAVE_FASTSAMPLING
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (burst-period=%0.4fs)", (1.0 / report->common->FPS));
#else
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (burst-period=%0.2fs)", (1.0 / report->common->FPS));
#endif
	    b += strlen(b);
	}
	if (isFullDuplex(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (full-duplex)");
	    b += strlen(b);
	} else if (isServerReverse(report->common) || isReverse(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (reverse)");
	    b += strlen(b);
	    if (isFQPacing(report->common)) {
		snprintf(b, SNBUFFERSIZE-strlen(b), " (fq)");
		b += strlen(b);
	    }
	}
	if (isTxStartTime(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (epoch-start)");
	    b += strlen(b);
	}
	if (isL2LengthCheck(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (l2mode)");
	    b += strlen(b);
	}
	if (isUDP(report->common) && isNoUDPfin(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (no-udp-fin)");
	    b += strlen(b);
	}
	if (isTripTime(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (trip-times)");
	    b += strlen(b);
	}

	if (isEnhanced(report->common)) {
	    snprintf(b, SNBUFFERSIZE-strlen(b), " (sock=%d)", report->common->socket);;
	    b += strlen(b);
	}
	if (isEnhanced(report->common) || isPeerVerDetect(report->common)) {
	    if (report->peerversion[0] != '\0') {
		snprintf(b, SNBUFFERSIZE-strlen(b), "%s", report->peerversion);
		b += strlen(b);
	    }
	}
	if (!isServerReverse(report->common) && (isEnhanced(report->common) || isConnectOnly(report->common))) {
	    if (report->connect_timestamp.tv_sec > 0) {
		struct tm ts;
		ts = *localtime(&report->connect_timestamp.tv_sec);
		char now_timebuf[80];
		strftime(now_timebuf, sizeof(now_timebuf), "%Y-%m-%d %H:%M:%S (%Z)", &ts);
		if (!isUDP(report->common) && (report->common->ThreadMode == kMode_Client)) {
		    snprintf(b, SNBUFFERSIZE-strlen(b), " (ct=%4.2f ms) on %s", report->connecttime, now_timebuf);
		} else {
		    snprintf(b, SNBUFFERSIZE-strlen(b), " on %s", now_timebuf);
		}
		b += strlen(b);
	    }
	}
	if (local->sa_family == AF_INET) {
	    inet_ntop(AF_INET, &((struct sockaddr_in*)local)->sin_addr, local_addr, REPORT_ADDRLEN);
	}
#ifdef HAVE_IPV6
	else {
	    inet_ntop(AF_INET6, &((struct sockaddr_in6*)local)->sin6_addr, local_addr, REPORT_ADDRLEN);
	}
#endif
	if (peer->sa_family == AF_INET) {
	    inet_ntop(AF_INET, &((struct sockaddr_in*)peer)->sin_addr, remote_addr, REPORT_ADDRLEN);
	}
#ifdef HAVE_IPV6
	else {
	    inet_ntop(AF_INET6, &((struct sockaddr_in6*)peer)->sin6_addr, remote_addr, REPORT_ADDRLEN);
	}
#endif
#ifdef HAVE_IPV6
	if (report->common->KeyCheck) {
	    if (isEnhanced(report->common) && report->common->Ifrname && (strlen(report->common->Ifrname) < SNBUFFERSIZE-strlen(b))) {
		printf(report_peer_dev, report->common->transferIDStr, local_addr, report->common->Ifrname, \
		       (local->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)local)->sin_port) : \
			ntohs(((struct sockaddr_in6*)local)->sin6_port)), \
		       remote_addr, (peer->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)peer)->sin_port) : \
				     ntohs(((struct sockaddr_in6*)peer)->sin6_port)), outbuffer);
	    } else {
		printf(report_peer, report->common->transferIDStr, local_addr, \
		       (local->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)local)->sin_port) : \
			ntohs(((struct sockaddr_in6*)local)->sin6_port)), \
		       remote_addr, (peer->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)peer)->sin_port) : \
				     ntohs(((struct sockaddr_in6*)peer)->sin6_port)), outbuffer);
	    }
	} else {
	    printf(report_peer_fail, local_addr, \
		   (local->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)local)->sin_port) : \
		    ntohs(((struct sockaddr_in6*)local)->sin6_port)), \
		   remote_addr, (peer->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)peer)->sin_port) : \
				 ntohs(((struct sockaddr_in6*)peer)->sin6_port)), outbuffer);
	}

#else
	if (report->common->KeyCheck) {
	    if (isEnhanced(report->common) && report->common->Ifrname  && (strlen(report->common->Ifrname) < SNBUFFERSIZE-strlen(b))) {
		printf(report_peer_dev, report->common->transferIDStr, local_addr, report->common->Ifrname, \
		       local_addr, (local->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)local)->sin_port) : 0), \
		       remote_addr, (peer->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)peer)->sin_port) :  0), \
		       outbuffer);
	    } else {
		printf(report_peer, report->common->transferIDStr, \
		       local_addr, (local->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)local)->sin_port) : 0), \
		       remote_addr, (peer->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)peer)->sin_port) :  0), \
		       outbuffer);
	    }
	} else {
	    printf(report_peer_fail, local_addr, (local->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)local)->sin_port) : 0), \
		   remote_addr, (peer->sa_family == AF_INET ? ntohs(((struct sockaddr_in*)peer)->sin_port) :  0), \
		   outbuffer);
	}
#endif
	if ((report->common->ThreadMode == kMode_Client) && !isServerReverse(report->common)) {
	    if (isTxHoldback(report->common) || isTxStartTime(report->common)) {
		struct tm ts;
		char start_timebuf[80];
		struct timeval now;
		struct timeval start;
#ifdef HAVE_CLOCK_GETTIME
		struct timespec t1;
		clock_gettime(CLOCK_REALTIME, &t1);
		now.tv_sec  = t1.tv_sec;
		now.tv_usec = t1.tv_nsec / 1000;
#else
		gettimeofday(&now, NULL);
#endif
		ts = *localtime(&now.tv_sec);
		char now_timebuf[80];
		strftime(now_timebuf, sizeof(now_timebuf), "%Y-%m-%d %H:%M:%S (%Z)", &ts);
		// Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
		int seconds_from_now;
		if (isTxHoldback(report->common)) {
		    seconds_from_now = report->txholdbacktime.tv_sec;
		    if (report->txholdbacktime.tv_usec > 0)
			seconds_from_now++;
		    start.tv_sec = now.tv_sec + seconds_from_now;
		    ts = *localtime(&start.tv_sec);
		} else {
		    ts = *localtime(&report->epochStartTime.tv_sec);
		    seconds_from_now = ceil(TimeDifference(report->epochStartTime, now));
		}
		strftime(start_timebuf, sizeof(start_timebuf), "%Y-%m-%d %H:%M:%S", &ts);
		if (seconds_from_now > 0) {
		    printf(client_report_epoch_start_current, report->common->transferID, seconds_from_now, \
			   start_timebuf, now_timebuf);
		} else {
		    printf(warn_start_before_now, report->common->transferID, report->epochStartTime.tv_sec, \
			   report->epochStartTime.tv_usec, start_timebuf, now_timebuf);
		}
	    }
	}
    }
    fflush(stdout);
}

void reporter_print_settings_report (struct ReportSettings *report) {
    assert(report != NULL);
    report->pid =  (int)  getpid();
    printf("%s", separator_line);
    if (report->common->ThreadMode == kMode_Listener) {
	reporter_output_listener_settings(report);
    } else {
	reporter_output_client_settings(report);
    }
    printf("%s", separator_line);
    fflush(stdout);
}

void reporter_peerversion (struct ConnectionInfo *report, uint32_t upper, uint32_t lower) {
    if (!upper || !lower) {
	report->peerversion[0]='\0';
    } else {
	int rel, major, minor, alpha;
	rel = (upper & 0xFFFF0000) >> 16;
	major = (upper & 0x0000FFFF);
	minor = (lower & 0xFFFF0000) >> 16;
	alpha = (lower & 0x0000000F);
	snprintf(report->peerversion, (PEERVERBUFSIZE-10), " (peer %d.%d.%d)", rel, major, minor);
	switch(alpha) {
	case 0:
	    sprintf(report->peerversion + strlen(report->peerversion) - 1,"-dev)");
	    break;
	case 1:
	    sprintf(report->peerversion + strlen(report->peerversion) - 1,"-rc)");
	    break;
	case 2:
	    sprintf(report->peerversion + strlen(report->peerversion) - 1,"-rc2)");
	    break;
	case 3:
	    break;
	case 4:
	    sprintf(report->peerversion + strlen(report->peerversion) - 1,"-private)");
	    break;
	case 5:
	    sprintf(report->peerversion + strlen(report->peerversion) - 1,"-master)");
	    break;
	default:
	    sprintf(report->peerversion + strlen(report->peerversion) - 1, "-unk)");
	}
	report->peerversion[PEERVERBUFSIZE-1]='\0';
    }
}

void reporter_print_server_relay_report (struct ServerRelay *report) {
    printf(server_reporting, report->info.common->transferID);
    if (!isEnhanced(report->info.common)) {
	udp_output_read(&report->info);
    } else {
	udp_output_read_enhanced(&report->info);
    }
    fflush(stdout);
}
