#!/usr/bin/env bash

# Author James C. Owens
# Version 2.5 - includes datetime stamps in UTC and also puts in output log and debug.log
# Version 3.0 - includes option for random or sequential execution of command file
# Version 3.5 - includes parallelism counter (j) and parallelism limiter
# Version 4.0 - Parameterize rpc_test_output.log and debug.log locations

export LC_ALL=C

timestamp() {
  date --utc +"%m/%d/%Y %H:%M:%S.%N"
}

lock="./$BASHPID.lock"
exec 8>$lock;

j_atom() {
(
flock -w 60.0 -x 8
echo $(<$lock)
) 8<$lock
}

j++() {
(
flock -w 60.0 -x 8
j=$(<$lock)
echo $((j + 1)) | tee $lock
) 8<$lock
}

j--() {
(
flock -w 60.0 -x 8
j=$(<$lock)
echo $((j - 1)) | tee $lock
)
} 8<$lock


gridcoindaemon=$1
command_file=$2
rpc_test_output_log=$3
debug_log=$4
iterations=$5
sleep_time=$6
maximum_parallelism=$7
random=$8

readarray -t command < <(sed -e "s|\r||" $command_file | grep -v "^#")
array_size=${#command[@]}

# For troubleshooting, uncomment the below
#echo "lockfile" $lock
#echo "command_file" $command_file
#echo "rpc test output log" $rpc_test_output_log
#echo "debug log" $debug_log
#echo "iterations" $iterations
#echo "array size" $array_size
#echo "sleep time" $sleep_time
#echo "maximum parallelism" $maximum_parallelism
#echo "random" $random

$gridcoindaemon debug4 true 1> /dev/null 2> /dev/null

# shells in progress (parallelism) count initialization
echo "0" > $lock

for ((i=1;i<=iterations;i++)); do

	if ((random==1)); then
	  index=$(shuf -i 1-$array_size -n 1)
	else
	  index=$((i%array_size)) 
	fi

	#echo "index" $index

	cmd=${command[$((index-1))]}

	#echo i=$i
	#echo j=$(j_atom)

	if (($(j_atom)<maximum_parallelism)); then

		# The below is executed in a subshell with an ampersand to achieve parallelism.
		(
			j_begin=$(j++)
			datetime_begin=$(timestamp)
			echo "$datetime_begin,$i,$j_begin,begin,$gridcoindaemon" $cmd | tee -a $rpc_test_output_log $debug_log

			eval $gridcoindaemon $cmd >> $rpc_test_output_log;
			
			j_end=$(j--)
			datetime_end=$(timestamp)
			echo "$datetime_end,$i,$j_end,end,$gridcoindaemon" $cmd | tee -a $rpc_test_output_log $debug_log
			
		) &

	else ((i--))
	fi

	sleep $sleep_time

done

wait

rm $lock
