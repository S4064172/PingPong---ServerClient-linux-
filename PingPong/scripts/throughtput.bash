#!/bin/bash


set -e

if [[ $# != 1 ]] ; then
	echo "Error: name protocol"
	exit -1
fi;

if [ ! -e ../data/${1}_throughput.dat ]
then
	echo "Error: unknown file"
	cd ../data && ls *_throughput.dat
	exit -1
fi;



CampinoneMin=($(head -n 1 ../data/${1}_throughput.dat ))
CampioneMax=($(tail  -n 1 ../data/${1}_throughput.dat))
	
DimMinByte=$( sed -e 's/[eE]+*/\\*10\\^/' <<< ${CampinoneMin[0]} ) #(printf "%.10f" ${String1[0]})  
ThroughputMin=$(sed -e 's/[eE]+*/\\*10\\^/' <<< ${CampinoneMin[2]} ) #(printf "%.10f" ${String1[2]})  
DimMaxByte=$(sed -e 's/[eE]+*/\\*10\\^/' <<< ${CampioneMax[0]} ) #(printf "%.10f" ${String1[0]})  
ThroughputMax=$(sed -e 's/[eE]+*/\\*10\\^/' <<< ${CampioneMax[2]} ) #(printf "%.10f" ${String1[2]})  

DelayMin=$(echo ${DimMinByte}/${ThroughputMin} | bc -l)
DelayMax=$(echo ${DimMaxByte}/${ThroughputMax} | bc -l)
differenceN=$(echo ${DimMaxByte}-${DimMinByte} | bc -l)
differenceD=$(echo ${DelayMax}-${DelayMin}  | bc -l)

B=$(echo "scale=5; ${differenceN}/${differenceD}" | bc -l)
L=$(echo "scale=9; ((-${DimMinByte})/${B}+${DelayMin})/1" | bc -l)


if [ ! -d  ../data/${1}_data ] ; then
  mkdir ../data/${1}_data
fi

rm -f  ../data/${1}_data/${1}.png

gnuplot <<-eNDgNUPLOTcOMMAND
	f(x) = x / ( $L + x / $B)
	set term png size 900, 700
	set output "../data/${1}_data/${1}.png"
	set xrange [$DimMinByte: ]
	set logscale x 2
	set logscale y 10
	set xlabel "msg size (B)"
	set ylabel "throughput (KB/s)"
	plot  f(x) title "Latency-Bandwith model with L = ${L} ane B=${B}" \
		with linespoints, \
		"../data/${1}_throughput.dat" using 1:3 title "${1} ping-pong Throughput" \
			with linespoints
	clear
eNDgNUPLOTcOMMAND

xdg-open ../data/${1}_data/${1}.png 2>>/dev/null &














