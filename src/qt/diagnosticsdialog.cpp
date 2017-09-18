#include "diagnosticsdialog.h"
#include "ui_diagnosticsdialog.h"

DiagnosticsDialog::DiagnosticsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiagnosticsDialog)
{
    ui->setupUi(this);

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, SIGNAL(finished(QNetworkReply *)), this, SLOT(getGithubVersionFinished(QNetworkReply *)));
}

DiagnosticsDialog::~DiagnosticsDialog()
{
    delete ui;
}

void DiagnosticsDialog::GetData() {
    std::string cpiddata = GetListOf("beacon");
    std::string sWhitelist = GetListOf("project");
    int64_t superblock_age = GetAdjustedTime() - mvApplicationCacheTimestamp["superblock;magnitudes"];
    double popularity = 0;
    std::string consensus_hash = GetNeuralNetworkSupermajorityHash(popularity);
    std::string sAge = ToString(superblock_age);
    std::string sBlock = mvApplicationCache["superblock;block_number"];
    std::string sTimestamp = TimestampToHRDate(mvApplicationCacheTimestamp["superblock;magnitudes"]);
    printf("Pushing diagnostic data...");
    double lastblockage = PreviousBlockAge();
    double PORDiff = GetDifficulty(GetLastBlockIndex(pindexBest, true));
    syncData = "<WHITELIST>" + sWhitelist + "</WHITELIST><CPIDDATA>"
        + cpiddata + "</CPIDDATA><QUORUMDATA><AGE>" + sAge + "</AGE><HASH>" + consensus_hash + "</HASH><BLOCKNUMBER>" + sBlock + "</BLOCKNUMBER><TIMESTAMP>"
        + sTimestamp + "</TIMESTAMP><PRIMARYCPID>" + KeyValue("cpid") + "</PRIMARYCPID><LASTBLOCKAGE>" + ToString(lastblockage) + "</LASTBLOCKAGE><DIFFICULTY>" + RoundToString(PORDiff,2) + "</DIFFICULTY></QUORUMDATA>";
    testnet_flag = fTestNet ? "TESTNET" : "MAINNET";
}

int DiagnosticsDialog::VarifyBoincPath() {
    boost::filesystem::path boincPath = (boost::filesystem::path) GetBoincDataDir();
    if(boincPath.empty())
        boincPath = KeyValue("boincdatadir");

    if(boost::filesystem::is_directory(boincPath))
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::FindCPID() {
    std::string cpid = KeyValue("cpid");

    if(cpid.length() != 0)
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::VarifyIsCPIDValid() {
    boost::filesystem::path clientStatePath = (boost::filesystem::path) GetBoincDataDir();
    if(!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";
    else
        clientStatePath = KeyValue("boincdatadir");

    if(clientStatePath.empty())
        return 0;

    boost::filesystem::ifstream clientStateStream;
    std::string clientState;

    clientStateStream.open(clientStatePath);
    clientState.assign((std::istreambuf_iterator<char>(clientStateStream)), std::istreambuf_iterator<char>());
    clientStateStream.close();

    size_t pos = 0;
    std::string cpid;
    if((pos = clientState.find("<external_cpid>")) != std::string::npos) {
        cpid = clientState.substr(pos + 15, clientState.length());
        pos = cpid.find("</external_cpid>");
        cpid.erase(pos, cpid.length());
    }

    if(KeyValue("cpid") == cpid)
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork() {
    std::string beacons = ExtractXML(syncData, "<CPIDDATA>", "</CPIDDATA>");
    boost::algorithm::to_lower(beacons);

    if(beacons.length() < 100)
        return -1;
    std::string cpid = KeyValue("cpid");
    if(cpid != "" && beacons.find(cpid) != std::string::npos)
        return 1;
    else
        return 0;
}

int DiagnosticsDialog::VarifyWalletIsSynced() {
    std::string walletAgeString = ExtractXML(syncData, "<LASTBLOCKAGE>", "</LASTBLOCKAGE>");
    long int walletAge = std::stoi(walletAgeString,nullptr,10);

    if(walletAgeString.length() == 0)
        return -1;
    if(walletAge < (60 * 60))
        return 1;
    else
        return 0;

}

int DiagnosticsDialog::VarifyCPIDHasRAC() {
    boost::filesystem::path clientStatePath = (boost::filesystem::path) GetBoincDataDir();
    if(!clientStatePath.empty())
        clientStatePath = clientStatePath / "client_state.xml";
    else
        clientStatePath = KeyValue("boincdatadir");

    if(clientStatePath.empty())
        return 0;

    boost::filesystem::ifstream clientStateStream;
    std::string clientState;
    std::vector<std::string> racStrings;
    std::vector<int> racValues;

    clientStateStream.open(clientStatePath);
    clientState.assign((std::istreambuf_iterator<char>(clientStateStream)), std::istreambuf_iterator<char>());
    clientStateStream.close();

    if(clientState.length() > 0) {
        size_t pos = 0;
        std::string delim = "</user_expavg_credit>";
        std::string line;
        while ((pos = clientState.find(delim)) != std::string::npos) {
            line = clientState.substr(0, pos);
            clientState.erase(0, pos + delim.length());
            pos = line.find("<user_expavg_credit>");
            racStrings.push_back(line.substr(pos + 20, line.length()));
        }
        for(std::string racString : racStrings) {
            racValues.push_back(std::stod(racString, nullptr));
        }
        return (int) std::accumulate(racValues.begin(), racValues.end(), 0);
    } else
        return 0;

}

int DiagnosticsDialog::VarifyAddNode() {
    long int peersLength = boost::filesystem::file_size(GetDataDir(true) / "peers.dat");
    std::string addNode = DiagnosticsDialog::KeyValue("addnode");

    if(addNode.length() == 0 && peersLength > 100)
        return 2;
    if(addNode.length() > 4 && peersLength == 0)
        return 3;
    if(peersLength > 100)
        return 1;
    if(peersLength == 0 && addNode == "")
        return -1;
    return 0;

}

void DiagnosticsDialog::VarifyClock() {
    QHostInfo info = QHostInfo::fromName("pool.ntp.org");
    udpSocket = new QUdpSocket(this);

    udpSocket->bind(123);

    char sendPkt[] = {010, 0, 0, 0, 0, 0, 0, 0,0};
    QByteArray sendBuffer(sendPkt);
    udpSocket->writeDatagram(sendBuffer, info.addresses().first(), 123);

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(clkFinished()));

}



void DiagnosticsDialog::VarifyTCPPort() {
    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, SIGNAL(connected()), this, SLOT(TCPFinished()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(TCPFailed(QAbstractSocket::SocketError)));

    tcpSocket->connectToHost("london.grcnode.co.uk", 32749);

}

std::string DiagnosticsDialog::KeyValue(std::string key) {
    boost::filesystem::path confFile = GetDataDir(true) / "gridcoinresearch.conf";
    std::vector<std::string> confKeys;
    boost::filesystem::ifstream configStream;

    configStream.open(confFile);
    for(std::string line; std::getline(configStream, line);) {
        boost::algorithm::split(confKeys, line, boost::is_any_of("="));
        if(boost::to_lower_copy(confKeys.at(0)) == boost::to_lower_copy(key)) {
            std::string value = confKeys.at(1);
            if(value.back() == '\r')
                value.pop_back();
            return value;
        }
    }
    return "";
}

void DiagnosticsDialog::on_testBtn_clicked() {
    int result = 0;
    DiagnosticsDialog::GetData();
    //boinc path
    ui->boincPathResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyBoincPath();
    if(result == 1)
        ui->boincPathResultLbl->setText("Passed");
    else
        ui->boincPathResultLbl->setText("Failed");
    //find cpid
    ui->findCPIDReaultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::FindCPID();
    if(result == 1)
        ui->findCPIDReaultLbl->setText(QString::fromStdString("Passed CPID: " + DiagnosticsDialog::KeyValue("cpid")));
    else
        ui->findCPIDReaultLbl->setText("Failed (Is CPID in gridcoinresearch.conf?)");
    //cpid valid
    ui->varifyCPIDValidResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyIsCPIDValid();
    if(result == 1)
        ui->varifyCPIDValidResultLbl->setText("Passed");
    else
        ui->varifyCPIDValidResultLbl->setText("Failed (BOINC CPID and gridcoinresearch.conf CPID do not match)");
    //cpid has rac
    ui->varifyCPIDHasRACResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyCPIDHasRAC();
    if(result > 0)
        ui->varifyCPIDHasRACResultLbl->setText(QString::fromStdString("Passed RAC: " + std::to_string(result)));
    else
        ui->varifyCPIDHasRACResultLbl->setText("Failed");
    //cpid is in nn
    ui->varifyCPIDIsInNNResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VerifyCPIDIsInNeuralNetwork();
    if(result == 1)
        ui->varifyCPIDIsInNNResultLbl->setText("Passed");
    else if (result == -1)
        ui->varifyCPIDIsInNNResultLbl->setText("Beacon data empty, sync wallet");
    else
        ui->varifyCPIDIsInNNResultLbl->setText("Failed");
    //wallet synced
    ui->varifyWalletIsSyncedResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyWalletIsSynced();
    if(result == 1)
        ui->varifyWalletIsSyncedResultLbl->setText("Passed");
    else if (result == -1)
        ui->varifyWalletIsSyncedResultLbl->setText("Sync data empty");
    else
        ui->varifyWalletIsSyncedResultLbl->setText("Failed");
    //clock
    ui->varifyClkResultLbl->setText("Testing...");
    this->repaint();
    DiagnosticsDialog::VarifyClock();
    //addnode
    ui->varifyAddnodeResultLbl->setText("Testing...");
    this->repaint();
    result = DiagnosticsDialog::VarifyAddNode();
    if(result == 1)
        ui->varifyAddnodeResultLbl->setText("Passed");
    else if(result == 2)
        ui->varifyAddnodeResultLbl->setText("Passed (Addnode does not exist but peers are synced)");
    else if(result == 3)
        ui->varifyAddnodeResultLbl->setText("Passed (Addnode exists but peers not synced)");
    else if(result == -1)
        ui->varifyAddnodeResultLbl->setText("Failed (Please add addnode=node.gridcoin.us in conf file)");
    else
        ui->varifyAddnodeResultLbl->setText("Failed");
    //tcp port
    ui->varifyTCPPortResultLbl->setText("Testing...");
    this->repaint();
    DiagnosticsDialog::VarifyTCPPort();
    //client version
    ui->checkClientVersionResultLbl->setText("Testing...");
    networkManager->get(QNetworkRequest(QUrl("https://api.github.com/repos/gridcoin/Gridcoin-Research/releases/latest")));

}

void DiagnosticsDialog::clkFinished() {
    time_t ntpTime;

    while(udpSocket->hasPendingDatagrams()) {
        QByteArray recvBuffer = udpSocket->readAll();
        if(recvBuffer.size() == 48){
            int count= 40;
            unsigned long DateTimeIn= uchar(recvBuffer.at(count))+ (uchar(recvBuffer.at(count+ 1)) << 8)+ (uchar(recvBuffer.at(count+ 2)) << 16)+ (uchar(recvBuffer.at(count+ 3)) << 24);
            ntpTime = ntohl((time_t)DateTimeIn);
            ntpTime -= 2208988800U;
        }
    }

    udpSocket->close();

    boost::posix_time::ptime localTime = boost::posix_time::microsec_clock::universal_time();
    boost::posix_time::ptime networkTime = boost::posix_time::from_time_t(ntpTime);

    boost::posix_time::time_duration timeDiff = networkTime - localTime;

    if(timeDiff.minutes() < 3)
        ui->varifyClkResultLbl->setText("Passed");
    else
        ui->varifyClkResultLbl->setText("Failed (Sync local time with network)");

    this->repaint();
}

void DiagnosticsDialog::TCPFinished() {
    tcpSocket->close();
    ui->varifyTCPPortResultLbl->setText("Passed");
    this->repaint();
}

void DiagnosticsDialog::TCPFailed(QAbstractSocket::SocketError socket) {
    ui->varifyTCPPortResultLbl->setText("Failed (Port 32749 may be blocked by your firewall)");
    this->repaint();
}

void DiagnosticsDialog::getGithubVersionFinished(QNetworkReply *reply) {
    QByteArray data;
    data = reply->readAll();
    std::string newVersionString;

    QJsonDocument replyJson = QJsonDocument::fromJson(data);
    QJsonObject obj = replyJson.object();
    if(!obj.empty()) {
        QJsonValue newVersionJVal = obj.value("name");
        if(!newVersionJVal.isUndefined()) {
            newVersionString = newVersionJVal.toString().toStdString();
        }
    }

    std::vector<std::string> newVersionStringSplit;
    boost::algorithm::split(newVersionStringSplit, newVersionString, boost::is_any_of("-"));
    newVersionString = newVersionStringSplit.at(0);

    std::string currentVersionString = FormatFullVersion();
    std::vector<std::string> currentVersionStringSplit;
    boost::algorithm::split(currentVersionStringSplit, currentVersionString, boost::is_any_of("-"));
    currentVersionString = currentVersionStringSplit.at(0);
    boost::algorithm::split(currentVersionStringSplit, currentVersionString, boost::is_any_of("v"));
    currentVersionString = currentVersionStringSplit.at(1);

    std::vector<std::string> currentVersionList;
    std::vector<std::string> newVersionList;

    boost::algorithm::split(currentVersionList, currentVersionString, boost::is_any_of("."));
    boost::algorithm::split(newVersionList, newVersionString, boost::is_any_of("."));

    int iNew;
    int iCurrent;
    for(unsigned int i=0; i<newVersionList.size(); i++) {
        iNew = std::stoi(newVersionList.at(i), nullptr, 10);
        iCurrent = std::stoi(currentVersionList.at(i), nullptr, 10);
        if(iNew > iCurrent) {
            ui->checkClientVersionResultLbl->setText("Faild (An Update is available, please update)");
            return;
        } else
            ui->checkClientVersionResultLbl->setText("Up to date");
    }
}
