#pragma once

#include <string>

bool VerifyCPIDSignature(
    const std::string& sCPID,
    const std::string& sBlockHash,
    const std::string& sSignature,
    const std::string& sBeaconPublicKey);

bool VerifyCPIDSignature(
    const std::string& sCPID,
    const std::string& sBlockHash,
    const std::string& sSignature);

bool SignBlockWithCPID(
    const std::string& sCPID,
    const std::string& sBlockHash,
    std::string& sSignature,
    std::string& sError,
    bool bAdvertising = false);
