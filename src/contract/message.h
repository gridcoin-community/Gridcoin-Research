#pragma once

#include <string>

class CWalletTx;

namespace NN { class Contract; }

std::pair<CWalletTx, std::string> SendContract(NN::Contract contract);

std::string SendPublicContract(NN::Contract contract);
