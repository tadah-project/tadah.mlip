#!/bin/bash
# remove all headers in db file before running this script.
# assumes no stress is given in an old db file so we set it to 0

    ##########
    # CONFIG #
    ##########

ENERGYROW=$2 # position of the first energy row

    #################
    # END OF CONFIG #
    #################

ene=($(awk -v erow="$ENERGYROW" '{if (NR%erow==0) print $0}' $1))
i=1
j=0
nstruc=1
while IFS= read -r line
do
	if (( $i == 1 )); then
		echo "Structure " $nstruc
		echo ${ene[$j]}
		j=$(( $j+1 ))
		nstruc=$(( $nstruc+1 ))
	fi

    # no stress so print zero tensor
	if (( $i == 4 )); then
		echo "0.0 0.0 0.0"
		echo "0.0 0.0 0.0"
		echo "0.0 0.0 0.0"
	fi

	if (( $i == $ENERGYROW )); then
		i=0
		echo ""
	else
		echo "$line" 
	fi

	i=$(( $i+1 ))
done < $1


