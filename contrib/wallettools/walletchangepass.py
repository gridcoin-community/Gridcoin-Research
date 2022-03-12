from jsonrpc import ServiceProxy
import getpass
access = ServiceProxy("http://127.0.0.1:8332")
pwd = getpass.getpass(prompt="Enter old wallet passphrase: ")
pwd2 = getpass.getpass(prompt="Enter new wallet passphrase: ")
access.walletpassphrasechange(pwd, pwd2)
