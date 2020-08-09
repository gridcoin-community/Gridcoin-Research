<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>AboutDialog</name>
    <message>
        <location filename="../forms/aboutdialog.ui" line="+14"/>
        <source>About Gridcoin</source>
        <translation>关于格雷德币</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>&lt;b&gt;Gridcoin&lt;/b&gt; </source>
        <translation>&lt;b&gt;格雷德币&lt;/b&gt; </translation>
    </message>
    <message>
        <location line="+58"/>
        <source>
This is experimental software.

Distributed under the MIT/X11 software license, see the accompanying file COPYING or https://opensource.org/licenses/mit-license.php.

This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (https://www.openssl.org/) and cryptographic software written by Eric Young (eay@cryptsoft.com) and UPnP software written by Thomas Bernard.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>AddressBookPage</name>
    <message>
        <location filename="../forms/addressbookpage.ui" line="+14"/>
        <source>Address Book</source>
        <translation>地址簿</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>These are your Gridcoin addresses for receiving payments. You may want to give a different one to each sender so you can keep track of who is paying you.</source>
        <translation>这是您用来接收支付的格雷德币地址列表。为不同的支付方建立不同的地址以便于了解支付来源。</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Double-click to edit address or label</source>
        <translation>双击编辑地址或标签</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>Create a new address</source>
        <translation>创建新地址</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;New</source>
        <translation>新建(&amp;N)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Copy the currently selected address to the system clipboard</source>
        <translation>复制当前选中的地址到系统剪贴板</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Copy</source>
        <translation>复制(&amp;C)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Show &amp;QR Code</source>
        <translation>显示二维码(&amp;Q)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Sign a message to prove you own a Gridcoin address</source>
        <translation>对信息进行签名以证明您对该格雷德币地址的所有权</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Sign &amp;Message</source>
        <translation>签名(&amp;M)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Verify a message to ensure it was signed with a specified Gridcoin address</source>
        <translation>验证信息以保证其经过指定格雷德币地址的签名</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Verify Message</source>
        <translation>验证消息(&amp;V)</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Delete the currently selected address from the list</source>
        <translation>从列表中删除选中的地址</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Delete</source>
        <translation>删除(&amp;D)</translation>
    </message>
    <message>
        <location filename="../addressbookpage.cpp" line="+65"/>
        <source>Copy &amp;Label</source>
        <translation>复制标签(&amp;L)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>&amp;Edit</source>
        <translation>编辑(&amp;E)</translation>
    </message>
    <message>
        <location line="+249"/>
        <source>Export Address Book Data</source>
        <translation>导出地址簿数据</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Error exporting</source>
        <translation>导出时发生错误</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Could not write to file %1.</source>
        <translation>无法写入文件 %1 。</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>Comma separated file (*.csv)</source>
        <translation>逗号分隔文件 (*.csv)</translation>
    </message>
</context>
<context>
    <name>AddressTableModel</name>
    <message>
        <location filename="../addresstablemodel.cpp" line="+145"/>
        <source>Label</source>
        <translation>标签</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>(no label)</source>
        <translation>(没有标签)</translation>
    </message>
</context>
<context>
    <name>AskPassphraseDialog</name>
    <message>
        <location filename="../forms/askpassphrasedialog.ui" line="+26"/>
        <source>Passphrase Dialog</source>
        <translation>密码对话框</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>Enter passphrase</source>
        <translation>输入密码</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>New passphrase</source>
        <translation>新密码</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Repeat new passphrase</source>
        <translation>重复新密码</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Serves to disable the trivial sendmoney when OS account compromised. Provides no real security.</source>
        <translation>在系统允许的情况下用于防止sendmoney欺诈，并未提供真正的安全防护措施。</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>For staking only</source>
        <translation>仅用于权益增值</translation>
    </message>
    <message>
        <location filename="../askpassphrasedialog.cpp" line="+37"/>
        <source>Enter the new passphrase to the wallet.&lt;br/&gt;Please use a passphrase of &lt;b&gt;ten or more random characters&lt;/b&gt;, or &lt;b&gt;eight or more words&lt;/b&gt;.</source>
        <translation>输入钱包的新密码。&lt;br/&gt;使用的密码请至少包含&lt;b&gt;10个以上随机字符&lt;/&gt;，或者是&lt;b&gt;8个以上的单词&lt;/b&gt;。</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Encrypt wallet</source>
        <translation>加密钱包</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>This operation needs your wallet passphrase to unlock the wallet.</source>
        <translation>此操作需要您首先使用密码解锁该钱包。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Unlock wallet</source>
        <translation>解锁钱包</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>This operation needs your wallet passphrase to decrypt the wallet.</source>
        <translation>该操作需要您首先使用密码解密钱包。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Decrypt wallet</source>
        <translation>解密钱包</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Change passphrase</source>
        <translation>更改密码</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Enter the old and new passphrase to the wallet.</source>
        <translation>请输入该钱包的旧密码与新密码。</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>Warning: If you encrypt your wallet and lose your passphrase, you will &lt;b&gt;LOSE ALL OF YOUR COINS&lt;/b&gt;!</source>
        <translation>警告：如果您丢失了加密该钱包的密码，其中所有的格雷德币将会丢失！</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Gridcoin will close now to finish the encryption process. Remember that encrypting your wallet cannot fully protect your coins from being stolen by malware infecting your computer.</source>
        <translation>格雷德币客户端即将关闭以完成加密过程。请记住，加密钱包并不能完全防止您的电子货币被入侵您计算机的木马软件盗窃。</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>Confirm wallet encryption</source>
        <translation>确认加密钱包</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Are you sure you wish to encrypt your wallet?</source>
        <translation>您确定需要为钱包加密吗？</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+60"/>
        <source>Wallet encrypted</source>
        <translation>钱包已加密</translation>
    </message>
    <message>
        <location line="-54"/>
        <source>IMPORTANT: Any previous backups you have made of your wallet file should be replaced with the newly generated, encrypted wallet file. For security reasons, previous backups of the unencrypted wallet file will become useless as soon as you start using the new, encrypted wallet.</source>
        <translation>重要提示：您以前备份的钱包文件应该替换成最新生成的加密钱包文件（重新备份）。从安全性上考虑，您以前备份的未加密的钱包文件，在您使用新的加密钱包后将无效，请重新备份。</translation>
    </message>
    <message>
        <location line="+9"/>
        <location line="+7"/>
        <location line="+44"/>
        <location line="+6"/>
        <source>Wallet encryption failed</source>
        <translation>钱包加密失败</translation>
    </message>
    <message>
        <location line="-56"/>
        <source>Wallet encryption failed due to an internal error. Your wallet was not encrypted.</source>
        <translation>由于一个本地错误，加密钱包的操作已经失败。您的钱包没能被加密。</translation>
    </message>
    <message>
        <location line="+7"/>
        <location line="+50"/>
        <source>The supplied passphrases do not match.</source>
        <translation>密码不匹配。</translation>
    </message>
    <message>
        <location line="-38"/>
        <source>Wallet unlock failed</source>
        <translation>钱包解锁失败</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+12"/>
        <location line="+19"/>
        <source>The passphrase entered for the wallet decryption was incorrect.</source>
        <translation>用于解密钱包的密码不正确。</translation>
    </message>
    <message>
        <location line="-20"/>
        <source>Wallet decryption failed</source>
        <translation>钱包解密失败</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Wallet passphrase was successfully changed.</source>
        <translation>修改钱包密码成功。</translation>
    </message>
    <message>
        <location line="+48"/>
        <location line="+24"/>
        <source>Warning: The Caps Lock key is on!</source>
        <translation>警告：大写锁定键处于打开状态！</translation>
    </message>
</context>
<context>
    <name>BitcoinGUI</name>
    <message>
        <location filename="../bitcoingui.cpp" line="+119"/>
        <source>Gridcoin</source>
        <translation>格雷德币</translation>
    </message>
    <message>
        <location line="+123"/>
        <source>Send coins to a Gridcoin address</source>
        <translation>向指定的地址发送格雷德币</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Show the list of addresses for receiving payments</source>
        <translation>显示用于接收支付的地址列表</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>&amp;Address Book</source>
        <translation>地址簿(&amp;A)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Edit the list of stored addresses and labels</source>
        <translation>管理已储存的地址和标签</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>&amp;Block Explorer</source>
        <translation>&amp;区块浏览器</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Block Explorer</source>
        <translation>区块浏览器</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Exchange</source>
        <translation>交易所</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+4"/>
        <source>Web Site</source>
        <translation>网站</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>&amp;Web Site</source>
        <translation>&amp;网站</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>&amp;GRC Chat Room</source>
        <translation>&amp;GRC聊天室</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>GRC Chatroom</source>
        <translation>GRC聊天室</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;BOINC</source>
        <translation>&amp;BOINC</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Gridcoin rewards distributed computing with BOINC</source>
        <translation>格雷德币给BOINC分布式计算参与者提供报酬</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>&amp;About Gridcoin</source>
        <translation>关于格雷德币(&amp;A)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Show information about Gridcoin</source>
        <translation>显示关于格雷德币的信息</translation>
    </message>
    <message>
        <location line="+8"/>
        <location line="+582"/>
        <source>New User Wizard</source>
        <translation>新用户向导</translation>
    </message>
    <message>
        <location line="-639"/>
        <source>&amp;Voting</source>
        <translation>&amp;投票</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Voting</source>
        <translation>投票</translation>
    </message>
    <message>
        <location line="+51"/>
        <source>&amp;Diagnostics</source>
        <translation>&amp;诊断</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Diagnostics</source>
        <translation>诊断</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;New User Wizard</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Modify configuration options for Gridcoin</source>
        <translation>更改格雷德币设置选项</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Encrypt or decrypt wallet</source>
        <translation>加密/解密钱包</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>&amp;Backup Wallet/Config...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Backup wallet/config to another location</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Unlock Wallet...</source>
        <translation>解锁钱包(&amp;U)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Unlock wallet</source>
        <translation>解锁钱包</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>&amp;Lock Wallet</source>
        <translation>锁定钱包(&amp;L)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Lock wallet</source>
        <translation>锁定钱包</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Sign &amp;message...</source>
        <translation>消息签名(&amp;M)...</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Export...</source>
        <translation>导出(&amp;E)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Export the data in the current tab to a file</source>
        <translation>导出当前标签页的数据</translation>
    </message>
    <message>
        <location line="+83"/>
        <source>&amp;Community</source>
        <translation>社区</translation>
    </message>
    <message>
        <location line="+111"/>
        <location line="+9"/>
        <source>[testnet]</source>
        <translation>[测试网络]</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+69"/>
        <source>Gridcoin client</source>
        <translation>格雷德币客户端</translation>
    </message>
    <message>
        <location line="+83"/>
        <source>%1 active connection(s) to Gridcoin network</source>
        <translation type="unfinished">与格雷德币网络建立了 %1 个可用连接</translation>
    </message>
    <message>
        <location line="+59"/>
        <source>Last received block was generated %1.</source>
        <translation>最新收到的区块产生于 %1。</translation>
    </message>
    <message>
        <location line="+68"/>
        <source>This transaction is over the size limit.  You can still send it for a fee of %1, which goes to the nodes that process your transaction and helps to support the network.  Do you want to pay the fee?</source>
        <translation>交易数据量超限。您仍可发送这笔交易，只需为节点确认您的交易以及支持网络运行提供%1的交易费。您希望付费吗？</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Confirm transaction fee</source>
        <translation>确认交易费</translation>
    </message>
    <message>
        <source>Please enter your boinc E-mail address, or click &lt;Cancel&gt; to skip for now:</source>
        <translation type="vanished">请输入您的BOINC邮箱地址，或按Cancel键先跳过此步骤</translation>
    </message>
    <message>
        <location line="+85"/>
        <source>Created new Configuration File Successfully. </source>
        <translation>c成功创建新配置文件。</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>New Account Created - Welcome Aboard!</source>
        <translation>已创建新账号-欢迎上船！</translation>
    </message>
    <message>
        <source>To get started with Boinc, run the boinc client, choose projects, then populate the gridcoinresearch.conf file in %appdata%\GridcoinResearch with your boinc e-mail address.  To run this wizard again, please delete the gridcoinresearch.conf file. </source>
        <translation type="vanished">若要开始BOINC计算，请运行BOINC客户端，选择项目，然后在%appdata%\GridcoinResearch中的文件gridcoinresearch.conf中写入您的BOINC邮箱地址。如果要再次运行此向导，请删除gridcoinresearch.conf文件。</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>New User Wizard - Skipped</source>
        <translation>新用户向导 - 已跳过</translation>
    </message>
    <message>
        <source>Attention! - Boinc Path Error!</source>
        <translation type="vanished">注意！—— BOINC路径错误！</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>Date: %1
Amount: %2
Type: %3
Address: %4</source>
        <translation type="unfinished">日期: %1
金额: %2
类型: %3
地址: %4
 {1
?} {2
?} {3
?} {4?}</translation>
    </message>
    <message>
        <location line="+446"/>
        <source>Staking.&lt;br&gt;Your weight is %1&lt;br&gt;Network weight is %2&lt;br&gt;&lt;b&gt;Estimated&lt;/b&gt; time to earn reward is %3.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Not staking; %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="-298"/>
        <location line="+15"/>
        <source>URI handling</source>
        <translation>URI处理</translation>
    </message>
    <message>
        <location line="-15"/>
        <location line="+15"/>
        <source>URI can not be parsed! This can be caused by an invalid Gridcoin address or malformed URI parameters.</source>
        <translation>无法解析 URI 地址！可能是因为格雷德币地址无效，或是 URI 参数格式错误。</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Wallet is &lt;b&gt;encrypted&lt;/b&gt; and currently %1 </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+0"/>
        <source>&lt;b&gt;unlocked for staking only&lt;/b&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+0"/>
        <source>&lt;b&gt;fully unlocked&lt;/b&gt;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+35"/>
        <source>Backup Wallet</source>
        <translation>备份钱包</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Wallet Data (*.dat)</source>
        <translation>钱包数据(*.dat)</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+6"/>
        <source>Backup Failed</source>
        <translation>备份失败</translation>
    </message>
    <message>
        <location line="-6"/>
        <location line="+6"/>
        <source>There was an error trying to save the wallet data to the new location.</source>
        <translation>存储地址列表到新位置时发生错误。</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>Backup Config</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Wallet Config (*.conf)</source>
        <translation type="unfinished"></translation>
    </message>
    <message numerus="yes">
        <location line="+179"/>
        <source>%n second(s)</source>
        <translation type="unfinished">
            <numerusform>%n 秒</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n minute(s)</source>
        <translation type="unfinished">
            <numerusform>%n 分钟</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n hour(s)</source>
        <translation type="unfinished">
            <numerusform>%n 小时</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n day(s)</source>
        <translation type="unfinished">
            <numerusform>%n 天</numerusform>
        </translation>
    </message>
    <message>
        <location line="-1154"/>
        <source>&amp;Overview</source>
        <translation>概况(&amp;O)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Show general overview of wallet</source>
        <translation>显示钱包概况</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>&amp;Transactions</source>
        <translation>交易记录(&amp;T)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Browse transaction history</source>
        <translation>查看交易历史</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>E&amp;xit</source>
        <translation>退出(&amp;X)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Quit application</source>
        <translation>退出程序</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>&amp;Options...</source>
        <translation>选项(&amp;O)...</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>&amp;Encrypt Wallet...</source>
        <translation>加密钱包(&amp;E)...</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>&amp;Change Passphrase...</source>
        <translation>更改密码(&amp;C)...</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Change the passphrase used for wallet encryption</source>
        <translation>更改钱包加密口令</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>&amp;Debug window</source>
        <translation>调试窗口(&amp;D)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Open debugging and diagnostic console</source>
        <translation>打开调试和诊断控制台</translation>
    </message>
    <message>
        <location line="-5"/>
        <source>&amp;Verify message...</source>
        <translation>验证消息(&amp;V)...</translation>
    </message>
    <message>
        <location line="-218"/>
        <source>Wallet</source>
        <translation>钱包</translation>
    </message>
    <message>
        <location line="+122"/>
        <source>&amp;Send</source>
        <translation>发送(&amp;S)</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>&amp;Receive</source>
        <translation>接收(&amp;R)</translation>
    </message>
    <message>
        <location line="+78"/>
        <source>&amp;Show / Hide</source>
        <translation>显示 / 隐藏(&amp;S)</translation>
    </message>
    <message>
        <location line="+78"/>
        <source>&amp;File</source>
        <translation>文件(&amp;F)</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>&amp;Settings</source>
        <translation>设置(&amp;S)</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>&amp;Help</source>
        <translation>帮助(&amp;H)</translation>
    </message>
    <message numerus="yes">
        <location line="+276"/>
        <source>Processed %n block(s) of transaction history.</source>
        <translation>
            <numerusform>已处理 %n 个交易历史数据块。</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+13"/>
        <source>%n second(s) ago</source>
        <translation type="unfinished">
            <numerusform>%n 秒前</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n minute(s) ago</source>
        <translation type="unfinished">
            <numerusform>%n 分钟前</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n hour(s) ago</source>
        <translation type="unfinished">
            <numerusform>%n 小时前</numerusform>
        </translation>
    </message>
    <message numerus="yes">
        <location line="+4"/>
        <source>%n day(s) ago</source>
        <translation type="unfinished">
            <numerusform>%n 天前</numerusform>
        </translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Up to date</source>
        <translation>已是最新</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Catching up...</source>
        <translation>更新中...</translation>
    </message>
    <message>
        <location line="+155"/>
        <source>Please enter your BOINC E-mail address, or click &lt;Cancel&gt; to skip for now:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+21"/>
        <source>To get started with BOINC, run the BOINC client, choose projects, then populate the gridcoinresearch.conf file in %appdata%\GridcoinResearch with your BOINC e-mail address.  To run this wizard again, please delete the gridcoinresearch.conf file. </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Attention! - BOINC Path Error!</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Sent transaction</source>
        <translation>发送交易</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Incoming transaction</source>
        <translation>流入交易</translation>
    </message>
    <message>
        <location line="+480"/>
        <location line="+17"/>
        <location line="+9"/>
        <source>none</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Scraper: waiting on wallet to sync.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Scraper: superblock not needed - inactive.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Scraper: downloading and processing stats.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Scraper: Convergence achieved, date/time %1 UTC. 
Project(s) excluded: %2. 
Scrapers included: %3. 
Scraper(s) excluded: %4. 
Scraper(s) not publishing: %5.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Scraper: Convergence achieved, date/time %1 UTC. 
 Project(s) excluded: %2.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Scraper: No convergence able to be achieved. Will retry in a few minutes.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="-356"/>
        <source>Wallet is &lt;b&gt;encrypted&lt;/b&gt; and currently &lt;b&gt;locked&lt;/b&gt;</source>
        <translation>钱包已被&lt;b&gt;加密&lt;/b&gt;，当前为&lt;b&gt;锁定&lt;/b&gt;状态</translation>
    </message>
    <message>
        <location filename="../bitcoin.cpp" line="+176"/>
        <source>A fatal error occurred. Gridcoin can no longer continue safely and will quit.</source>
        <translation>发生致命错误。格雷德币不再安全，即将退出。</translation>
    </message>
</context>
<context>
    <name>ClientModel</name>
    <message>
        <location filename="../clientmodel.cpp" line="+123"/>
        <source>Network Alert</source>
        <translation>网络警报</translation>
    </message>
</context>
<context>
    <name>CoinControlDialog</name>
    <message>
        <location filename="../forms/coincontroldialog.ui" line="+14"/>
        <source>Coin Control</source>
        <translation>货币支配</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>Quantity:</source>
        <translation>数量：</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Bytes:</source>
        <translation>比特数：</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>Amount:</source>
        <translation>金额：</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Priority:</source>
        <translation>优先权：</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>Fee:</source>
        <translation>费用：</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Low Output:</source>
        <translation>输出：</translation>
    </message>
    <message>
        <location line="+144"/>
        <source>Tree &amp;mode</source>
        <translation>树形模式</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>&amp;List mode</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+50"/>
        <source>Label</source>
        <translation>标签</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Priority</source>
        <translation>优先权</translation>
    </message>
    <message>
        <location line="-191"/>
        <source>After Fee:</source>
        <translation>加上费用后:</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Change:</source>
        <translation>零钱 : </translation>
    </message>
    <message>
        <location line="+63"/>
        <source>(un)select all</source>
        <translation>(取消)全选</translation>
    </message>
    <message>
        <location line="+74"/>
        <source>Amount</source>
        <translation>金额</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Date</source>
        <translation>日期</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Confirmations</source>
        <translation>确认数</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Confirmed</source>
        <translation>已确认</translation>
    </message>
    <message>
        <location filename="../coincontroldialog.cpp" line="+36"/>
        <source>Copy address</source>
        <translation>复制地址</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy label</source>
        <translation>复制标签</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+26"/>
        <source>Copy amount</source>
        <translation>复制金额</translation>
    </message>
    <message>
        <location line="-25"/>
        <source>Copy transaction ID</source>
        <translation>复制转账ID</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Copy priority</source>
        <translation>复制优先权</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy low output</source>
        <translation>复制低级输出</translation>
    </message>
    <message>
        <location line="+318"/>
        <source>highest</source>
        <translation>极高</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>high</source>
        <translation>高</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>medium-high</source>
        <translation>中高</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>medium</source>
        <translation>中等</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>low-medium</source>
        <translation>中低</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>low</source>
        <translation>低</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>lowest</source>
        <translation>极低</translation>
    </message>
    <message>
        <location line="+155"/>
        <source>DUST</source>
        <translation>DUST</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>This label turns red, if the transaction size is bigger than 10000 bytes.

 This means a fee of at least %1 per kb is required.

 Can vary +/- 1 Byte per input.</source>
        <translation>若转账数据量大于10000比特，本标签会变红。
            
这意味着要求至少每Kb%1的费用。
            
每次输入可能有+/-1字节的波动。</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Transactions with higher priority get more likely into a block.

This label turns red, if the priority is smaller than &quot;medium&quot;.

 This means a fee of at least %1 per kb is required.</source>
        <translation></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This label turns red, if any recipient receives an amount smaller than %1.

 This means a fee of at least %2 is required. 

 Amounts below 0.546 times the minimum relay fee are shown as DUST.</source>
        <translation>若任意收款方收到的金额小于%1，本标签会变红。
            
这意味着要求至少%2的费用。
            
金额少于最小广播费用的0.546倍的交易将显示为DUST。</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This label turns red, if the change is smaller than %1.

 This means a fee of at least %2 is required.</source>
        <translation>若更改少于%1，本标签会变红。
这意味着要求至少%2的费用。</translation>
    </message>
    <message>
        <location line="-501"/>
        <source>Copy quantity</source>
        <translation>复制数量</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Copy fee</source>
        <translation>复制费用</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy after fee</source>
        <translation>复制转账金额加费用</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy bytes</source>
        <translation>复制比特数</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Copy change</source>
        <translation>复制零钱</translation>
    </message>
    <message>
        <location line="+481"/>
        <source>yes</source>
        <translation>是</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>no</source>
        <translation>否</translation>
    </message>
    <message>
        <location line="+50"/>
        <location line="+63"/>
        <source>(no label)</source>
        <translation>(无标签)</translation>
    </message>
    <message>
        <location line="-9"/>
        <source>change from %1 (%2)</source>
        <translation>来自%1的零钱 (%2)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>(change)</source>
        <translation>(零钱)</translation>
    </message>
</context>
<context>
    <name>DiagnosticsDialog</name>
    <message>
        <location filename="../forms/diagnosticsdialog.ui" line="+14"/>
        <location line="+210"/>
        <source>Diagnostics</source>
        <translation type="unfinished">诊断</translation>
    </message>
    <message>
        <location line="-157"/>
        <source>Verify CPID is in Neural Network</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+51"/>
        <source>Verify BOINC path</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Verify CPID has RAC</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Verify wallet is synced</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Verify CPID is valid</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+51"/>
        <source>Find PrimaryCPID (Windows Only)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+41"/>
        <source>Verify clock</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Verify connections to seeds</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Verify connections to network</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Verify TCP port 32749</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Check client version</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+48"/>
        <source>Close</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Test</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EditAddressDialog</name>
    <message>
        <location filename="../forms/editaddressdialog.ui" line="+14"/>
        <source>Edit Address</source>
        <translation>编辑地址</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>&amp;Label</source>
        <translation>标签(&amp;L)</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>The label associated with this address book entry</source>
        <translation>与此地址条目关联的标签</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>The address associated with this address book entry. This can only be modified for sending addresses.</source>
        <translation>该地址与地址簿中的条目已关联，无法作为发送地址编辑。</translation>
    </message>
    <message>
        <location line="-10"/>
        <source>&amp;Address</source>
        <translation>地址(&amp;A)</translation>
    </message>
    <message>
        <location filename="../editaddressdialog.cpp" line="+20"/>
        <source>New receiving address</source>
        <translation>新接收地址</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>New sending address</source>
        <translation>新发送地址</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Edit receiving address</source>
        <translation>编辑接收地址</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Edit sending address</source>
        <translation>编辑发送地址</translation>
    </message>
    <message>
        <location line="+71"/>
        <source>The entered address &quot;%1&quot; is not a valid Gridcoin address.</source>
        <translation>您输入的 &quot;%1&quot; 不是合法的格雷德币地址。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The entered address &quot;%1&quot; is already in the address book.</source>
        <translation>输入的地址 &quot;%1&quot; 已经存在于地址薄。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Could not unlock wallet.</source>
        <translation>无法解锁钱包</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>New key generation failed.</source>
        <translation>密钥创建失败。</translation>
    </message>
</context>
<context>
    <name>GUIUtil::HelpMessageBox</name>
    <message>
        <location filename="../guiutil.cpp" line="+507"/>
        <source>version</source>
        <translation>版本</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Usage:</source>
        <translation>用途：</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>command-line options</source>
        <translation>命令行选项</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>UI options</source>
        <translation>UI选项</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Set language, for example &quot;de_DE&quot; (default: system locale)</source>
        <translation>设置语言, 例如 &quot;de_DE&quot; (缺省: 系统语言)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Start minimized</source>
        <translation>启动时最小化</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Show splash screen on startup (default: 1)</source>
        <translation>启动时显示版权页 (缺省: 1)</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Gridcoin-Qt</source>
        <translation>Gridcoin-Qt</translation>
    </message>
</context>
<context>
    <name>NewPollDialog</name>
    <message>
        <location filename="../votingdialog.cpp" line="+876"/>
        <location line="+96"/>
        <source>Create Poll</source>
        <translation>创建民意调查</translation>
    </message>
    <message>
        <location line="-81"/>
        <source>Title: </source>
        <translation>标题：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Days: </source>
        <translation>天数：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Question: </source>
        <translation>问题：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Discussion URL: </source>
        <translation>讨论URL：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Share Type: </source>
        <translation>计票方式：</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Add Item</source>
        <translation>增加条目</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Remove Item</source>
        <translation>移除条目</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Clear All</source>
        <translation>全部清除</translation>
    </message>
    <message>
        <location line="+36"/>
        <source>Creating poll failed! Title is missing.</source>
        <translation>创建民意调查失败！缺少标题。</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Creating poll failed! Days value is missing.</source>
        <translation>创建民意调查失败！缺少天数值。</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Creating poll failed! Polls can not last longer than 180 days.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Creating poll failed! Question is missing.</source>
        <translation>创建民意调查失败！缺少问题。</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Creating poll failed! URL is missing.</source>
        <translation>创建民意调查失败！缺少URL。</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Creating poll failed! Answer is missing.</source>
        <translation>创建民意调查失败！缺少备选答案。</translation>
    </message>
</context>
<context>
    <name>OptionsDialog</name>
    <message>
        <location filename="../forms/optionsdialog.ui" line="+14"/>
        <source>Options</source>
        <translation>选项</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>&amp;Main</source>
        <translation>主选项(&amp;M)</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Optional transaction fee per kB that helps make sure your transactions are processed quickly. Most transactions are 1 kB. Fee 0.01 recommended.</source>
        <translation>建议支付交易费用，有助于您的交易得到尽快处理。 绝大多数交易的字节数为 1 KB。建议支付0.01个格雷德币。</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Pa&amp;y transaction fee</source>
        <translation>支付转账费用</translation>
    </message>
    <message>
        <source>Reserved amount does not participate in staking and is therefore spendable at any time.</source>
        <translation type="vanished">保留金额不参与权益增值,在任何时刻均可消费。</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>Reser&amp;ve</source>
        <translation>保留</translation>
    </message>
    <message>
        <location line="+31"/>
        <source>Automatically start Gridcoin after logging in to the system.</source>
        <translation>登录系统后自动启动格雷德币。</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Start Gridcoin on system login</source>
        <translation>系统登录时启动格雷德币(&amp;S)</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Detach block and address databases at shutdown. This means they can be moved to another data directory, but it slows down shutdown. The wallet is always detached.</source>
        <translation>关闭时分开区块数据库和地址数据库。这意味着您可以将数据库文件移动至其他文件夹，但关闭会变慢。钱包文件始终是分开的。</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Detach databases at shutdown</source>
        <translation>关闭时分开区块数据库和地址数据库(&amp;D)</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>Automatically open the Gridcoin client port on the router. This only works when your router supports UPnP and it is enabled.</source>
        <translation>自动在路由器中打开比特币端口。只有当您的路由器支持并开启 UPnP 选项时此功能才有效。</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Connect to the Gridcoin network through a SOCKS proxy (e.g. when connecting through Tor).</source>
        <translation>通过一个SOCKS4代理连接到格雷德币网络 (如使用Tor连接时)。</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Connect through SOCKS proxy:</source>
        <translation>通过SOCKS4代理连接(&amp;C)</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Pro&amp;xy IP:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+19"/>
        <source>IP address of the proxy (e.g. 127.0.0.1)</source>
        <translation>代理服务器的IP地址(例如127.0.0.1)</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>SOCKS &amp;Version:</source>
        <translation>Socks版本(&amp;V)：</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>SOCKS version of the proxy (e.g. 5)</source>
        <translation>代理服务器的Socks版本(例如5)</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>Minimize instead of exit the application when the window is closed. When this option is enabled, the application will be closed only after selecting Quit in the menu.</source>
        <translation>当窗口关闭时程序最小化而不是退出。当使用该选项时，程序只能通过在菜单中选择退出来关闭。</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>The user interface language can be set here. This setting will take effect after restarting Gridcoin.</source>
        <translation>设置语言选项。需重启客户端软件才能生效。</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>Style:</source>
        <translation>风格：</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Choose a stylesheet to change the look of the wallet.</source>
        <translation>选择样式表以更改钱包外观。</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Whether to show Gridcoin addresses in the transaction list or not.</source>
        <translation>在转账列表中是否显示格雷德币地址。</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Display addresses in transaction list</source>
        <translation>在转账列表中显示地址(&amp;D)</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Display coin &amp;control features (advanced users only!)</source>
        <translation>显示货币支配特征(仅限专家！)</translation>
    </message>
    <message>
        <location line="+85"/>
        <source>&amp;Apply</source>
        <translation>应用(&amp;A)</translation>
    </message>
    <message>
        <location line="-339"/>
        <source>&amp;Network</source>
        <translation>网络(&amp;N)</translation>
    </message>
    <message>
        <location line="-80"/>
        <source>Reserved amount secures a balance in wallet that can be spendable at anytime. However reserve will secure utxo(s) of any size to respect this setting.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+89"/>
        <source>Map port using &amp;UPnP</source>
        <translation>使用 &amp;UPnP 映射端口</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>&amp;Port:</source>
        <translation>端口(&amp;P)：</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>Port of the proxy (e.g. 9050)</source>
        <translation>代理端口(例如9050)</translation>
    </message>
    <message>
        <location line="+56"/>
        <source>&amp;Window</source>
        <translation>窗口(&amp;W)</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Show only a tray icon after minimizing the window.</source>
        <translation>最小化窗口后只显示一个托盘标志</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>&amp;Minimize to the tray instead of the taskbar</source>
        <translation>最小化到托盘而非任务栏(&amp;M)</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>M&amp;inimize on close</source>
        <translation>关闭时最小化(&amp;I)</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>&amp;Display</source>
        <translation>显示(&amp;D)</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>User Interface &amp;language:</source>
        <translation>使用界面语言(&amp;L)：</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>&amp;Unit to show amounts in:</source>
        <translation>格雷德币金额单位(&amp;U)：</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Choose the default subdivision unit to show in the interface and when sending coins.</source>
        <translation>选择显示及发送货币时使用的最小单位</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>Whether to show coin control features or not.</source>
        <translation>是否显示货币支配特征。</translation>
    </message>
    <message>
        <location line="+71"/>
        <source>&amp;OK</source>
        <translation>确定(&amp;O)</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>&amp;Cancel</source>
        <translation>取消(&amp;C)</translation>
    </message>
    <message>
        <location filename="../optionsdialog.cpp" line="+55"/>
        <source>default</source>
        <translation>默认</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Native</source>
        <translation>本地</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>Light</source>
        <translation>轻</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Dark</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+125"/>
        <location line="+9"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location line="-9"/>
        <location line="+9"/>
        <source>This setting will take effect after restarting Gridcoin.</source>
        <translation>这个设置将在格雷德币重启后生效。</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>The supplied proxy address is invalid.</source>
        <translation>提供的代理地址无效。</translation>
    </message>
</context>
<context>
    <name>OverviewPage</name>
    <message>
        <location filename="../forms/overviewpage.ui" line="+32"/>
        <source>Form</source>
        <translation>表单</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>Wallet</source>
        <translation>钱包</translation>
    </message>
    <message>
        <location line="+10"/>
        <location line="+401"/>
        <source>The displayed information may be out of date. Your wallet automatically synchronizes with the Gridcoin network after a connection is established, but this process has not completed yet.</source>
        <translation>现在显示的消息可能是过期的. 在连接上格雷德币网络节点后，您的钱包将自动与网络同步，但是这个过程还没有完成。</translation>
    </message>
    <message>
        <location line="-339"/>
        <source>Stake</source>
        <translation>权益增值</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Total number of coins that are staking, and do not yet count toward the current balance</source>
        <translation></translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Unconfirmed</source>
        <translation>未确认</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Total of transactions that have yet to be confirmed, and do not yet count toward the current balance</source>
        <translation>尚未确认的交易总额, 未计入当前余额</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Total mined coins that have not yet matured.</source>
        <translation>全部已挖到而未成熟的货币。</translation>
    </message>
    <message>
        <location line="+88"/>
        <source>Blocks:</source>
        <translation>区块数：</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Difficulty:</source>
        <translation>难度：</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Net Weight:</source>
        <translation>全网权重：</translation>
    </message>
    <message>
        <location line="+258"/>
        <source>Error Messages:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="-234"/>
        <source>Magnitude:</source>
        <translation>权重</translation>
    </message>
    <message>
        <source>Project:</source>
        <translation type="vanished">项目：</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>CPID:</source>
        <translation>CPID：</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Status:</source>
        <translation>状态：</translation>
    </message>
    <message>
        <location line="+203"/>
        <source>Current Poll:</source>
        <translation>当前民意调查：</translation>
    </message>
    <message>
        <location line="-439"/>
        <source>Available:</source>
        <translation>可使用的余额：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Your current spendable balance</source>
        <translation>您当前可使用的余额</translation>
    </message>
    <message>
        <location line="+59"/>
        <source>Immature:</source>
        <translation>未成熟的：</translation>
    </message>
    <message>
        <location line="+39"/>
        <source>Total:</source>
        <translation>总计：</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Your current total balance</source>
        <translation>您当前的总余额</translation>
    </message>
    <message>
        <location line="+100"/>
        <source>Coin Weight:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+128"/>
        <source>Recent transactions</source>
        <translation></translation>
    </message>
    <message>
        <location filename="../overviewpage.cpp" line="+128"/>
        <location line="+1"/>
        <source>out of sync</source>
        <translation>未完成同步</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../bitcoin.cpp" line="+182"/>
        <source>%1 didn&apos;t yet exit safely...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../guiutil.cpp" line="-461"/>
        <source>N/A</source>
        <translation type="unfinished">不可用</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>%1 ms</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+30"/>
        <source>%1 s</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="-24"/>
        <source>%1 B</source>
        <translation type="unfinished">%1 B</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 KB</source>
        <translation type="unfinished">%1 KB</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 MB</source>
        <translation type="unfinished">%1 MB</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 GB</source>
        <translation type="unfinished">%1 GB</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>%1 d</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 h</source>
        <translation type="unfinished">%1 时</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 m</source>
        <translation type="unfinished">%1 分</translation>
    </message>
    <message>
        <location line="+43"/>
        <source>None</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>QRCodeDialog</name>
    <message>
        <location filename="../forms/qrcodedialog.ui" line="+14"/>
        <source>QR Code Dialog</source>
        <translation>二维码对话框</translation>
    </message>
    <message>
        <location line="+62"/>
        <source>Request Payment</source>
        <translation>要求支付</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Label:</source>
        <translation>标签：</translation>
    </message>
    <message>
        <location line="+19"/>
        <source>Message:</source>
        <translation>消息：</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>Amount:</source>
        <translation>金额:</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>&amp;Save As...</source>
        <translation>另存为(&amp;S)...</translation>
    </message>
    <message>
        <location filename="../qrcodedialog.cpp" line="+62"/>
        <source>Error encoding URI into QR Code.</source>
        <translation>将URI编码为二维码时出现错误。</translation>
    </message>
    <message>
        <location line="+40"/>
        <source>The entered amount is invalid, please check.</source>
        <translation>输入的金额无效，请检查。</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Resulting URI too long, try to reduce the text for label / message.</source>
        <translation>得到的URI超长，请尝试缩减标签/消息中的文字。</translation>
    </message>
    <message>
        <location line="+25"/>
        <source>Save QR Code</source>
        <translation>保存二维码</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>PNG Images (*.png)</source>
        <translation>PNG图片(*.png)</translation>
    </message>
</context>
<context>
    <name>RPCConsole</name>
    <message>
        <location filename="../forms/rpcconsole.ui" line="+51"/>
        <location line="+43"/>
        <location line="+47"/>
        <location line="+16"/>
        <location line="+23"/>
        <location line="+16"/>
        <location line="+36"/>
        <location line="+16"/>
        <location line="+30"/>
        <location line="+58"/>
        <location line="+43"/>
        <location line="+42"/>
        <location line="+443"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+23"/>
        <location line="+26"/>
        <location line="+23"/>
        <location line="+23"/>
        <location filename="../rpcconsole.cpp" line="+478"/>
        <source>N/A</source>
        <translation>不可用</translation>
    </message>
    <message>
        <location line="-1088"/>
        <source>Client version</source>
        <translation>客户端版本</translation>
    </message>
    <message>
        <location line="-135"/>
        <source>&amp;Information</source>
        <translation>信息(&amp;I)</translation>
    </message>
    <message>
        <location line="+291"/>
        <source>Startup time</source>
        <translation>启动时间</translation>
    </message>
    <message>
        <location line="-215"/>
        <source>Number of connections</source>
        <translation>连接数</translation>
    </message>
    <message>
        <location line="+245"/>
        <source>Block chain</source>
        <translation>区块链</translation>
    </message>
    <message>
        <location line="-58"/>
        <source>Current number of blocks</source>
        <translation>当前区块数</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Last block time</source>
        <translation>上个区块的时间</translation>
    </message>
    <message>
        <location line="+96"/>
        <source>&amp;Open</source>
        <translation>打开(&amp;O)</translation>
    </message>
    <message>
        <location line="+885"/>
        <source>&amp;Console</source>
        <translation>控制台(&amp;C)</translation>
    </message>
    <message>
        <location line="-858"/>
        <source>&amp;Network Traffic</source>
        <translation>&amp;网络流量</translation>
    </message>
    <message>
        <location line="-174"/>
        <source>Client name</source>
        <translation>客户端名称</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Qt version</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+219"/>
        <source>&amp;Clear</source>
        <translation>&amp;清除</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Totals</source>
        <translation>总计</translation>
    </message>
    <message>
        <location line="+64"/>
        <source>In:</source>
        <translation>输入：</translation>
    </message>
    <message>
        <location line="+80"/>
        <source>Out:</source>
        <translation>输出：</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>&amp;Peers</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+67"/>
        <source>Banned peers</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+65"/>
        <location filename="../rpcconsole.cpp" line="+369"/>
        <source>Select a peer to view detailed information.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+37"/>
        <source>Whitelisted</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Direction</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Version</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>User Agent</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Services</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Starting Block</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Synced Headers</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Synced Blocks</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Ban Score</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Connection Time</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Last Send</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Last Receive</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Sent</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Received</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Ping Time</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>The duration of a currently outstanding ping.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Ping Wait</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Min Ping</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Time Offset</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+124"/>
        <source>&amp;Scraper</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="-1081"/>
        <source>Debug log file</source>
        <translation>调试日志文件</translation>
    </message>
    <message>
        <location line="+1058"/>
        <source>Clear console</source>
        <translation>清空控制台</translation>
    </message>
    <message>
        <location filename="../rpcconsole.cpp" line="-403"/>
        <source>Use up and down arrows to navigate history, and &lt;b&gt;Ctrl-L&lt;/b&gt; to clear screen.</source>
        <translation>使用上下方向键浏览历史,  &lt;b&gt;Ctrl-L&lt;/b&gt;清除屏幕。</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Type &lt;b&gt;help&lt;/b&gt; for an overview of available commands.</source>
        <translation>使用 &lt;b&gt;help&lt;/b&gt; 命令显示帮助信息。</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>Welcome to the Gridcoin RPC console! </source>
        <translation>欢迎来到格雷德币RPC控制台！</translation>
    </message>
    <message>
        <location line="-126"/>
        <source>&amp;Disconnect</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+1"/>
        <location line="+1"/>
        <location line="+1"/>
        <source>Ban for</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="-3"/>
        <source>1 &amp;hour</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>1 &amp;day</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>1 &amp;week</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>1 &amp;year</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+47"/>
        <source>&amp;Unban</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+25"/>
        <source>Yes</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+0"/>
        <source>No</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+170"/>
        <source>%1 B</source>
        <translation>%1 B</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 KB</source>
        <translation>%1 KB</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 MB</source>
        <translation>%1 MB</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 GB</source>
        <translation>%1 GB</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>%1 m</source>
        <translation>%1 分</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>%1 h</source>
        <translation>%1 时</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>%1 h %2 m</source>
        <translation>%1 时 %2 分</translation>
    </message>
    <message>
        <location line="+125"/>
        <source>(node id: %1)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>via %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+1"/>
        <source>never</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Inbound</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Outbound</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+6"/>
        <source>Unknown</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../forms/rpcconsole.ui" line="-1335"/>
        <source>Gridcoin - Debug Console</source>
        <translation>格雷德币 - 调试控制台</translation>
    </message>
    <message>
        <location line="+335"/>
        <source>Boost version</source>
        <translation>加速器版本</translation>
    </message>
    <message>
        <location line="-44"/>
        <source>Proof Of Research Difficulty</source>
        <translation>科研证明难度</translation>
    </message>
    <message>
        <location line="-247"/>
        <source>1</source>
        <translation>1</translation>
    </message>
    <message>
        <location line="+164"/>
        <source>Gridcoin Core:</source>
        <translation>格雷德币核心：</translation>
    </message>
    <message>
        <location line="-104"/>
        <source>Build date</source>
        <translation>创建日期</translation>
    </message>
    <message>
        <location line="+201"/>
        <source>Network:</source>
        <translation>网络：</translation>
    </message>
    <message>
        <location line="-275"/>
        <source>On testnet</source>
        <translation>测试网络</translation>
    </message>
    <message>
        <location line="+348"/>
        <source>Estimated total blocks</source>
        <translation>预计区块总数</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Open the Gridcoin debug log file from the current data directory. This can take a few seconds for large log files.</source>
        <translation>在当前数据目录打开调试日志文件。大的日志文件需要几秒钟。</translation>
    </message>
    <message>
        <location line="-301"/>
        <source>Command-line options</source>
        <translation>命令行选项</translation>
    </message>
    <message>
        <location line="-33"/>
        <source>Show the Gridcoin help message to get a list with possible Gridcoin command-line options.</source>
        <translation>显示格雷德币帮助信息以得到格雷德币命令行可能选项的列表。</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>&amp;Show</source>
        <translation>显示(&amp;S)</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>OpenSSL version</source>
        <translation>OpenSSL版本</translation>
    </message>
</context>
<context>
    <name>SendCoinsDialog</name>
    <message>
        <location filename="../forms/sendcoinsdialog.ui" line="+14"/>
        <location filename="../sendcoinsdialog.cpp" line="+177"/>
        <location line="+5"/>
        <location line="+5"/>
        <location line="+5"/>
        <location line="+6"/>
        <location line="+5"/>
        <location line="+5"/>
        <source>Send Coins</source>
        <translation>付款</translation>
    </message>
    <message>
        <location line="+67"/>
        <source>Coin Control Features</source>
        <translation>货币支配特征</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Inputs...</source>
        <translation>输入...</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>automatically selected</source>
        <translation>已自动选择</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Insufficient funds!</source>
        <translation>金额不足</translation>
    </message>
    <message>
        <location line="+83"/>
        <source>Quantity:</source>
        <translation>数量</translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+26"/>
        <source>0</source>
        <translation>0</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>Bytes:</source>
        <translation>字节数：</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>Amount:</source>
        <translation>金额：</translation>
    </message>
    <message>
        <location line="+16"/>
        <location line="+68"/>
        <location line="+68"/>
        <location line="+23"/>
        <source>0.00 GRC</source>
        <translation>0.00 GRC</translation>
    </message>
    <message>
        <location line="-149"/>
        <source>Priority:</source>
        <translation>优先权：</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>medium</source>
        <translation>中</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>Fee:</source>
        <translation>费用：</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Low Output:</source>
        <translation>低级输出：</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>no</source>
        <translation>否</translation>
    </message>
    <message>
        <location line="+29"/>
        <source>After Fee:</source>
        <translation>加上费用后:</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Change</source>
        <translation>零钱</translation>
    </message>
    <message>
        <location line="+44"/>
        <source>custom change address</source>
        <translation>惯用的零钱地址</translation>
    </message>
    <message>
        <location line="+141"/>
        <source>Remove all transaction fields</source>
        <translation>移除所有交易项</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>123.456 GRC</source>
        <translation>123.456 GRC</translation>
    </message>
    <message>
        <location line="-70"/>
        <source>Send to multiple recipients at once</source>
        <translation>一次发送给多个接收者</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Add &amp;Recipient</source>
        <translation>添加接收人(&amp;R)</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Clear &amp;All</source>
        <translation>清除所有(&amp;A)</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>Balance:</source>
        <translation>余额：</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>Confirm the send action</source>
        <translation>确认并付款</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>S&amp;end</source>
        <translation>付款(&amp;E)</translation>
    </message>
    <message>
        <location line="-206"/>
        <source>Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)</source>
        <translation>输入一个格雷德币地址(例如S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)</translation>
    </message>
    <message>
        <location filename="../sendcoinsdialog.cpp" line="-165"/>
        <source>Copy quantity</source>
        <translation></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy amount</source>
        <translation>复制金额</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy fee</source>
        <translation>复制费用</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy after fee</source>
        <translation>复制转账金额加费用</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy bytes</source>
        <translation>复制比特数</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy priority</source>
        <translation>复制优先权</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy low output</source>
        <translation>复制低级输出</translation>
    </message>
    <message>
        <location line="+93"/>
        <source>&lt;b&gt;%1&lt;/b&gt; to %2 (%3)</source>
        <translation>&lt;b&gt;%1&lt;/b&gt; 到 %2 (%3)</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Are you sure you want to send %1?</source>
        <translation>您确定要付款 %1 吗?</translation>
    </message>
    <message>
        <location line="+0"/>
        <source> and </source>
        <translation> 和 </translation>
    </message>
    <message>
        <location line="+29"/>
        <source>The recipient address is not valid, please recheck.</source>
        <translation>接收者地址不合法，请检查。</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>Duplicate address found, can only send to each address once per send operation.</source>
        <translation>发现重复的地址, 每次只能对同一地址发送一次。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Error: Transaction creation failed.</source>
        <translation>错误: 创建交易失败。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.</source>
        <translation>错误: 交易被拒绝. 如果您使用的是备份钱包，可能存在两个钱包不同步的情况，另一个钱包中的比特币已经被使用，但本地的这个钱包尚没有记录。</translation>
    </message>
    <message>
        <location line="+251"/>
        <source>WARNING: Invalid Gridcoin address</source>
        <translation>警告：无效的格雷德币地址</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>WARNING: unknown change address</source>
        <translation>警告：未知的零钱地址</translation>
    </message>
    <message>
        <location line="-427"/>
        <source>Copy change</source>
        <translation>复制余额</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>Confirm send coins</source>
        <translation>确认付款</translation>
    </message>
    <message>
        <location line="+35"/>
        <source>The amount to pay must be larger than 0.</source>
        <translation>支付金额必须大于0。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The amount exceeds your balance.</source>
        <translation>金额超出您的账上余额。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>The total exceeds your balance when the %1 transaction fee is included.</source>
        <translation>计入 %1 交易费后的金额超出您的账上余额。</translation>
    </message>
    <message>
        <location line="+280"/>
        <source>(no label)</source>
        <translation>(无标签)</translation>
    </message>
</context>
<context>
    <name>SendCoinsEntry</name>
    <message>
        <location filename="../forms/sendcoinsentry.ui" line="+155"/>
        <source>A&amp;mount:</source>
        <translation>金额(&amp;M)</translation>
    </message>
    <message>
        <location line="-106"/>
        <source>Pay &amp;To:</source>
        <translation>付给(&amp;T)：</translation>
    </message>
    <message>
        <location line="-35"/>
        <source>Form</source>
        <translation>表单</translation>
    </message>
    <message>
        <location line="+128"/>
        <source>&amp;Label:</source>
        <translation>标签(&amp;L)?</translation>
    </message>
    <message>
        <location line="-75"/>
        <source>The address to send the payment to  (e.g. Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)</source>
        <translation>付款地址 (例如:Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Choose address from address book</source>
        <translation>从地址薄选择地址</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Remove this recipient</source>
        <translation>移除此接收者</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>Send Custom Message to a Gridcoin Recipient</source>
        <translation>向格雷德币收款者发送常用消息</translation>
    </message>
    <message>
        <location line="-90"/>
        <source>Alt+A</source>
        <translation>Alt+A</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Paste address from clipboard</source>
        <translation>从剪贴板粘贴地址</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Alt+P</source>
        <translation>Alt+P</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Message:</source>
        <translation>消息：</translation>
    </message>
    <message>
        <location line="-89"/>
        <location line="+3"/>
        <source>Enter a label for this address to add it to your address book</source>
        <translation>为这个地址输入一个标签，以便将它添加到您的地址簿</translation>
    </message>
    <message>
        <location line="+33"/>
        <source>Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)</source>
        <translation>请输入格雷德币地址 (例如:S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)</translation>
    </message>
</context>
<context>
    <name>SignVerifyMessageDialog</name>
    <message>
        <location filename="../forms/signverifymessagedialog.ui" line="+14"/>
        <source>Signatures - Sign / Verify a Message</source>
        <translation>签名 - 签名/验证一条消息</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>&amp;Sign Message</source>
        <translation>对消息签名(&amp;S)</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>You can sign messages with your addresses to prove you own them. Be careful not to sign anything vague, as phishing attacks may try to trick you into signing your identity over to them. Only sign fully-detailed statements you agree to.</source>
        <translation>您可以用你的地址对消息进行签名，以证明您是该地址的所有人。注意不要对模棱两可的消息签名，以免遭受钓鱼式攻击。请确保消息真实明确的表达了您的意愿。</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>The address to sign the message with (e.g. Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)</source>
        <translation>用来签名的格雷德币地址  (例如Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)</translation>
    </message>
    <message>
        <location line="+13"/>
        <location line="+198"/>
        <source>Choose an address from the address book</source>
        <translation>从地址簿选择地址</translation>
    </message>
    <message>
        <location line="-113"/>
        <source>Sign the message to prove you own this Gridcoin address</source>
        <translation>发送签名消息以证明您是该格雷德币地址的拥有者</translation>
    </message>
    <message>
        <location line="+79"/>
        <source>Enter the signing address, message (ensure you copy line breaks, spaces, tabs, etc. exactly) and signature below to verify the message. Be careful not to read more into the signature than what is in the signed message itself, to avoid being tricked by a man-in-the-middle attack.</source>
        <translation>请在下面输入接收者地址、消息（确保换行符、空格符、制表符等完全相同）和签名以验证消息。请仔细核对签名信息，以提防中间人攻击。</translation>
    </message>
    <message>
        <location line="+21"/>
        <source>The address the message was signed with (e.g. Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)</source>
        <translation>消息使用的签名地址 (例如Sjz75uKHzUQJnSdzvpiigEGxseKkDhQToX)</translation>
    </message>
    <message>
        <location line="+47"/>
        <source>Verify the message to ensure it was signed with the specified Gridcoin address</source>
        <translation>验证消息，确保消息是由指定的格雷德币地址签名过的</translation>
    </message>
    <message>
        <location line="-222"/>
        <location line="+198"/>
        <source>Alt+A</source>
        <translation>Alt+A</translation>
    </message>
    <message>
        <location line="-188"/>
        <source>Paste address from clipboard</source>
        <translation>从剪贴板粘贴地址</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Alt+P</source>
        <translation>Alt+P</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Enter the message you want to sign here</source>
        <translation>请输入您要发送的签名消息</translation>
    </message>
    <message>
        <location line="+22"/>
        <source>Copy the current signature to the system clipboard</source>
        <translation>复制当前签名至剪切板</translation>
    </message>
    <message>
        <location line="+24"/>
        <source>Sign &amp;Message</source>
        <translation>消息签名(&amp;M)</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Reset all sign message fields</source>
        <translation>清空所有签名消息栏</translation>
    </message>
    <message>
        <location line="+3"/>
        <location line="+147"/>
        <source>Clear &amp;All</source>
        <translation>清除所有(&amp;A)</translation>
    </message>
    <message>
        <location line="-94"/>
        <location line="+77"/>
        <source>&amp;Verify Message</source>
        <translation>验证消息(&amp;V)</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Reset all verify message fields</source>
        <translation>清空所有验证消息栏</translation>
    </message>
    <message>
        <location line="-256"/>
        <location line="+198"/>
        <source>Enter a Gridcoin address (e.g. S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)</source>
        <translation>输入一个格雷德币地址 (例如S67nL4vELWwdDVzjgtEP4MxryarTZ9a8GB)</translation>
    </message>
    <message>
        <location line="-134"/>
        <source>Click &quot;Sign Message&quot; to generate signature</source>
        <translation>单击“签名消息”产生签名。</translation>
    </message>
    <message>
        <location line="+166"/>
        <source>Enter Gridcoin signature</source>
        <translation>输入格雷德币签名</translation>
    </message>
    <message>
        <location filename="../signverifymessagedialog.cpp" line="+105"/>
        <location line="+81"/>
        <source>The entered address is invalid.</source>
        <translation>输入的地址非法</translation>
    </message>
    <message>
        <location line="-81"/>
        <location line="+8"/>
        <location line="+73"/>
        <location line="+8"/>
        <source>Please check the address and try again.</source>
        <translation>请检查地址后重试。</translation>
    </message>
    <message>
        <location line="-81"/>
        <location line="+81"/>
        <source>The entered address does not refer to a key.</source>
        <translation>输入的地址没有关联的公私钥对。</translation>
    </message>
    <message>
        <location line="-73"/>
        <source>Wallet unlock was cancelled.</source>
        <translation>钱包解锁动作取消。</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Private key for the entered address is not available.</source>
        <translation>找不到输入地址关联的私钥。</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Message signing failed.</source>
        <translation>消息签名失败。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Message signed.</source>
        <translation>消息已签名。</translation>
    </message>
    <message>
        <location line="+59"/>
        <source>The signature could not be decoded.</source>
        <translation>签名无法解码。</translation>
    </message>
    <message>
        <location line="+0"/>
        <location line="+13"/>
        <source>Please check the signature and try again.</source>
        <translation>请检查签名后重试。</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>The signature did not match the message digest.</source>
        <translation>签名与消息摘要不匹配。</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Message verification failed.</source>
        <translation>消息验证失败。</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Message verified.</source>
        <translation>消息验证成功。</translation>
    </message>
</context>
<context>
    <name>TransactionDesc</name>
    <message numerus="yes">
        <location filename="../transactiondesc.cpp" line="+35"/>
        <source>Open for %n more block(s)</source>
        <translation>
            <numerusform>%n 个数据块后打开</numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Open until %1</source>
        <translation>至 %1 个数据块时开启</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>conflicted</source>
        <translation>冲突</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1/offline</source>
        <translation>%1 / 离线</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1/unconfirmed</source>
        <translation>%1 / 未确认</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>%1 confirmations</source>
        <translation>%1 个确认</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>Status</source>
        <translation>状态</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>, has not been successfully broadcast yet</source>
        <translation>，未被成功广播</translation>
    </message>
    <message numerus="yes">
        <location line="+3"/>
        <source>, broadcast through %n node(s)</source>
        <translation>
            <numerusform>, 通过 %n 个节点广播 </numerusform>
        </translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Date</source>
        <translation>日期</translation>
    </message>
    <message>
        <location line="+4"/>
        <location line="+5"/>
        <source>Source</source>
        <translation>源</translation>
    </message>
    <message>
        <location line="+18"/>
        <location line="+18"/>
        <source>From</source>
        <translation>来自</translation>
    </message>
    <message>
        <location line="+167"/>
        <source>Gridcoin generated coins must mature 110 blocks before they can be spent. When you generated this block, it was broadcast to the network to be added to the block chain. If it fails to get into the chain, its state will change to &quot;not accepted&quot; and it won&apos;t be spendable. This may occasionally happen if another node generates a block within a few seconds of yours.</source>
        <translation>生成的格雷德币必须经过 110 个区块的成熟期后才可以使用。区块被生成后，它将被广播到网络中以加入区块链。如果它未能进入区块链，其状态将变为“未被接受”并且不可使用。有些时候，可能会有另一个节点比你早几秒钟成功生成一个区块，此时就可能发生这种情况。</translation>
    </message>
    <message>
        <location line="-208"/>
        <source>Generated in CoinBase</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+11"/>
        <source>MINED - POS</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MINED - POR</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MINED - ORPHANED</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>POS SIDE STAKE</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>POR SIDE STAKE</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MINED - UNKNOWN</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+25"/>
        <source>unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <location line="+1"/>
        <location line="+25"/>
        <location line="+63"/>
        <source>To</source>
        <translation>到</translation>
    </message>
    <message>
        <location line="-84"/>
        <location line="+3"/>
        <source>own address</source>
        <translation>自己的地址</translation>
    </message>
    <message>
        <location line="-3"/>
        <source>label</source>
        <translation>标签</translation>
    </message>
    <message>
        <location line="+40"/>
        <location line="+14"/>
        <location line="+50"/>
        <location line="+20"/>
        <location line="+74"/>
        <source>Credit</source>
        <translation>收入</translation>
    </message>
    <message numerus="yes">
        <location line="-155"/>
        <source>matures in %n more block(s)</source>
        <translation>
            <numerusform>%n 个数据块后成熟(mature) </numerusform>
        </translation>
    </message>
    <message>
        <location line="+3"/>
        <source>not accepted</source>
        <translation>未被接受</translation>
    </message>
    <message>
        <location line="+48"/>
        <location line="+9"/>
        <location line="+16"/>
        <location line="+74"/>
        <source>Debit</source>
        <translation>支出</translation>
    </message>
    <message>
        <location line="-83"/>
        <source>Transaction fee</source>
        <translation>交易费</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Net amount</source>
        <translation>净额</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Message</source>
        <translation>消息</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Comment</source>
        <translation>备注</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>TX ID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <location line="+3"/>
        <source>Block Hash</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Transaction Stake Data</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Transaction Message Data</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+21"/>
        <source>Transaction Debits/Credits</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Transaction Data</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Transaction Inputs</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Amount</source>
        <translation>金额</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>true</source>
        <translation>是</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>false</source>
        <translation>否</translation>
    </message>
</context>
<context>
    <name>TransactionDescDialog</name>
    <message>
        <location filename="../forms/transactiondescdialog.ui" line="+20"/>
        <source>Transaction details</source>
        <translation>交易详情</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>This pane shows a detailed description of the transaction</source>
        <translation>当前面板显示了交易的详细信息</translation>
    </message>
    <message>
        <location line="+28"/>
        <source>Execute Contract</source>
        <translation>履行合同</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>C&amp;lose</source>
        <translation>关闭(&amp;l)</translation>
    </message>
</context>
<context>
    <name>TransactionTableModel</name>
    <message>
        <location filename="../transactiontablemodel.cpp" line="+236"/>
        <source>Date</source>
        <translation>日期</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Amount</source>
        <translation>金额</translation>
    </message>
    <message numerus="yes">
        <location line="+52"/>
        <source>Open for %n more block(s)</source>
        <translation>
            <numerusform>在 %n 个区块后打开</numerusform>
        </translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Confirming (%1 of %2 recommended confirmations)&lt;br&gt;</source>
        <translation>确认中 (推荐 %2个确认，已经有 %1个确认)</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Immature (%1 confirmations, will be available after %2)&lt;br&gt;</source>
        <translation>未成熟 (%1 个确认，将在 %2 个后可用)&lt;br&gt;</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>This block was not received by any other nodes&lt;br&gt; and will probably not be accepted!</source>
        <translation>此数据块未被任何其他节点接收，可能不被接受！</translation>
    </message>
    <message>
        <location line="+316"/>
        <source>Destination address of transaction.</source>
        <translation>转账目标地址</translation>
    </message>
    <message>
        <location line="-337"/>
        <source>Open until %1</source>
        <translation>直到 %1 后再打开</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Offline</source>
        <translation>离线</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Unconfirmed</source>
        <translation>未确认的 </translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Confirmed (%1 confirmations)</source>
        <translation>已确认 (%1 条确认信息)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Conflicted</source>
        <translation>冲突的</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Generated but not accepted</source>
        <translation>已生成但未被接受</translation>
    </message>
    <message>
        <location line="+44"/>
        <source>Received with</source>
        <translation>收款</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Received from</source>
        <translation>收款来自</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Sent to</source>
        <translation>付款</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Payment to yourself</source>
        <translation>付款给自己</translation>
    </message>
    <message>
        <location line="+7"/>
        <source>MINED - POS</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MINED - POR</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MINED - ORPHANED</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>POS SIDE STAKE</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>POR SIDE STAKE</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>MINED - UNKNOWN</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+54"/>
        <source>(n/a)</source>
        <translation>(不可用)</translation>
    </message>
    <message>
        <location line="+190"/>
        <source>Transaction status. Hover over this field to show number of confirmations.</source>
        <translation>交易状态。 鼠标移到此区域可显示确认项数量。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Date and time that the transaction was received.</source>
        <translation>接收到交易的时间</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Type of transaction.</source>
        <translation>交易类别。</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Amount removed from or added to balance.</source>
        <translation>从余额添加或移除的金额。</translation>
    </message>
</context>
<context>
    <name>TransactionView</name>
    <message>
        <location filename="../transactionview.cpp" line="+55"/>
        <location line="+16"/>
        <source>All</source>
        <translation>全部</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>Today</source>
        <translation>今天</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This week</source>
        <translation>这星期</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This month</source>
        <translation>这个月</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Last month</source>
        <translation>上个月</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>This year</source>
        <translation>今年</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Range...</source>
        <translation>指定范围...</translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Received with</source>
        <translation>收款</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Sent to</source>
        <translation>付款</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>To yourself</source>
        <translation>给自己</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Mined</source>
        <translation>挖矿所得</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Other</source>
        <translation>其他</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Enter address or label to search</source>
        <translation>输入地址或标签进行搜索</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Min amount</source>
        <translation>最小金额</translation>
    </message>
    <message>
        <location line="+183"/>
        <source>Export Transaction Data</source>
        <translation>导出交易历史</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Amount</source>
        <translation>金额</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Error exporting</source>
        <translation>导出时出错</translation>
    </message>
    <message>
        <location line="+0"/>
        <source>Could not write to file %1.</source>
        <translation>无法写入文件 %1 .</translation>
    </message>
    <message>
        <location line="-168"/>
        <source>Copy address</source>
        <translation>复制地址</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy label</source>
        <translation>复制标签</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy amount</source>
        <translation>复制金额</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Copy transaction ID</source>
        <translation>复制交易识别码</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Edit label</source>
        <translation>????</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Show transaction details</source>
        <translation>编辑标签</translation>
    </message>
    <message>
        <location line="+145"/>
        <source>Comma separated file (*.csv)</source>
        <translation>逗号分隔文件 (*.csv)</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Confirmed</source>
        <translation>已确认</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Date</source>
        <translation>日期</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Type</source>
        <translation>类型</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Label</source>
        <translation>标签</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Address</source>
        <translation>地址</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>ID</source>
        <translation>ID</translation>
    </message>
    <message>
        <location line="+104"/>
        <source>Range:</source>
        <translation>范围：</translation>
    </message>
    <message>
        <location line="+8"/>
        <source>to</source>
        <translation>到</translation>
    </message>
</context>
<context>
    <name>VotingChartDialog</name>
    <message>
        <location filename="../votingdialog.cpp" line="-405"/>
        <source>Poll Results</source>
        <translation>民意调查结果</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Q: </source>
        <translation>问：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Discussion URL: </source>
        <translation>讨论网址：</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Chart</source>
        <translation>图表</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>List</source>
        <translation>列表</translation>
    </message>
    <message>
        <location line="-28"/>
        <source>Best Answer: </source>
        <translation>最佳答案</translation>
    </message>
</context>
<context>
    <name>VotingDialog</name>
    <message>
        <location line="-313"/>
        <source>Active Polls (Right Click to Vote)</source>
        <translation>开放的民意调查 (点击右键以投票)</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Filter: </source>
        <translation>过滤器：</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Reload Polls</source>
        <translation>重新加载民意调查</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Load History</source>
        <translation>加载历史</translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Create Poll</source>
        <translation>新建民意调查</translation>
    </message>
    <message>
        <location line="+46"/>
        <source>...loading data!</source>
        <translation>...正在加载数据！</translation>
    </message>
    <message>
        <location line="+26"/>
        <source>No polls !</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>VotingTableModel</name>
    <message>
        <location line="-382"/>
        <source>#</source>
        <translation>#</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Title</source>
        <translation>标题</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Expiration</source>
        <translation>截止时间</translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Share Type</source>
        <translation>计票方式</translation>
    </message>
    <message>
        <location line="-2"/>
        <source># Voters</source>
        <translation>#投票者</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Total Shares</source>
        <translation>总票数</translation>
    </message>
    <message>
        <location line="-2"/>
        <source>Best Answer</source>
        <translation>最佳答案</translation>
    </message>
    <message>
        <location line="+126"/>
        <source>Row Number.</source>
        <translation>行数。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Title.</source>
        <translation>标题。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Expiration.</source>
        <translation>截止时间。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Share Type.</source>
        <translation>计票方式。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Total Participants.</source>
        <translation>总参与人数。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Total Shares.</source>
        <translation>总票数。</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Best Answer.</source>
        <translation>最佳答案。</translation>
    </message>
</context>
<context>
    <name>VotingVoteDialog</name>
    <message>
        <location line="+533"/>
        <source>PlaceVote</source>
        <translation>放置投票</translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Q: </source>
        <translation>问题：</translation>
    </message>
    <message>
        <location line="+10"/>
        <source>Discussion URL: </source>
        <translation>讨论网址：</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Best Answer: </source>
        <translation>最佳答案：</translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Vote</source>
        <translation>投票</translation>
    </message>
    <message>
        <location line="+37"/>
        <source>Vote failed! Select one or more items to vote.</source>
        <translation>投票失败！选择一项或更多项后再投票。</translation>
    </message>
</context>
<context>
    <name>WalletModel</name>
    <message>
        <location filename="../walletmodel.cpp" line="+239"/>
        <source>Sending...</source>
        <translation>正在发送...</translation>
    </message>
</context>
<context>
    <name>bitcoin-core</name>
    <message>
        <location filename="../bitcoinstrings.cpp" line="+225"/>
        <source>Options:</source>
        <translation>选项：
</translation>
    </message>
    <message>
        <location line="+38"/>
        <source>Specify data directory</source>
        <translation>指定数据目录
</translation>
    </message>
    <message>
        <location line="-121"/>
        <source>Connect to a node to retrieve peer addresses, and disconnect</source>
        <translation>连接一个节点并获取对端地址，然后断开连接</translation>
    </message>
    <message>
        <location line="+124"/>
        <source>Specify your own public address</source>
        <translation>指定您的公共地址</translation>
    </message>
    <message>
        <location line="-159"/>
        <source>Accept command line and JSON-RPC commands</source>
        <translation>接受命令行和 JSON-RPC 命令
</translation>
    </message>
    <message>
        <location line="+135"/>
        <source>Run in the background as a daemon and accept commands</source>
        <translation>在后台作为守护进程运行并接受指令
</translation>
    </message>
    <message>
        <location line="-182"/>
        <source>Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)</source>
        <translation>当钱包转账变化时执行命令 (命令行中的 %s 会被替换成TxID)</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>Block creation options:</source>
        <translation>区块生成选项：</translation>
    </message>
    <message>
        <location line="+41"/>
        <source>Failed to listen on any port. Use -listen=0 if you want this.</source>
        <translation>监听任意端口失败。 若您希望如此，使用 -listen=0.</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>Specify wallet file (within data directory)</source>
        <translation>指定钱包文件（数据目录内）</translation>
    </message>
    <message>
        <location line="-18"/>
        <source>Send trace/debug info to console instead of debug.log file</source>
        <translation>跟踪/调试信息输出到控制台，不输出到 debug.log 文件</translation>
    </message>
    <message>
        <location line="+13"/>
        <source>Shrink debug.log file on client startup (default: 1 when no -debug)</source>
        <translation>客户端启动时压缩debug.log文件(缺省：no-debug模式时为1)</translation>
    </message>
    <message>
        <location line="+30"/>
        <source>Username for JSON-RPC connections</source>
        <translation>JSON-RPC 连接用户名</translation>
    </message>
    <message>
        <location line="-59"/>
        <source>Password for JSON-RPC connections</source>
        <translation>JSON-RPC 连接密码
</translation>
    </message>
    <message>
        <location line="-168"/>
        <source>Execute command when the best block changes (%s in cmd is replaced by block hash)</source>
        <translation>当最佳区块变化时执行命令 (命令行中的 %s 会被替换成区块哈希值)</translation>
    </message>
    <message>
        <location line="+53"/>
        <source>Allow DNS lookups for -addnode, -seednode and -connect</source>
        <translation>使用 -addnode, -seednode 和 -connect选项时允许DNS查找</translation>
    </message>
    <message>
        <location line="+151"/>
        <source>Staking Only - Investor Mode</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Staking Only - No Eligible Research Projects</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+6"/>
        <source>To use the %s option</source>
        <translation>使用 %s 选项</translation>
    </message>
    <message>
        <location line="-259"/>
        <source>%s, you must set a rpcpassword in the configuration file:
 %s
It is recommended you use the following random password:
rpcuser=gridcoinrpc
rpcpassword=%s
(you do not need to remember this password)
The username and password MUST NOT be the same.
If the file does not exist, create it with owner-readable-only file permissions.
It is also recommended to set alertnotify so you are notified of problems;
for example: alertnotify=echo %%s | mail -s &quot;Gridcoin Alert&quot; admin@foo.com
</source>
        <translation>%s,您必须在配置文件中设置RPC密码：
 %s
推荐您使用以下随机密码：
rpcuser=gridcoinrpc
rpcpassword=%s
(您无需记忆此密码)
用户名和密码绝对不能雷同。
若文件不存在，请新建并设置为仅所有者可读。
同样推荐设置alertnotify，这样当有问题时您会被通知到；
例如： alertnotify=echo %%s | mail -s &quot;Gridcoin Alert&quot; admin@foo.com
</translation>
    </message>
    <message>
        <location line="+182"/>
        <source>Loading addresses...</source>
        <translation>正在加载地址...</translation>
    </message>
    <message>
        <location line="-14"/>
        <source>Invalid -proxy address: &apos;%s&apos;</source>
        <translation>无效的代理地址: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>Unknown network specified in -onlynet: &apos;%s&apos;</source>
        <translation>被指定的是未知网络 -onlynet: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="-200"/>
        <source>Unable to bind to %s on this computer. Gridcoin is probably already running.</source>
        <translation>无法在本机绑定 %s 端口。比特币客户端软件可能已经在运行。</translation>
    </message>
    <message>
        <location line="+196"/>
        <source>Unable to bind to %s on this computer (bind returned error %d, %s)</source>
        <translation>无法绑定本机端口 %s  (返回错误消息 %d, %s)</translation>
    </message>
    <message>
        <location line="-115"/>
        <source>Error: Wallet locked, unable to create transaction  </source>
        <translation>错误：钱包已锁定，不能创建交易</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Error: Wallet unlocked for staking only, unable to create transaction.</source>
        <translation>错误：钱包解锁仅用于权益增值，无法创建交易。</translation>
    </message>
    <message>
        <location line="-110"/>
        <source>Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds  </source>
        <translation>错误：转账需要至少 %s 的转账费，因为其数额，复杂度或使用了近期收到的存款  </translation>
    </message>
    <message>
        <location line="+107"/>
        <source>Error: Transaction creation failed  </source>
        <translation>错误：交易创建失败 </translation>
    </message>
    <message>
        <location line="+88"/>
        <source>Sending...</source>
        <translation>发送中...</translation>
    </message>
    <message>
        <location line="-199"/>
        <source>Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.</source>
        <translation>交易被拒绝。您钱包中的钱币可能已经被花费，例如当您使用wallet.dat文件的副本，钱币在该副本中被花费但未在这里标记为已花费时。</translation>
    </message>
    <message>
        <location line="+139"/>
        <source>Invalid amount</source>
        <translation>无效金额</translation>
    </message>
    <message>
        <location line="-8"/>
        <source>Insufficient funds</source>
        <translation>存款不足</translation>
    </message>
    <message>
        <location line="+18"/>
        <source>Loading block index...</source>
        <translation>加载区块索引...</translation>
    </message>
    <message>
        <location line="-163"/>
        <source>An error occurred while setting up the RPC port %u for listening on IPv6, falling back to IPv4: %s</source>
        <translation>当设置RPC端口%u以进行IPv6监听时出错，回到IPv4: %s</translation>
    </message>
    <message>
        <location line="-5"/>
        <source>Acceptable ciphers (default: TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>An error occurred while setting up the RPC port %u for listening on IPv4: %s</source>
        <translation>设置RPC端口%u以监听IPv4:%s时出现错误</translation>
    </message>
    <message>
        <location line="+68"/>
        <source>You must set rpcpassword=&lt;password&gt; in the configuration file:
%s
If the file does not exist, create it with owner-readable-only file permissions.</source>
        <translation>您必须在配置文件
%s
中加入选项
rpcpassword=&lt;password&gt;
如果配置文件不存在，请新建，并将文件权限设置为仅允许文件所有者读取。</translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Balance too low to create a smart contract.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+17"/>
        <source>Compute Neural Network Hashes...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+20"/>
        <source>Error obtaining status.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+11"/>
        <source>Finding first applicable Research Project...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Gridcoin version</source>
        <translation>格雷德币版本</translation>
    </message>
    <message>
        <location line="+23"/>
        <source>Loading Network Averages...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Loading Persisted Data Cache...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Maximum number of outbound connections (default: 8)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+14"/>
        <source>No coins</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>No current polls</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+7"/>
        <source>Out of range exception while parsing Transaction Message -&gt; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>POR Blocks Verified</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Please wait for new user wizard to start...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+25"/>
        <source>Set the number of threads to service RPC calls (default: 4)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Share Type Debug</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Share Type</source>
        <translation type="unfinished">计票方式</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Specify configuration file (default: gridcoinresearch.conf)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Text Message</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Text Rain Message</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Title</source>
        <translation type="unfinished">标题</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>URL</source>
        <translation type="unfinished">网址</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Unable To Send Beacon! Unlock Wallet!</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Unable to extract Share Type. Vote likely &gt; 6 months old</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Unknown error</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Unknown</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Usage:</source>
        <translation>使用：</translation>
    </message>
    <message>
        <location line="-39"/>
        <source>Send command to -server or gridcoind</source>
        <translation>向-server或gridcoind发送指令</translation>
    </message>
    <message>
        <location line="-52"/>
        <source>List commands</source>
        <translation>命令列表
</translation>
    </message>
    <message>
        <location line="-22"/>
        <source>Get help for a command</source>
        <translation>得到关于某个命令的帮助
</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Gridcoin</source>
        <translation>格雷德币</translation>
    </message>
    <message>
        <location line="+98"/>
        <source>This help message</source>
        <translation>该帮助信息
</translation>
    </message>
    <message>
        <location line="-7"/>
        <source>Specify pid file (default: gridcoind.pid)</source>
        <translation>指定pid文件 (默认：gridcoind.pid)</translation>
    </message>
    <message>
        <location line="-12"/>
        <source>Set database cache size in megabytes (default: 25)</source>
        <translation>设置以MB为单位的数据库缓存大小 (默认值： 25MB)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Set database disk log size in megabytes (default: 100)</source>
        <translation>设置以MB为单位的数据磁盘日志大小 (默认值： 100MB)</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Specify connection timeout in milliseconds (default: 5000)</source>
        <translation>指定连接超时毫秒数 (默认: 5000)</translation>
    </message>
    <message>
        <location line="-121"/>
        <source>Connect through socks proxy</source>
        <translation>通过 socks 代理连接</translation>
    </message>
    <message>
        <location line="+103"/>
        <source>Select the version of socks proxy to use (4-5, default: 5)</source>
        <translation>选择SOCKS服务器的使用版本(4-5, 默认： 5)</translation>
    </message>
    <message>
        <location line="+44"/>
        <source>Use proxy to reach tor hidden services (default: same as -proxy)</source>
        <translation>使用代理到达隐藏服务器 (默认: 与-proxy相同)</translation>
    </message>
    <message>
        <location line="-94"/>
        <source>Listen for connections on &lt;port&gt; (default: 32749 or testnet: 32748)</source>
        <translation>使用&lt;port&gt;端口监听连接 (默认: 32749 或测试网络: 32748)</translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Maintain at most &lt;n&gt; connections to peers (default: 125)</source>
        <translation>保留最多 &lt;n&gt; 条节点连接 (默认: %u) </translation>
    </message>
    <message>
        <location line="-90"/>
        <source>Add a node to connect to and attempt to keep the connection open</source>
        <translation>添加节点并与其保持连接</translation>
    </message>
    <message>
        <location line="+27"/>
        <source>Connect only to the specified node(s)</source>
        <translation>只连接到特定的节点</translation>
    </message>
    <message>
        <location line="+84"/>
        <source>Only connect to nodes in network &lt;net&gt; (IPv4, IPv6 or Tor)</source>
        <translation>只连接 &lt;net&gt;网络中的节点 (ipv4, ipv6 或 onion)</translation>
    </message>
    <message>
        <location line="-75"/>
        <source>Discover own IP address (default: 1 when listening and no -externalip)</source>
        <translation>找到您自己的IP地址(缺省:1，当监听时， 且无 -externalip)</translation>
    </message>
    <message>
        <location line="-41"/>
        <source>Accept connections from outside (default: 1 if no -proxy or -connect)</source>
        <translation>接受外部连接 (默认: 1，若无 -proxy 或 -connect)</translation>
    </message>
    <message>
        <location line="+16"/>
        <source>Bind to given address. Use [host]:port notation for IPv6</source>
        <translation>绑定指定的IP地址开始监听。IPv6地址请使用[host]:port 格式</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>Find peers using DNS lookup (default: 1)</source>
        <translation>通过DNS查找网络上的节点 (缺省: 1)</translation>
    </message>
    <message>
        <location line="-91"/>
        <source>Sync time with other nodes. Disable if time on your system is precise e.g. syncing with NTP (default: 1)</source>
        <translation>与其他节点同步时间。无效化之，若您的系统时间是准确的，例如与NTP同步(默认: 1)</translation>
    </message>
    <message>
        <location line="+194"/>
        <source>Threshold for disconnecting misbehaving peers (default: 100)</source>
        <translation>断开 非礼节点的阀值 (默认: 100) </translation>
    </message>
    <message>
        <location line="-204"/>
        <source>Number of seconds to keep misbehaving peers from reconnecting (default: 86400)</source>
        <translation>限制 非礼节点 若干秒内不能连接 (默认: 86400)  (??: 86400)</translation>
    </message>
    <message>
        <location line="+138"/>
        <source>Maximum per-connection receive buffer, &lt;n&gt;*1000 bytes (default: 5000)</source>
        <translation>每个连接的最大接收缓存，&lt;n&gt;*1000 字节 (默认: 5000)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Maximum per-connection send buffer, &lt;n&gt;*1000 bytes (default: 1000)</source>
        <translation>每个连接的最大发送缓存，&lt;n&gt;*1000 字节 (默认: 1000)</translation>
    </message>
    <message>
        <location line="+80"/>
        <source>Use UPnP to map the listening port (default: 1 when listening)</source>
        <translation>使用UPnP暴露本机监听端口（默认：1 当正在监听)</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Use UPnP to map the listening port (default: 0)</source>
        <translation>使用UPnP暴露本机监听端口（默认：0）</translation>
    </message>
    <message>
        <location line="-118"/>
        <source>Fee per KB to add to transactions you send</source>
        <translation>每发送1KB交易所需的费用</translation>
    </message>
    <message>
        <location line="-69"/>
        <source>When creating transactions, ignore inputs with value less than this (default: 0.01)</source>
        <translation>当生成交易时，忽略价值小于此的输入 (默认：0.01)</translation>
    </message>
    <message>
        <location line="+190"/>
        <source>Use the test network</source>
        <translation>使用测试网络
</translation>
    </message>
    <message>
        <location line="-61"/>
        <source>Output extra debugging information. Implies all other -debug* options</source>
        <translation>输出调试信息。蕴含任何其他-debug*选项</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Output extra network debugging information</source>
        <translation>输出额外网络调试信息</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Prepend debug output with timestamp</source>
        <translation>输出调试信息时，前面加上时间戳</translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Send trace/debug info to debugger</source>
        <translation>跟踪/调试信息发送到调试器</translation>
    </message>
    <message>
        <location line="-182"/>
        <source>Listen for JSON-RPC connections on &lt;port&gt; (default: 15715 or testnet: 25715)</source>
        <translation>使用 &lt;port&gt;端口监听 JSON-RPC 连接 (默认: 15715 ; testnet: 25715) </translation>
    </message>
    <message>
        <location line="+51"/>
        <source>Allow JSON-RPC connections from specified IP address</source>
        <translation>允许来自指定IP地址的 JSON-RPC 连接</translation>
    </message>
    <message>
        <location line="+129"/>
        <source>Send commands to node running on &lt;ip&gt; (default: 127.0.0.1)</source>
        <translation>向IP地址为 &lt;ip&gt; 的节点发送指令 (缺省: 127.0.0.1)</translation>
    </message>
    <message>
        <location line="-8"/>
        <source>Require a confirmations for change (default: 0)</source>
        <translation>改变时要求一个确认 (默认：0)</translation>
    </message>
    <message>
        <location line="-193"/>
        <source>Enforce transaction scripts to use canonical PUSH operators (default: 1)</source>
        <translation>强制要求转账脚本使用PUSH运算(默认：1)</translation>
    </message>
    <message>
        <location line="+12"/>
        <source>Execute command when a relevant alert is received (%s in cmd is replaced by message)</source>
        <translation>当收到相关警报时执行指令 (命令行中的 %s 会被替换成消息)</translation>
    </message>
    <message>
        <location line="+226"/>
        <source>Upgrade wallet to latest format</source>
        <translation>升级钱包到最新版</translation>
    </message>
    <message>
        <location line="-29"/>
        <source>Set key pool size to &lt;n&gt; (default: 100)</source>
        <translation>设置密钥池大小为 &lt;n&gt; (缺省: 100)
</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>Rescan the block chain for missing wallet transactions</source>
        <translation>重新扫描数据链以查找遗漏的交易</translation>
    </message>
    <message>
        <location line="-119"/>
        <source>Attempt to recover private keys from a corrupt wallet.dat</source>
        <translation>尝试从一个损坏的wallet.dat文件中恢复私钥</translation>
    </message>
    <message>
        <location line="-108"/>
        <location line="+1"/>
        <source>None</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source> days</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+14"/>
        <source>A beacon was advertised less then 5 blocks ago. Please wait a full 5 blocks for your beacon to enter the chain.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+14"/>
        <source>Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+32"/>
        <source>Specify p2p connection timeout in seconds. This option determines the amount of time a peer may be inactive before the connection to it is dropped. (minimum: 1, default: 45)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Unable to obtain superblock data before vote was made to calculate voting weight</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+26"/>
        <source>Add Beacon Contract</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Add Foundation Poll</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Add Poll</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Add Project</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Address</source>
        <translation type="unfinished">地址</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Alert: </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Answer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Answers</source>
        <translation type="unfinished">答案</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Average Magnitude</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Balance</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Block Version</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Block not in index</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Block read failed</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Blocks Loaded</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Blocks Verified</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Boinc Public Key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Boinc Reward</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>CPID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Client Version</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Contract length for beacon is less then 256 in length. Size: </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Current Neural Hash</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Data</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Delete Beacon Contract</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Delete Project</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Difficulty</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Duration</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>ERROR</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Eligible for Research Rewards</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Entire balance reserved</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+9"/>
        <source>Error: Wallet locked, unable to create transaction.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Expires</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Height</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>How many blocks to check at startup (default: 2500, 0 = all)</source>
        <translation>启动时需检查的区块数量 (缺省: 2500, 设置0为检查所有区块)</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>How thorough the block verification is (0-6, default: 1)</source>
        <translation>需要几个确认 (0-6个, 缺省: 1个)</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Imports blocks from external blk000?.dat file</source>
        <translation>从外来文件 blk000?.dat 导入区块数据</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Interest</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+5"/>
        <source>Invalid amount for -peertimeout=&lt;amount&gt;: &apos;%s&apos;</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Invalid argument exception while parsing Transaction Message -&gt; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Invalid team</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Is Superblock</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+6"/>
        <source>Loading banlist...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Low difficulty!; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Magnitude</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Malformed CPID</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Message Data</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Message Length</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Message Type</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Message</source>
        <translation type="unfinished">消息</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Miner: </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Name</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Net averages not yet loaded; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Network Date</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Neural Contract Binary Size</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Neural Hash</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>No Attached Messages</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>No mature coins</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>No utxos available due to reserve balance</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Offline; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Organization</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+8"/>
        <source>Print version and exit</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Project email mismatch</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Public Key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Question</source>
        <translation type="unfinished">问题</translation>
    </message>
    <message>
        <location line="+4"/>
        <source>Research Age</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+15"/>
        <source>Set minimum block size in bytes (default: 0)</source>
        <translation>以比特为单位设置最小区块大小(默认：0)</translation>
    </message>
    <message>
        <location line="-1"/>
        <source>Set maximum block size in bytes (default: 250000)</source>
        <translation></translation>
    </message>
    <message>
        <location line="-184"/>
        <source>Set maximum size of high-priority/low-fee transactions in bytes (default: 27000)</source>
        <translation>以比特为单位设置高优先级/低费用的转账的最大大小 (默认: 27000)</translation>
    </message>
    <message>
        <location line="+172"/>
        <source>SSL options: (see the Bitcoin Wiki for SSL setup instructions)</source>
        <translation>SSL选项：(见Bitcoin Wiki中SSL安装的讲解)</translation>
    </message>
    <message>
        <location line="+42"/>
        <source>Use OpenSSL (https) for JSON-RPC connections</source>
        <translation>为 JSON-RPC 连接使用 OpenSSL (https)连接</translation>
    </message>
    <message>
        <location line="-35"/>
        <source>Server certificate file (default: server.cert)</source>
        <translation>服务器证书 (默认为 server.cert)
</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Server private key (default: server.pem)</source>
        <translation>服务器私钥 (默认为 server.pem)
</translation>
    </message>
    <message>
        <location line="-65"/>
        <source>Invalid amount for -paytxfee=&lt;amount&gt;: &apos;%s&apos;</source>
        <translation>非法金额 -paytxfee=&lt;amount&gt;: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="-100"/>
        <source>Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction.</source>
        <translation>警告：-paytxfee 被设置得非常高！若您发送一个转账，这是您将支付的转账费。</translation>
    </message>
    <message>
        <location line="+99"/>
        <source>Invalid amount for -mininput=&lt;amount&gt;: &apos;%s&apos;</source>
        <translation>无效金额 -paytxfee=&lt;amount&gt;: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="-5"/>
        <source>Initialization sanity check failed. Gridcoin is shutting down.</source>
        <translation>初始化完整性检查失败。格雷德币正在停止运行。</translation>
    </message>
    <message>
        <location line="+113"/>
        <source>Wallet %s resides outside data directory %s.</source>
        <translation>钱包%s位于数据目录%s之外。</translation>
    </message>
    <message>
        <location line="-254"/>
        <source>Cannot obtain a lock on data directory %s.  Gridcoin is probably already running.</source>
        <translation>无法给数据目录 %s 加锁。格雷德币进程可能已在运行。</translation>
    </message>
    <message>
        <location line="+252"/>
        <source>Verifying database integrity...</source>
        <translation>检查数据库完整性...</translation>
    </message>
    <message>
        <location line="-244"/>
        <source>Error initializing database environment %s! To recover, BACKUP THAT DIRECTORY, then remove everything from it except for wallet.dat.</source>
        <translation>初始化数据库环境 %s 时出错！为恢复，备份该目录，然后删除除wallet.dat之外的全部文件</translation>
    </message>
    <message>
        <location line="+48"/>
        <source>Warning: wallet.dat corrupt, data salvaged! Original wallet.dat saved as wallet.{timestamp}.bak in %s; if your balance or transactions are incorrect you should restore from a backup.</source>
        <translation>警告：wallet.dat损坏，数据已抢救! 原始的wallet.dat文件已在%s中保存为wallet.{timestamp}.bak.若您的余额或转账记录有误，您应该从备份中恢复。</translation>
    </message>
    <message>
        <location line="+202"/>
        <source>Weight</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>wallet.dat corrupt, salvage failed</source>
        <translation>wallet.dat损坏，抢救失败</translation>
    </message>
    <message>
        <location line="-19"/>
        <source>Unknown -socks proxy version requested: %i</source>
        <translation>被指定的是未知socks代理版本: %i</translation>
    </message>
    <message>
        <location line="-95"/>
        <source>Invalid -tor address: &apos;%s&apos;</source>
        <translation>无效的 -tor 地址: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="-49"/>
        <source>Cannot resolve -bind address: &apos;%s&apos;</source>
        <translation>无法解析 -bind 端口地址: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Cannot resolve -externalip address: &apos;%s&apos;</source>
        <translation>无法解析 -externalip 地址: &apos;%s&apos;</translation>
    </message>
    <message>
        <location line="+52"/>
        <source>Invalid amount for -reservebalance=&lt;amount&gt;</source>
        <translation>对-reservebalance=&lt;amount&gt; 无效的金额</translation>
    </message>
    <message>
        <location line="-33"/>
        <source>Error loading blkindex.dat</source>
        <translation>blkindex.dat文件加载错误</translation>
    </message>
    <message>
        <location line="+45"/>
        <source>Loading wallet...</source>
        <translation>正在加载钱包...</translation>
    </message>
    <message>
        <location line="-43"/>
        <source>Error loading wallet.dat: Wallet corrupted</source>
        <translation>wallet.dat钱包文件加载错误：钱包损坏</translation>
    </message>
    <message>
        <location line="-65"/>
        <source>Warning: error reading wallet.dat! All keys read correctly, but transaction data or address book entries might be missing or incorrect.</source>
        <translation>警告： 读取wallet.dat时出现错误！所有密钥均已正确解读，但转账数据或地址簿条目可能缺失或不正确。</translation>
    </message>
    <message>
        <location line="+66"/>
        <source>Error loading wallet.dat: Wallet requires newer version of Gridcoin</source>
        <translation>wallet.dat钱包文件加载错误：请升级到最新格雷德币客户端</translation>
    </message>
    <message>
        <location line="+134"/>
        <source>Vote</source>
        <translation type="unfinished">投票</translation>
    </message>
    <message>
        <location line="+2"/>
        <source>Wallet locked; </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Wallet needed to be rewritten: restart Gridcoin to complete</source>
        <translation>钱包文件需要重写：请退出并重新启动格雷德币客户端</translation>
    </message>
    <message>
        <location line="-139"/>
        <source>Error loading wallet.dat</source>
        <translation>wallet.dat钱包文件加载错误</translation>
    </message>
    <message>
        <location line="-22"/>
        <source>Cannot downgrade wallet</source>
        <translation>无法降级钱包</translation>
    </message>
    <message>
        <location line="+3"/>
        <source>Cannot write default address</source>
        <translation>无法写入缺省地址</translation>
    </message>
    <message>
        <location line="+103"/>
        <source>Rescanning...</source>
        <translation>正在重新扫描...</translation>
    </message>
    <message>
        <location line="-63"/>
        <source>Importing blockchain data file.</source>
        <translation>导入区块链数据文件。</translation>
    </message>
    <message>
        <location line="+1"/>
        <source>Importing bootstrap blockchain data file.</source>
        <translation>导入引导程序区块链数据文件。</translation>
    </message>
    <message>
        <location line="-13"/>
        <source>Error: could not start node</source>
        <translation>错误: 无法启动节点</translation>
    </message>
    <message>
        <location line="-15"/>
        <source>Done loading</source>
        <translation>加载完成</translation>
    </message>
    <message>
        <location line="-61"/>
        <source>Warning: Please check that your computer&apos;s date and time are correct! If your clock is wrong Gridcoin will not work properly.</source>
        <translation>警告：请确定您当前计算机的日期和时间是正确的！格雷德币将无法在错误的时间下正常工作。</translation>
    </message>
    <message>
        <location line="+207"/>
        <source>Warning: Disk space is low!</source>
        <translation>警告：磁盘剩余空间低!</translation>
    </message>
    <message>
        <location line="-136"/>
        <source>Error</source>
        <translation>错误</translation>
    </message>
</context>
</TS>
