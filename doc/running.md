<!--- 02-21-2015  R. Halford --->

Running Gridcoin in Secure Environment
======================================

This describes  the process to run Gridcoin in a secure environment. Please look at the build docs to generate the binaries first.

```bash
$su root
$cd Gridcoin-Research/src
$adduser grcuser && usermod -g users grcuser && delgroup grcuser && chmod 0701 /home/grcuser
$mkdir /home/grcuser/bin
$cp gridcoinresearchd /home/grcuser/bin/gridcoinresearchd
$chown -R grcuser:users /home/grcuser/bin
$cd && rm -rf grcuser
```

Then start Gridcoin Research (From User Environment):

```bash
$su grcuser
$cd && bin/gridcoinresearchd
```

After running the first time you will receive an error instructing you to create a config file called gridcoinresearch.conf:

	$nano ~/.GridcoinResearch/gridcoinresearch.conf && chmod 0600 ~/.GridcoinResearch/gridcoinresearch.conf

Add the following to your config file, changing the username and password to something secure:

```
daemon=1
server=1
email=
rpcuser=<username>
rpcpassword=<password>
addnode=node.gridcoin.us
```

Then press Ctrl-X to exit and save and lastly run the Daemon:

```bash
$cd /bin
$./gridcoinresearchd
```
Wait til server starts, then verify that getinfo returns with connections > 1 and Blocks > 1:

	gridcoinresearchd getinfo

