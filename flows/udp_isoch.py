#!/usr/bin/env python3.5
#
# Author Robert J. McMahon
# Date November 2017
import shutil
import logging
import flows
import argparse
import time, datetime
import os,sys

from datetime import datetime as datetime, timezone
from flows import *

parser = argparse.ArgumentParser(description='Run an isochronous UDP data stream')
parser.add_argument('-s','--server', type=str, required=True, help='host to run iperf server')
parser.add_argument('-c','--client', type=str, default='localhost', required=False, help='host to run iperf client')
parser.add_argument('-d','--dst', type=str, required=True, help='iperf destination ip address')
parser.add_argument('-i','--interval', type=int, required=False, default=0, help='iperf report interval')
parser.add_argument('-t','--time', type=int, default=10, required=False, help='time or duration to run traffic')
parser.add_argument('-O','--offered_load', type=str, default="60:18M,0", required=False, help='offered load; <fps>:<mean>,<variance>')
parser.add_argument('-T','--title', type=str, default="", required=False, help='title for graphs')
parser.add_argument('-S','--tos', type=str, default='BE', required=False, help='type of service or access class; BE, VI, VO or BK')
parser.add_argument('-o','--output_directory', type=str, required=False, default='./data', help='output directory')
parser.add_argument('--loglevel', type=str, required=False, default='INFO', help='python logging level, e.g. INFO or DEBUG')
parser.add_argument('--stress_server', type=str, required=False, default=None, help='host to run iperf server for stress traffic')
parser.add_argument('--stress_client', type=str, required=False, default=None, help='host to run iperf client for stress traffic')
parser.add_argument('--stress_proto', type=str, required=False, default='TCP', help='IP protocol for stress traffic')
parser.add_argument('--stress_offered_load', type=str, required=False, default='20M', help='Offered load for stress traffic')
parser.add_argument('--stress_tos', type=str, default='BE', required=False, help='type of service or access class for stress traffic; BE, VI, VO or BK')
parser.add_argument('--stress_dst', type=str, required=False, default=None, help='iperf destination ip address for stress traffic')

args = parser.parse_args()

logfilename='test.log'
if not os.path.exists(args.output_directory):
    print('Making log directory {}'.format(args.output_directory))
    os.makedirs(args.output_directory)

fqlogfilename = os.path.join(args.output_directory, logfilename)
print('Writing log to {}'.format(fqlogfilename))

logging.basicConfig(filename=fqlogfilename, level=logging.INFO, format='%(asctime)s %(name)s %(module)s %(levelname)-8s %(message)s')

logging.getLogger('asyncio').setLevel(logging.INFO)
root = logging.getLogger(__name__)
loop = asyncio.get_event_loop()
loop.set_debug(False)

plottitle='('+ args.offered_load + ' ' + args.tos +') ' + args.title + ' ' + str(args.time) + 'sec '

#main udp isochronous traffic flow
flows = [iperf_flow(name="ISOCH", server=args.server, client=args.client, dst=args.dst, proto='UDP', offered_load=args.offered_load, interval=args.interval, flowtime=args.time, tos=args.tos, debug=False)]

print("Running isochronous traffic client={} server={} dest={} with load {} for {} seconds".format(args.client, args.server, args.dst, args.offered_load, args.time))

#optional "stressor flow"
if args.stress_server and args.stress_client and args.stress_dst :
    flows.append(iperf_flow(name="STRESS", user='root', server=args.stress_server, client=args.stress_client, dst=args.stress_dst, proto=args.stress_proto, offered_load=args.stress_offered_load, interval=args.interval, flowtime=args.time, tos=args.stress_tos, debug=False))
    print("Running stress {} traffic client={} server={} dest={} with load {}".format(args.stress_proto, args.stress_client, args.stress_server, args.stress_dst, args.stress_offered_load, args.time))

iperf_flow.run(time=args.time, flows='all', preclean=False)
for flow in flows :
    for histogram in flow.histograms :
        histogram.plot(title=plottitle, directory=args.output_directory)
        histogram.plot(title=plottitle, outputtype='svg', directory=args.output_directory)
print('Finished.  Results written to directory {}'.format(args.output_directory))

logging.shutdown()