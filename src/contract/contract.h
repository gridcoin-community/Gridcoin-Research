#pragma once

#include "fwd.h"
#include <string>

std::string SendContract(std::string sType, std::string sName, std::string sContract);
std::string SendMessage(bool bAdd, std::string sType, std::string sPrimaryKey, std::string sValue,
                    std::string sMasterKey, int64_t MinimumBalance, double dFees, std::string strPublicKey);
std::string GetBurnAddress();
std::string SignMessage(std::string sMsg, std::string sPrivateKey);
bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);
bool SignBlockWithCPID(const std::string& sCPID, const std::string& sBlockHash, std::string& sSignature, std::string& sError, bool bAdvertising=false);
bool CheckMessageSignature(std::string sAction,std::string messagetype, std::string sMsg, std::string sSig, std::string opt_pubkey);
std::string executeRain(std::string sRecipients);
bool MemorizeMessage(const CTransaction &tx, double dAmount, std::string sRecipient);
