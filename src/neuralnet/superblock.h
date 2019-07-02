#pragma once

#include "scraper/fwd.h"
#include <string>
#include "key.h"
#include "neuralnet/cpid.h"

std::string UnpackBinarySuperblock(std::string block);
std::string PackBinarySuperblock(std::string sBlock);

namespace NN {
class Superblock
{
public: /* fields */

    uint32_t nSBVersion;

    int64_t nTime;
    int64_t nHeight;

    ScraperStats mScraperSBStats;

    bool fReducedMapsPopulated = false;

    // ----- Project ---- ProjID
    std::map<std::string, uint16_t> mProjectRef;
    // ----- CPID ------- CPIDID
    std::map<NN::Cpid, uint32_t> mCPIDRef;

    // ------ ProjID ----------- avgRAC ---- RAC
    std::map<uint16_t, std::pair<uint64_t, uint64_t>> mProjectStats;
    // ------ CPIDID ----- Mag
    std::map<uint32_t, uint16_t> mCPIDMagnitudes;
    //------- ProjID ---------- CPIDID ------------- TC -------- RAC ----- Mag
    std::map<uint16_t, std::map<uint32_t, std::tuple<uint64_t, uint64_t, uint16_t>>> mProjectCPIDStats;

    uint32_t nZeroMagCPIDs;
    uint64_t nNetworkMagnitude;

    /*
    std::string sContractHash;
    std::string sContract;

    ConvergedManifest Convergence;

    std::vector<BeaconAcknowledgement> m_verified_beacons;


    */

    /*
    IMPLEMENT_SERIALIZE
    (
        READWRITE(m_version);
        READWRITE(m_projects);
        READWRITE(m_magnitudes);
        READWRITE(m_verified_beacons);
    )
    */

public: /* public methods */

    void PopulateReducedMaps();

    // This serializes the full map.
    void SerializeSuperblock(CDataStream& ss, int nType, int nVersion) const;
    void UnserializeSuperblock(CReaderStream& ss);

    // This is similar to legacy
    void SerializeSuperblock2(CDataStream& ss, int nType, int nVersion) const;
    void UnserializeSuperblock2(CReaderStream& ss);


    /*

    Superblock();
    int64_t Age() const;

    static Superblock UnpackLegacy(const std::string& binary);
    std::string PackLegacy() const;

    QuorumHash ComputeQuorumHash() const;
    double ComputeAverageMagnitude() const;



    */
};
}
