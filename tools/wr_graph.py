#!/usr/bin/python

#####################################################
## This work is part of the White Rabbit project
##
## The script analyzes log messages from
## _stat cont_ shell command of WRPC.
##
## Author: Grzegorz Daniluk, Anders Wallin
#####################################################

import sys
import time
import numpy as np
import matplotlib as mpl

import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

DISP_FIELDS = ['lock', 'mu', 'cko', 'sec']
ANL_FIELDS  = ['lock', 'mu', 'cko', 'sec', 'nsec', 'ss']

MIN_TIME_INTERVAL	= 0.5	# for logs produced every second, MUST BE CHANGED if
MAX_TIME_INTERVAL = 1.5 # logs are produced more often
MAX_MU_JUMP  = 4000
MAX_CKO_JUMP = 50

#####################################################
# parse log file and return 2d array with values we want to analyze (ANL_FIELDS)
def get_wr_data(filename):
	f = open(filename)
	vals = []
	for i in range(0, len(ANL_FIELDS)):
		vals.append([])
	for line in f:
		found = 0
		if line.startswith( 'lnk:' ): # process only lines with stat-data
			for field in line.split():
				if found == len(ANL_FIELDS):
					break
				val = field.split(':')
				if val[0] in ANL_FIELDS:
					found +=1
					try:
						vals[ANL_FIELDS.index(val[0])].append(int(val[1]))
					except ValueError:
						vals[ANL_FIELDS.index(val[0])].append(val[1])
	return vals

#####################################################

def plot_combo_data(vals):
	fig = []
	ax = []

	tstamp = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
	for i in range(0, len(vals)):
		fig.append(plt.figure(i))
		plt.title('%s, updated %s' % (DISP_FIELDS[i], tstamp) )

		ax.append(fig[i].add_subplot(111))
		ax[i].set_xlabel('datapoint number')
		ax[i].set_ylabel(DISP_FIELDS[i])
		ax[i].plot(range(0, len(vals[i])), vals[i], marker='.', linestyle='None', c='b',scaley=True,scalex=False)
		ax[i].set_xlim([ 0 , len(vals[i]) ])

#####################################################
# analyze if _lock_ is always 1
def anl_lock(vals):
	lock_idx = ANL_FIELDS.index("lock")
	# find first sample when it got locked
	start=0;
	for lock in vals[lock_idx]:
		if lock == 1:
			break
		start += 1

	#now from that place we check if it's always locked
	passed = 1;
	for i in range(start, len(vals[lock_idx])):
		if vals[lock_idx][i] != 1:
			print "ERROR: loss lock at sample " + str(i)
			passed = 0;

	return passed

#####################################################

# analyze if _ss_ is always 'TRACK_PHASE'
def anl_state(vals):
	state_idx = ANL_FIELDS.index("ss")
	# find first sample when it got to TRACK_PHASE
	start=0;
	for ss in vals[state_idx]:
		if ss == """'TRACK_PHASE'""":
			break
		start += 1

	#now from that place we check if it's always TRACK_PHASE
	passed = 1;
	for i in range(start, len(vals[state_idx])):
		if vals[state_idx][i] != """'TRACK_PHASE'""":
			print "ERROR: quit TRACK_PHASE at sample " + str(i) + " to " + vals[state_idx][i]
			passed = 0;

	return passed

#####################################################

def anl_time(vals):
	sec_idx  = ANL_FIELDS.index("sec")
	nsec_idx = ANL_FIELDS.index("nsec")
	ss_idx 	 = ANL_FIELDS.index("ss")
	# first convert sec,nsec to float
	time_log = []
	for i in range(0, len(vals[sec_idx])):
		time_log.append(float(vals[sec_idx][i]) + float(vals[nsec_idx][i])/1000000000)
	# find where it first was synchronized (state == TRACK_PHASE)
	start=0;
	for ss in vals[ss_idx]:
		if ss == """'TRACK_PHASE'""":
			break
		start += 1

	passed = 1
	# check if it always increases
	for i in range(start+1, len(time_log)):
		if time_log[i] < time_log[i-1] or time_log[i] - time_log[i-1] < MIN_TIME_INTERVAL or time_log[i] - time_log[i-1] > MAX_TIME_INTERVAL:
			print "ERROR: time counter at sample " + str(i)
			passed = 0;

	return passed
#####################################################

def anl_rtt(vals):
	mu_idx = ANL_FIELDS.index("mu")
	passed = 1
	for i in range(1, len(vals[mu_idx])):
		if vals[mu_idx][i] - vals[mu_idx][i-1] > MAX_MU_JUMP or vals[mu_idx][i] - vals[mu_idx][i-1] < -MAX_MU_JUMP:
			print "ERROR: rtt jump at sample " + str(i) + " is " + str(vals[mu_idx][i]-vals[mu_idx][i-1]) + "ps"
			passed = 0

	return passed

#####################################################

def anl_cko(vals):
	ss_idx 	= ANL_FIELDS.index("ss")
	cko_idx = ANL_FIELDS.index("cko")

	# find where it was first synchronized (state == TRACK_PHASE)
	start=0;
	for ss in vals[ss_idx]:
		if ss == """'TRACK_PHASE'""":
			break
		start += 1

	passed = 1
	for i in range(start, len(vals[cko_idx])):
		if vals[cko_idx][i] > MAX_CKO_JUMP or vals[cko_idx][i] < -MAX_CKO_JUMP:
			print "ERROR: cko too large at sample " + str(i) + " value is " + str(vals[cko_idx][i]) + "ps"
			passed = 0

	return passed
	
#####################################################

if len(sys.argv) == 1:
	print "You did not specify the log file"
	sys.exit()

vals = get_wr_data(sys.argv[1])

if anl_lock(vals):
	print "LOCK:\t Success, always locked"
if anl_state(vals):
	print "STATE:\t Success, always TRACK_PHASE"
if anl_time(vals):
	print "TIME:\t Success, always growing"
if anl_rtt(vals):
	print "RTT:\t Success, no jumps detected"
if anl_cko(vals):
	print "CKO:\t Success, no values outside accepted range"

plot_combo_data(vals[0:len(DISP_FIELDS)])
plt.show()
