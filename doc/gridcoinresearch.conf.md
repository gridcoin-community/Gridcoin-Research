## Default gridcoinresearch.conf location

    Windows:  %AppData%\GridcoinResearch\

    Linux:    ~/.GridcoinResearch/

    macOS:    /Users/USERNAME/Library/Application Support/GridcoinResearch/

## Testnet

Note: It is not supported to enter *testnet=1* flag into configuration
file. It *must* be specified on the command line in the form of
*-testnet* argument. Keyword testnet in configuration file has undefined
behavior.

## Basic Configuration File

    #############################################################################
    #################### Example gridcoinresearch.conf file #####################
    #############################################################################
    ##
    ## Default gridcoinresearch.conf location:
    ##
    ##  Win:   %AppData%\GridcoinResearch\
    ##  Linux: ~/.GridcoinResearch/
    ##  macOS: /Users/USERNAME/Library/Application/Support/GridcoinResearch/
    ##
    ## Single # lines are commands, remove the # in Front of the command to use it
    ## Double ## lines are comments
    ##
    #############################################################################
    ####################### Required Settings (All OS's) ########################
    #############################################################################

    ## Community provided list of addnodes available at https://github.com/gridcoin-community/Gridcoin-Wiki/wiki/List-of-Addnodes
    #~~~~~Copy & Paste Addnodes here~~~~~


    #~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ## BOINC account E-Mail
    ## Use blank or INVESTOR for Investor or Pool Mining
    ## Note the PrimaryCPID field is deprecated and ignored since the 4.0.5.0
    ## release.
    email=

    ## Port 32749/TCP open or forwarded required for Inbound Connections
    ## (Not required but highly recommended)
    #listen=1

    ## Required for Headless set-ups
    #daemon=1

    #############################################################################
    ############# RPC Settings for Remote Access and Headless Users #############
    ############ Warning: Set a Good Password and Secure Your System ############
    #############################################################################

    #server=1
    #rpcallowip=127.0.0.1
    #rpcallowip=<IP Address of Remote System>
    #rpcport=<Port for RPC Communication>
    #rpcuser=<A Username for RPC>
    #rpcpassword=<A STRONG Password for RPC>

    #############################################################################
    ######################## Optional BOINC settings ############################
    ########### (Required if BOINC installed to non-default location) ###########
    #############################################################################

    ## Windows (Note the double backslashes are necessary)
    #boincdatadir=C:\\ProgramData\\BOINC\\

    ## Linux
    #boincdatadir=/var/lib/boinc-client/

    ## macOS
    #boincdatadir=/Library/Application Support/BOINC Data/

    #############################################################################
    ######################## Optional Network settings ##########################
    #############################################################################

    ## Maximum number of inbound+outbound connections. Default 125
    #maxconnections=125
    ## Maximum number of outbound connections. Default 8
    #maxoutboundconnections=8
    ## Manually Set-up Ports
    #upnp=false
    #externalip=<Your IP Address>

    #############################################################################
    ############################## Other Entries ################################
    #############################################################################

    ## See detailed Other Entries description section below.

    #debug=true
    #debug=<category>

    #enablestakesplit=1
    #stakingefficiency=<percentage between 75 and 98, defaults to 90>
    #minstakesplitvalue=<value in GRC, minimum and defaults to 800>

    #enablesidestaking=1
    #sidestake=<address>,<allocation percentage>

## Addnodes

The list of addnodes you provide are the nodes that your client will
attempt to establish outbound connections with. The basic configuration
file does not include addnodes. A full current list of addnodes can be
found at https://github.com/gridcoin-community/Gridcoin-Wiki/wiki/List-of-Addnodes

If your system fails to sync, check your list of addnodes against the current list.
It is not required for all of the addnodes to be reachable, but at least one
reliable addnode, and preferably more than one that is reachable, should be
in your config file.

Ensure you don't have an addnode=your own ip, or you will end up banning
yourself (because when the node sends itself the first message, the
local time is far enough off of the network time (which it does not know
yet) so it will ban itself.

## Other Entries

Most of Gridcoin's config file flags and command line arguments are
taken directly from Bitcoin, and you can find a list which explains a
lot of these options here: <https://en.bitcoin.it/wiki/Running_Bitcoin>

**debug=true**
**debug=<category>**

Let your node receive tons of extra messages in debug.log. From the 4.1.0.0
release onward, logging can also be enabled by category. You can see a list
of categories by issuing the command "logging". Note that not all categories
are available yet, as the wallet is transiting from the traditional debug
flags to these categories.

Some Gridcoin specific other entries:

**enablestakesplit=1**
**stakingefficiency=<percentage between 75 and 98, defaults to 90>**
**minstakesplitvalue=<value in GRC, minimum and defaults to 800>**

enablestakesplit=1 will enable the automatic splitting of UTXO's in the
coinstake transaction (stake outputs). Zero is the default (disabled).

stakingefficiency=xx is an integer that specifies the desired staking
efficiency. This is constrained by the code to be between 75% and 98%,
in case an unreasonable value is provided.

minstakesplitvalue=xxx is an integer that specifies the minimum UTXO size
desired post split to provide a secondary control on UTXO size. If
difficulty drops and a high efficiency is specified, the efficiency alone
would split UTXO's into amounts smaller than the user desires. This will
prevent that from occurring. If a user specifies less than 800 GRC, then
the code uses 800 GRC. Note that the stake splitter uses a 160 block
averaging interval for calculating the difficulty to smooth out the
difficulty swings.

**enablesidestaking=1**
**sidestake=<address>,<allocation percentage>**

You can specify multiple sidetake entries, just like addnode or connect.
Note that the total number of outputs for the coinstake is limited
to 8 in block version 10+, and vout[0] must be empty, so that gives 7
usable outputs. One must always be reserved for the actual coinstake
output (return), so that leaves up to 6 usable outputs for rewards
distribution. You can specify more than six entries for sidestaking.
If more than six are specified, six entries per stake are randomly
chosen from the list.

Note that the total of all of the percentages can add up to less than
100%, in which cases the leftover reward will be returned back
to the staker on the coinstake(s).
