#!/bin/bash

for i in `cat targets.conf  | awk '{print $2;}'`; do 
	echo $i; 
	snmpwalk -v 2c -c 'evo5$link' ar1.sof10.evolink.net $i ; 
done

