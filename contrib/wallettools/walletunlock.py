from jsonrpc import ServiceProxy
import getpass
access = ServiceProxy("http://127.0.0.1:8332")
pwd = getpass.getpass(prompt="Enter wallet passphrase: ")
access.walletpassphrase(pwd, 60)
