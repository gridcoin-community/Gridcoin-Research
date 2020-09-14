rpc_stress_test.sh

Original Author: James C. Owens
Current Version: 4.0

rpc_stress_test.sh is a bash script that runs rpc commands against the Gridcoin client at a controllable rate to provide for stress testing. The script is multithreaded (using subshells with the & ending). It will only run under Linux. It may run under WSL or cygwin but has not been tested in that environment. The script has no error checking, and so must be used only by people able to properly diagnose possible error conditions encountered during testing. Additions to improve the script are always welcome.

The usage is rpc_stress_test.sh <gridcoindaemon> <command_file> <rpc_test_output_log> <debug_log> <iterations> <sleep_time> <maximum_parallelism> <random>

<gridcoindaemon> : The name of the gridcoin daemon executable. This will typically be gridcoindaemon.
<command_file> : The name of the file that contains the commands that will be executed as part of the test. You may use # in front of a command to "comment it out" which allows you to conveniently turn off a command. Note that the script uses eval to execute the command, so some script variables can be used in the commands.
<rpc_test_output_log> : The name of the file that will contain the command outputs.
<debug.log> : The name of the gridcoin debug log (usually debug.log), to integrate the rpc timings into the main log to provide an integrated picture.
<iterations> : The number of commands desired to be run during the test.
<sleep_time> : The amount of seconds (in decimal form) to wait from one command invocation to the next.
<maximum_parallelism> : The maximum number of commands that will be allowed to be in flight at any one time. This is to prevent a runaway situation with a pileup of many commands in case some of them become unresponsive, which could happen in a stress testing scenario.
<random> : 0 will cause the script to execute the commands sequentially in the order listed in the command file. 1 will cause the script to pick commands at random from the command file.

The script should usually be run while in the .GridcoinResearch directory (for mainnet) or .GridcoinResearch/testnet directory (if testnet). It is STRONGLY ADVISED that any stress testing be done against wallets in testnet and not mainnet.

Stress testing involving commands that generate transactions on the network is not allowed at any time on mainnet, and will only be allowed on testnet by permission of the testnet coordinator, under carefully reviewed and controlled conditions.

A typical commandline for testnet (from the testnet subdirectory) would be

./rpc_stress_test.sh ../gridcoinresearchd command_file.dat rpc_test_output.log debug.log 1000 0.5 100 1 | tee rpc_test.log

This would be a test of 1000 command invocations at random from the command file, with a 0.5 second spacing between command invocations.

The purpose of the pipe and tee at the end is to provide the output of the script to both the console and a log at the same time.

The script console output looks like the following (this is an example run with the provided example command file:

05/23/2018 01:53:37.715987571,1,1,begin,../gridcoinresearchd-staging.sh -testnet getblockchaininfo
05/23/2018 01:53:37.823599886,2,2,begin,../gridcoinresearchd-staging.sh -testnet dumpprivkey $($gridcoindaemon getnewaddress; sleep 0.25)
05/23/2018 01:53:37.826900191,1,1,end,../gridcoinresearchd-staging.sh -testnet getblockchaininfo
05/23/2018 01:53:37.915069162,3,2,begin,../gridcoinresearchd-staging.sh -testnet dumpprivkey $($gridcoindaemon getnewaddress; sleep 0.25)
05/23/2018 01:53:38.021945936,4,3,begin,../gridcoinresearchd-staging.sh -testnet getnetworkinfo
05/23/2018 01:53:38.139469907,5,4,begin,../gridcoinresearchd-staging.sh -testnet getnetworkinfo
05/23/2018 01:53:38.266971443,6,5,begin,../gridcoinresearchd-staging.sh -testnet getblockchaininfo
05/23/2018 01:53:38.361519191,7,6,begin,../gridcoinresearchd-staging.sh -testnet listtransactions
05/23/2018 01:53:38.487301179,8,7,begin,../gridcoinresearchd-staging.sh -testnet listtransactions
05/23/2018 01:53:38.608563671,9,8,begin,../gridcoinresearchd-staging.sh -testnet getwalletinfo
05/23/2018 01:53:38.687542740,4,6,end,../gridcoinresearchd-staging.sh -testnet getnetworkinfo
05/23/2018 01:53:38.683003108,5,7,end,../gridcoinresearchd-staging.sh -testnet getnetworkinfo
05/23/2018 01:53:38.725206893,10,7,begin,../gridcoinresearchd-staging.sh -testnet getwalletinfo
05/23/2018 01:53:38.733894002,7,6,end,../gridcoinresearchd-staging.sh -testnet listtransactions
05/23/2018 01:53:38.795346312,2,5,end,../gridcoinresearchd-staging.sh -testnet dumpprivkey $($gridcoindaemon getnewaddress; sleep 0.25)
05/23/2018 01:53:38.839347841,6,4,end,../gridcoinresearchd-staging.sh -testnet getblockchaininfo
05/23/2018 01:53:38.885275421,8,3,end,../gridcoinresearchd-staging.sh -testnet listtransactions
05/23/2018 01:53:38.895480791,11,4,begin,../gridcoinresearchd-staging.sh -testnet listunspent
05/23/2018 01:53:38.963203747,12,5,begin,../gridcoinresearchd-staging.sh -testnet getwalletinfo
05/23/2018 01:53:38.997576132,9,4,end,../gridcoinresearchd-staging.sh -testnet getwalletinfo
05/23/2018 01:53:39.015189015,10,3,end,../gridcoinresearchd-staging.sh -testnet getwalletinfo
05/23/2018 01:53:39.031542876,3,2,end,../gridcoinresearchd-staging.sh -testnet dumpprivkey $($gridcoindaemon getnewaddress; sleep 0.25)
05/23/2018 01:53:39.035421544,12,1,end,../gridcoinresearchd-staging.sh -testnet getwalletinfo
05/23/2018 01:53:39.055025731,11,0,end,../gridcoinresearchd-staging.sh -testnet listunspent

This is in CSV format to be easily processed by an analysis script or spreadsheet.

The columns are as follows:
Date/time, Invocation Sequence Number, Parallelism Number, command begin or end, command.

For the parallelism number, if it is the command begin, it represents the number of tasks in flight including the one that was just started. If it is the command end, it represents the number of commands in flight still after the command has ended (i.e. not including the just finished command).



