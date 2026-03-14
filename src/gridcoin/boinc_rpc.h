// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#ifndef GRIDCOIN_BOINC_RPC_H
#define GRIDCOIN_BOINC_RPC_H

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <univalue.h>

namespace GRC {

//!
//! \brief Represents a BOINC project account with CPID information.
//!
struct BoincProjectAccount
{
    std::string project_name;      //!< Name of the BOINC project
    std::string project_url;       //!< Master URL of the project
    std::string account_name;      //!< Account username
    std::string internal_cpid;     //!< Internal/cross project ID
    std::string external_cpid;     //!< External CPID (MD5 of internal + email)
    std::string email_hash;        //!< MD5 hash of account email
    std::string team_name;         //!< Team name if joined
    double rac;                    //!< Recent Average Credit
    int64_t creation_time;         //!< Account creation timestamp
    bool attached;                 //!< Whether account is attached
    
    BoincProjectAccount() 
        : rac(0.0), creation_time(0), attached(false) {}
};

//!
//! \brief Result of CPID synchronization operation.
//!
struct CpidSyncResult
{
    bool success;                          //!< Whether sync succeeded
    std::string message;                   //!< Human-readable result message
    std::string resolved_cpid;             //!< The CPID that was resolved/synced
    std::vector<std::string> updated_projects; //!< Projects that were updated
    std::vector<std::string> errors;       //!< Any errors encountered
    
    CpidSyncResult() : success(false) {}
};

//!
//! \brief Result of pool detection check.
//!
struct PoolDetectionResult
{
    bool is_pool;                          //!< Whether pool attachment detected
    std::string pool_name;                 //!< Name of detected pool
    std::string pool_cpid;                 //!< Pool's CPID
    std::vector<std::string> pool_projects; //!< Projects attached to pool
    
    PoolDetectionResult() : is_pool(false) {}
};

//!
//! \brief Result of email validation check.
//!
struct EmailValidationResult
{
    bool all_match;                        //!< Whether all emails match
    std::string primary_email;             //!< Most common email found
    std::map<std::string, std::vector<std::string>> email_to_projects; //!< Email -> projects mapping
    std::vector<std::string> inconsistencies; //!< List of inconsistencies found
    
    EmailValidationResult() : all_match(true) {}
};

//!
//! \brief Result of CPID matching check.
//!
struct CpidMatchResult
{
    bool all_match;                        //!< Whether all CPIDs match
    std::string primary_cpid;              //!< Most common CPID found
    std::map<std::string, std::vector<std::string>> cpid_to_projects; //!< CPID -> projects mapping
    std::vector<std::string> mismatches;   //!< List of mismatched projects
    
    CpidMatchResult() : all_match(true) {}
};

//!
//! \brief Represents the oldest CPID found during resolution.
//!
struct OldestCpidInfo
{
    std::string cpid;                      //!< The oldest CPID
    std::string project_name;              //!< Project where oldest CPID was found
    int64_t creation_time;                 //!< Creation timestamp
    std::string email;                     //!< Email associated with the CPID
    
    OldestCpidInfo() : creation_time(0) {}
};

//!
//! \brief Provides communication with BOINC client via HTTP RPC.
//!
//! This class implements the BOINC HTTP RPC protocol to:
//! - Query project and account information
//! - Manage project attachments
//! - Retrieve CPID and email information
//!
//! BOINC RPC reference: https://boinc.berkeley.edu/trac/wiki/ClientRpc
//!
class BoincRpcClient
{
public:
    //!
    //! \brief Initialize the BOINC RPC client.
    //!
    //! \param host RPC host (default: localhost)
    //! \param port RPC port (default: 1043)
    //! \param password RPC password (optional, from gui_rpc_auth.cfg)
    //!
    BoincRpcClient(
        const std::string& host = "localhost",
        uint16_t port = 1043,
        const std::string& password = "");
    
    //!
    //! \brief Connect to the BOINC client.
    //!
    //! \return true if connection successful.
    //!
    bool Connect();
    
    //!
    //! \brief Disconnect from the BOINC client.
    //!
    void Disconnect();
    
    //!
    //! \brief Check if connected to BOINC client.
    //!
    //! \return true if connected.
    //!
    bool IsConnected() const;
    
    //!
    //! \brief Get all project accounts from BOINC.
    //!
    //! \return Vector of project account information.
    //!
    std::vector<BoincProjectAccount> GetProjectAccounts();
    
    //!
    //! \brief Get project state XML for a specific project.
    //!
    //! \param project_url Master URL of the project.
    //! \return Project state XML string.
    //!
    std::string GetProjectState(const std::string& project_url);
    
    //!
    //! \brief Detach from a project.
    //!
    //! \param project_url Master URL of the project.
    //! \param remove_files Whether to remove project files.
    //! \return true if detach successful.
    //!
    bool DetachProject(const std::string& project_url, bool remove_files = false);
    
    //!
    //! \brief Attach to a project.
    //!
    //! \param project_url Master URL of the project.
    //! \param account_name Account username.
    //! \param account_key Account key.
    //! \return true if attach successful.
    //!
    bool AttachProject(
        const std::string& project_url,
        const std::string& account_name,
        const std::string& account_key);
    
    //!
    //! \brief Get BOINC client state.
    //!
    //! \return Client state information as UniValue.
    //!
    UniValue GetClientState();
    
    //!
    //! \brief Get BOINC configuration.
    //!
    //! \return Configuration as UniValue.
    //!
    UniValue GetConfig();
    
    //!
    //! \brief Read the gui_rpc_auth.cfg file to get the RPC password.
    //!
    //! \param boinc_data_dir BOINC data directory path.
    //! \return RPC password if found.
    //!
    static std::string ReadRpcPassword(const std::string& boinc_data_dir);
    
private:
    std::string m_host;
    uint16_t m_port;
    std::string m_password;
    std::string m_cookie;
    bool m_connected;
    
    //!
    //! \brief Send an RPC request to BOINC.
    //!
    //! \param method RPC method name.
    //! \param params Parameters for the method.
    //! \return Response XML string.
    //!
    std::string SendRpcRequest(const std::string& method, const std::string& params = "");
    
    //!
    //! \brief Parse project account information from XML.
    //!
    //! \param xml Project account XML.
    //! \return Parsed project account.
    //!
    BoincProjectAccount ParseProjectAccount(const std::string& xml);
    
    //!
    //! \brief Extract XML element value.
    //!
    //! \param xml XML string.
    //! \param tag Element tag name.
    //! \return Element value or empty string.
    //!
    static std::string ExtractXmlValue(const std::string& xml, const std::string& tag);
};

//!
//! \brief Provides CPID synchronization utilities.
//!
//! This class handles:
//! - Pool detection
//! - Email validation
//! - CPID matching and resolution
//! - XML file modification
//!
class CpidSynchronizer
{
public:
    //!
    //! \brief Initialize the CPID synchronizer.
    //!
    //! \param boinc_rpc BOINC RPC client instance.
    //!
    explicit CpidSynchronizer(BoincRpcClient& boinc_rpc);
    
    //!
    //! \brief Detect if user is attached to a Gridcoin pool.
    //!
    //! \param known_pool_cpids Set of known pool CPIDs.
    //! \return Pool detection result.
    //!
    PoolDetectionResult DetectPool(const std::vector<std::string>& known_pool_cpids);
    
    //!
    //! \brief Validate that all BOINC accounts use the same email.
    //!
    //! \param projects List of project accounts.
    //! \return Email validation result.
    //!
    EmailValidationResult ValidateEmails(const std::vector<BoincProjectAccount>& projects);
    
    //!
    //! \brief Check if all CPIDs match across projects.
    //!
    //! \param projects List of project accounts.
    //! \return CPID match result.
    //!
    CpidMatchResult CheckCpidMatch(const std::vector<BoincProjectAccount>& projects);
    
    //!
    //! \brief Find the oldest CPID by comparing account creation dates.
    //!
    //! \param projects List of project accounts.
    //! \return Information about the oldest CPID.
    //!
    OldestCpidInfo FindOldestCpid(const std::vector<BoincProjectAccount>& projects);
    
    //!
    //! \brief Synchronize CPIDs by writing the oldest CPID to all project XML files.
    //!
    //! \param projects List of project accounts.
    //! \param target_cpid The CPID to sync to.
    //! \param email Email address for CPID hash computation.
    //! \return Synchronization result.
    //!
    CpidSyncResult SyncCpids(
        const std::vector<BoincProjectAccount>& projects,
        const std::string& target_cpid,
        const std::string& email);
    
    //!
    //! \brief Update external CPID in a project's XML file.
    //!
    //! \param project_name Name of the project.
    //! \param new_cpid New external CPID to write.
    //! \param email Email for CPID validation.
    //! \return true if update successful.
    //!
    bool UpdateProjectCpid(
        const std::string& project_name,
        const std::string& new_cpid,
        const std::string& email);
    
    //!
    //! \brief Get the path to BOINC's client_state.xml file.
    //!
    //! \return Path to client_state.xml.
    //!
    static std::string GetClientStatePath();
    
    //!
    //! \brief Get the path to a project's project_state.xml file.
    //!
    //! \param project_url Project master URL.
    //! \return Path to project_state.xml.
    //!
    static std::string GetProjectStatePath(const std::string& project_url);
    
private:
    BoincRpcClient& m_boinc_rpc;
    
    //!
    //! \brief Read and parse client_state.xml.
    //!
    //! \return Vector of project XML strings.
    //!
    std::vector<std::string> ReadClientStateXml();
    
    //!
    //! \brief Write modified client_state.xml.
    //!
    //! \param content New XML content.
    //! \return true if write successful.
    //!
    bool WriteClientStateXml(const std::string& content);
    
    //!
    //! \brief Compute external CPID from internal CPID and email.
    //!
    //! \param internal_cpid Internal CPID (hex string).
    //! \param email Email address.
    //! \return External CPID (hex string).
    //!
    static std::string ComputeExternalCpid(
        const std::string& internal_cpid,
        const std::string& email);
};

} // namespace GRC

#endif // GRIDCOIN_BOINC_RPC_H
