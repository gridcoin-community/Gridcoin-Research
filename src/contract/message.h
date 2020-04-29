#pragma once

#include <string>

bool CheckMessageSignature(
    std::string sAction,
    std::string messagetype,
    std::string sMsg,
    std::string sSig,
    std::string opt_pubkey);

std::string SendContract(std::string sType, std::string sName, std::string sContract);

std::string SendMessage(
    bool bAdd,
    std::string sType,
    std::string sPrimaryKey,
    std::string sValue,
    std::string sMasterKey,
    int64_t MinimumBalance,
    double dFees,
    std::string strPublicKey);

std::string GetBurnAddress();

std::string SignMessage(std::string sMsg, std::string sPrivateKey);

std::string SignMessage(const std::string& sMsg, CKey& key);
