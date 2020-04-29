#pragma once

bool VerifyCPIDSignature(std::string sCPID, std::string sBlockHash, std::string sSignature);
bool SignBlockWithCPID(const std::string& sCPID, const std::string& sBlockHash, std::string& sSignature, std::string& sError, bool bAdvertising=false);
