#!/bin/bash

set -e

if [[ $# != 2 ]] ; then
	echo "Error: name protocol dim "
	exit -1
fi;

if [ ! -e ../data/${1}_${2}.out ]
then
	echo "Error: unknown file"
	cd ../data && ls tcp*.out udp*.out
	exit -1
fi;
sampleRTT=($(cat ../data/${1}_${2}.out | grep "Round" | tr -s " " | cut -d " " -f 5  ))
nRepetitions=($(cat ../data/${1}_${2}.out |grep "Round" | tr -s " " | cut -d " " -f 9 | tail -n 1 | bc -l) )

#estimatedRTTi = 0.875*estimatedRTT(i-1) + 0.125*sampleRTTi
#variabilityRTTi = 0.75*variabilityRTT(i-1) + 0.25*|sampleRTTi - estimatedRTTi|
# repetition *_sampleRTT *_estimatedRTT *_variabilityRTT (4 colonne, con *=tcp oppure *=udp)

if [ ! -d  ../data/${1}_data ] ; then
  mkdir ../data/${1}_data
fi


rm -r -f  ../data/${1}_data/${1}_${2}
mkdir  ../data/${1}_data/${1}_${2}
estimatedRTT=$(echo ${sampleRTT[0]} | bc -l)
variabilityRTT=$(echo 0 | bc -l)
printf "1 ${sampleRTT[0]} $estimatedRTT $variabilityRTT\n" >> ../data/${1}_data/${1}_${2}/${1}_${2}.dat

for (( Index=1; Index < $nRepetitions; Index++ )); do 
	estimatedRTT=$(echo 0.875*$estimatedRTT + 0.125*${sampleRTT[$Index]}| bc -l)
	differenceRTT=$(echo  ${sampleRTT[$Index]} - $estimatedRTT  | bc -l | tr -d -)
	variabilityRTT=$(echo 0.75*$variabilityRTT + 0.25*$differenceRTT| bc -l)
	differenceRTT1=$(echo $Index + 1 | bc)
	printf "$differenceRTT1 ${sampleRTT[$Index]} $estimatedRTT $variabilityRTT\n" >> ../data/${1}_data/${1}_${2}/${1}_${2}.dat
done

gnuplot <<-eNDgNUPLOTcOMMAND
	set term png size 900, 700
	set output "../data/${1}_data/${1}_${2}/${1}_${2}.png"
	set xrange [0:]
	set xtics 20

	set nologscale x 
	set nologscale y 
	set xlabel "repetition"
	set ylabel "RTT"
	plot "../data/${1}_data/${1}_${2}/${1}_${2}.dat" using 1:2 title "${1} ${2} sampleRTT" \
			with linespoints , \
		"../data/${1}_data/${1}_${2}/${1}_${2}.dat" using 1:3 title "${1} ${2} estimatedRTT" \
			with linespoints  , \
		"../data/${1}_data/${1}_${2}/${1}_${2}.dat" using 1:4 title "${1} ${2} variabilityRTT" \
			with linespoints
	clear
eNDgNUPLOTcOMMAND

xdg-open ../data/${1}_data/${1}_${2}/${1}_${2}.png 2>>/dev/null &

