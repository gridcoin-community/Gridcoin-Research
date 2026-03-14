// Copyright (c) 2014-2026 The Gridcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include "gridcoin/boinc_rpc.h"
#include "gridcoin/boinc.h"
#include "gridcoin/support/xml.h"
#include "util.h"
#include "fs.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>

using namespace GRC;

// -----------------------------------------------------------------------------
// Class: BoincRpcClient
// -----------------------------------------------------------------------------

BoincRpcClient::BoincRpcClient(
    const std::string& host,
    uint16_t port,
    const std::string& password)
    : m_host(host)
    , m_port(port)
    , m_password(password)
    , m_connected(false)
{
}

bool BoincRpcClient::Connect()
{
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::socket socket(io_context);
        
        auto endpoints = resolver.resolve(m_host, std::to_string(m_port));
        boost::asio::connect(socket, endpoints);
        
        // If password is empty, try to read from gui_rpc_auth.cfg
        if (m_password.empty()) {
            fs::path boinc_dir = GetBoincDataDir();
            if (!boinc_dir.empty()) {
                m_password = ReadRpcPassword(boinc_dir.string());
            }
        }
        
        m_connected = true;
        LogPrintf("BOINC RPC: Connected to %s:%d", m_host, m_port);
        return true;
    } catch (const std::exception& e) {
        LogPrintf("BOINC RPC: Connection failed: %s", e.what());
        m_connected = false;
        return false;
    }
}

void BoincRpcClient::Disconnect()
{
    m_connected = false;
    m_cookie.clear();
    LogPrintf("BOINC RPC: Disconnected");
}

bool BoincRpcClient::IsConnected() const
{
    return m_connected;
}

std::string BoincRpcClient::ReadRpcPassword(const std::string& boinc_data_dir)
{
    fs::path auth_file = fs::path(boinc_data_dir) / "gui_rpc_auth.cfg";
    
    try {
        if (fs::exists(auth_file)) {
            std::ifstream file(auth_file.string());
            if (file.is_open()) {
                std::string password;
                std::getline(file, password);
                boost::trim(password);
                LogPrintf("BOINC RPC: Read password from %s", auth_file.string());
                return password;
            }
        }
    } catch (const std::exception& e) {
        LogPrintf("BOINC RPC: Failed to read auth file: %s", e.what());
    }
    
    return "";
}

std::string BoincRpcClient::SendRpcRequest(const std::string& method, const std::string& params)
{
    if (!m_connected) {
        LogPrintf("BOINC RPC: Not connected");
        return "";
    }
    
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);
        
        auto endpoints = boost::asio::ip::tcp::resolver(io_context).resolve(m_host, std::to_string(m_port));
        boost::asio::connect(socket, endpoints);
        
        // Build XML-RPC request
        std::ostringstream request;
        request << "<?xml version=\"1.0\"?>\r\n";
        request << "<methodCall>\r\n";
        request << "<methodName>" << method << "</methodName>\r\n";
        
        if (!params.empty()) {
            request << "<params>" << params << "</params>\r\n";
        }
        
        request << "</methodCall>\r\n";
        
        // Build HTTP request
        std::string body = request.str();
        std::ostringstream http_request;
        http_request << "POST /api HTTP/1.1\r\n";
        http_request << "Host: " << m_host << ":" << m_port << "\r\n";
        http_request << "Content-Type: text/xml\r\n";
        http_request << "Content-Length: " << body.size() << "\r\n";
        
        if (!m_password.empty()) {
            // Simple password authentication (BOINC uses cookie-based auth)
            http_request << "Cookie: " << m_password << "\r\n";
        }
        
        http_request << "\r\n" << body;
        
        // Send request
        boost::asio::write(socket, boost::asio::buffer(http_request.str()));
        
        // Read response
        boost::asio::streambuf response_buffer;
        boost::asio::read_until(socket, response_buffer, "\r\n\r\n");
        
        std::istream response_stream(&response_buffer);
        std::string header_line;
        while (std::getline(response_stream, header_line) && header_line != "\r") {
            // Parse headers if needed
        }
        
        // Read body
        std::string body_str(boost::asio::buffers_begin(response_buffer.data()),
                             boost::asio::buffers_end(response_buffer.data()));
        
        return body_str;
    } catch (const std::exception& e) {
        LogPrintf("BOINC RPC: Request failed: %s", e.what());
        return "";
    }
}

std::vector<BoincProjectAccount> BoincRpcClient::GetProjectAccounts()
{
    std::vector<BoincProjectAccount> accounts;
    
    // BOINC RPC doesn't have a direct "get all projects" method
    // We need to read from client_state.xml instead
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return accounts;
    }
    
    fs::path client_state_file = boinc_dir / "client_state.xml";
    
    try {
        if (!fs::exists(client_state_file)) {
            return accounts;
        }
        
        std::ifstream file(client_state_file.string());
        if (!file.is_open()) {
            return accounts;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        // Split by <project> tags
        std::vector<std::string> project_xmls;
        size_t pos = 0;
        while ((pos = content.find("<project>", pos)) != std::string::npos) {
            size_t end_pos = content.find("</project>", pos);
            if (end_pos != std::string::npos) {
                project_xmls.push_back(content.substr(pos, end_pos - pos + 10));
                pos = end_pos + 10;
            } else {
                break;
            }
        }
        
        // Parse each project
        for (const auto& xml : project_xmls) {
            BoincProjectAccount account;
            account.project_name = ExtractXmlValue(xml, "project_name");
            account.project_url = ExtractXmlValue(xml, "master_url");
            account.account_name = ExtractXmlValue(xml, "user_name");
            account.internal_cpid = ExtractXmlValue(xml, "cross_project_id");
            account.external_cpid = ExtractXmlValue(xml, "external_cpid");
            account.email_hash = ExtractXmlValue(xml, "email_hash");
            account.team_name = ExtractXmlValue(xml, "team_name");
            
            std::string rac_str = ExtractXmlValue(xml, "user_expavg_credit");
            if (!rac_str.empty()) {
                try {
                    account.rac = std::stod(rac_str);
                } catch (...) {
                    account.rac = 0.0;
                }
            }
            
            std::string creation_str = ExtractXmlValue(xml, "creation_time");
            if (!creation_str.empty()) {
                try {
                    account.creation_time = std::stoll(creation_str);
                } catch (...) {
                    account.creation_time = 0;
                }
            }
            
            std::string attached_str = ExtractXmlValue(xml, "attached");
            account.attached = (attached_str == "1" || attached_str.empty());
            
            if (!account.project_name.empty()) {
                accounts.push_back(account);
            }
        }
    } catch (const std::exception& e) {
        LogPrintf("BOINC RPC: Failed to read projects: %s", e.what());
    }
    
    return accounts;
}

std::string BoincRpcClient::GetProjectState(const std::string& project_url)
{
    // Project state is stored in project_state.xml in the project directory
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return "";
    }
    
    // Extract hostname from URL for directory name
    std::string hostname = project_url;
    size_t scheme_pos = hostname.find("://");
    if (scheme_pos != std::string::npos) {
        hostname = hostname.substr(scheme_pos + 3);
    }
    size_t path_pos = hostname.find("/");
    if (path_pos != std::string::npos) {
        hostname = hostname.substr(0, path_pos);
    }
    
    fs::path project_dir = boinc_dir / "projects" / hostname;
    fs::path state_file = project_dir / "project_state.xml";
    
    try {
        if (fs::exists(state_file)) {
            std::ifstream file(state_file.string());
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                return buffer.str();
            }
        }
    } catch (const std::exception& e) {
        LogPrintf("BOINC RPC: Failed to read project state: %s", e.what());
    }
    
    return "";
}

bool BoincRpcClient::DetachProject(const std::string& project_url, bool remove_files)
{
    // BOINC RPC detach method
    std::string params = "<param><value><string>" + project_url + "</string></value></param>";
    if (remove_files) {
        params += "<param><value><boolean>1</boolean></value></param>";
    }
    
    std::string response = SendRpcRequest("project_detach", params);
    return response.find("<fault>") == std::string::npos;
}

bool BoincRpcClient::AttachProject(
    const std::string& project_url,
    const std::string& account_name,
    const std::string& account_key)
{
    // BOINC RPC attach method
    std::ostringstream params;
    params << "<param><value><string>" << project_url << "</string></value></param>";
    params << "<param><value><string>" << account_name << "</string></value></param>";
    params << "<param><value><string>" << account_key << "</string></value></param>";
    
    std::string response = SendRpcRequest("project_attach", params.str());
    return response.find("<fault>") == std::string::npos;
}

UniValue BoincRpcClient::GetClientState()
{
    UniValue result(UniValue::VOBJ);
    
    auto accounts = GetProjectAccounts();
    UniValue projects(UniValue::VARR);
    
    for (const auto& account : accounts) {
        UniValue project(UniValue::VOBJ);
        project.pushKV("name", account.project_name);
        project.pushKV("url", account.project_url);
        project.pushKV("account", account.account_name);
        project.pushKV("internal_cpid", account.internal_cpid);
        project.pushKV("external_cpid", account.external_cpid);
        project.pushKV("team", account.team_name);
        project.pushKV("rac", account.rac);
        project.pushKV("attached", account.attached);
        projects.push_back(project);
    }
    
    result.pushKV("projects", projects);
    return result;
}

UniValue BoincRpcClient::GetConfig()
{
    UniValue result(UniValue::VOBJ);
    
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return result;
    }
    
    fs::path config_file = boinc_dir / "cc_config.xml";
    
    try {
        if (fs::exists(config_file)) {
            std::ifstream file(config_file.string());
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                result.pushKV("config_xml", buffer.str());
            }
        }
    } catch (const std::exception& e) {
        LogPrintf("BOINC RPC: Failed to read config: %s", e.what());
    }
    
    return result;
}

std::string BoincRpcClient::ExtractXmlValue(const std::string& xml, const std::string& tag)
{
    std::string open_tag = "<" + tag + ">";
    std::string close_tag = "</" + tag + ">";
    
    size_t start = xml.find(open_tag);
    if (start == std::string::npos) {
        return "";
    }
    start += open_tag.length();
    
    size_t end = xml.find(close_tag, start);
    if (end == std::string::npos) {
        return "";
    }
    
    return xml.substr(start, end - start);
}

BoincProjectAccount BoincRpcClient::ParseProjectAccount(const std::string& xml)
{
    BoincProjectAccount account;
    account.project_name = ExtractXmlValue(xml, "project_name");
    account.project_url = ExtractXmlValue(xml, "master_url");
    account.account_name = ExtractXmlValue(xml, "user_name");
    account.internal_cpid = ExtractXmlValue(xml, "cross_project_id");
    account.external_cpid = ExtractXmlValue(xml, "external_cpid");
    account.email_hash = ExtractXmlValue(xml, "email_hash");
    account.team_name = ExtractXmlValue(xml, "team_name");
    
    std::string rac_str = ExtractXmlValue(xml, "user_expavg_credit");
    if (!rac_str.empty()) {
        try {
            account.rac = std::stod(rac_str);
        } catch (...) {
            account.rac = 0.0;
        }
    }
    
    std::string creation_str = ExtractXmlValue(xml, "creation_time");
    if (!creation_str.empty()) {
        try {
            account.creation_time = std::stoll(creation_str);
        } catch (...) {
            account.creation_time = 0;
        }
    }
    
    std::string attached_str = ExtractXmlValue(xml, "attached");
    account.attached = (attached_str == "1" || attached_str.empty());
    
    return account;
}

// -----------------------------------------------------------------------------
// Class: CpidSynchronizer
// -----------------------------------------------------------------------------

CpidSynchronizer::CpidSynchronizer(BoincRpcClient& boinc_rpc)
    : m_boinc_rpc(boinc_rpc)
{
}

PoolDetectionResult CpidSynchronizer::DetectPool(const std::vector<std::string>& known_pool_cpids)
{
    PoolDetectionResult result;
    
    auto projects = m_boinc_rpc.GetProjectAccounts();
    
    for (const auto& project : projects) {
        // Check if external CPID matches a known pool
        for (const auto& pool_cpid : known_pool_cpids) {
            if (project.external_cpid == pool_cpid) {
                result.is_pool = true;
                result.pool_cpid = pool_cpid;
                result.pool_projects.push_back(project.project_name);
                
                // Try to get pool name from project name or URL
                if (project.project_url.find("grcpool") != std::string::npos) {
                    result.pool_name = "grcpool.com";
                } else if (project.project_url.find("gridcoinpool") != std::string::npos) {
                    result.pool_name = "gridcoinpool.ru";
                } else {
                    result.pool_name = project.project_name;
                }
            }
        }
    }
    
    return result;
}

EmailValidationResult CpidSynchronizer::ValidateEmails(const std::vector<BoincProjectAccount>& projects)
{
    EmailValidationResult result;
    
    // Group projects by email hash
    std::map<std::string, std::vector<std::string>> email_to_projects;
    
    for (const auto& project : projects) {
        if (!project.email_hash.empty()) {
            email_to_projects[project.email_hash].push_back(project.project_name);
        }
    }
    
    result.email_to_projects = email_to_projects;
    
    // Check if all emails match
    if (email_to_projects.size() > 1) {
        result.all_match = false;
        
        // Find the most common email
        size_t max_count = 0;
        for (const auto& pair : email_to_projects) {
            if (pair.second.size() > max_count) {
                max_count = pair.second.size();
                result.primary_email = pair.first;
            }
            
            // Report inconsistencies
            if (pair.second.size() < max_count) {
                for (const auto& project : pair.second) {
                    result.inconsistencies.push_back(
                        "Project '" + project + "' uses different email");
                }
            }
        }
    } else if (email_to_projects.size() == 1) {
        result.primary_email = email_to_projects.begin()->first;
    }
    
    return result;
}

CpidMatchResult CpidSynchronizer::CheckCpidMatch(const std::vector<BoincProjectAccount>& projects)
{
    CpidMatchResult result;
    
    // Group projects by CPID
    std::map<std::string, std::vector<std::string>> cpid_to_projects;
    
    for (const auto& project : projects) {
        if (!project.external_cpid.empty()) {
            cpid_to_projects[project.external_cpid].push_back(project.project_name);
        }
    }
    
    result.cpid_to_projects = cpid_to_projects;
    
    // Check if all CPIDs match
    if (cpid_to_projects.size() > 1) {
        result.all_match = false;
        
        // Find the most common CPID
        size_t max_count = 0;
        for (const auto& pair : cpid_to_projects) {
            if (pair.second.size() > max_count) {
                max_count = pair.second.size();
                result.primary_cpid = pair.first;
            }
            
            // Report mismatches
            if (pair.second.size() < max_count) {
                for (const auto& project : pair.second) {
                    result.mismatches.push_back(
                        "Project '" + project + "' has CPID " + pair.first);
                }
            }
        }
    } else if (cpid_to_projects.size() == 1) {
        result.primary_cpid = cpid_to_projects.begin()->first;
    }
    
    return result;
}

OldestCpidInfo CpidSynchronizer::FindOldestCpid(const std::vector<BoincProjectAccount>& projects)
{
    OldestCpidInfo oldest;
    
    for (const auto& project : projects) {
        if (!project.external_cpid.empty()) {
            if (oldest.cpid.empty() || project.creation_time < oldest.creation_time) {
                oldest.cpid = project.external_cpid;
                oldest.project_name = project.project_name;
                oldest.creation_time = project.creation_time;
            }
        }
    }
    
    return oldest;
}

CpidSyncResult CpidSynchronizer::SyncCpids(
    const std::vector<BoincProjectAccount>& projects,
    const std::string& target_cpid,
    const std::string& email)
{
    CpidSyncResult result;
    
    if (target_cpid.empty()) {
        result.errors.push_back("Target CPID is empty");
        return result;
    }
    
    // Read client_state.xml
    auto project_xmls = ReadClientStateXml();
    if (project_xmls.empty()) {
        result.errors.push_back("Failed to read client_state.xml");
        return result;
    }
    
    std::string modified_content;
    bool any_updated = false;
    
    // Process each project XML section
    for (auto& xml : project_xmls) {
        std::string project_name = BoincRpcClient::ExtractXmlValue(xml, "project_name");
        std::string current_cpid = BoincRpcClient::ExtractXmlValue(xml, "external_cpid");
        
        // Update external CPID if different
        if (current_cpid != target_cpid) {
            // Replace external_cpid in XML
            std::string old_tag = "<external_cpid>" + current_cpid + "</external_cpid>";
            std::string new_tag = "<external_cpid>" + target_cpid + "</external_cpid>";
            
            size_t pos = xml.find(old_tag);
            if (pos != std::string::npos) {
                xml.replace(pos, old_tag.length(), new_tag);
                result.updated_projects.push_back(project_name);
                any_updated = true;
            }
        }
        
        modified_content += "<project>" + xml + "</project>\n";
    }
    
    // Write modified client_state.xml
    if (any_updated) {
        if (WriteClientStateXml(modified_content)) {
            result.success = true;
            result.resolved_cpid = target_cpid;
            result.message = "Successfully synchronized " + 
                            std::to_string(result.updated_projects.size()) + 
                            " project(s) to CPID " + target_cpid;
        } else {
            result.errors.push_back("Failed to write client_state.xml");
        }
    } else {
        result.success = true;
        result.resolved_cpid = target_cpid;
        result.message = "All projects already synchronized to CPID " + target_cpid;
    }
    
    return result;
}

bool CpidSynchronizer::UpdateProjectCpid(
    const std::string& project_name,
    const std::string& new_cpid,
    const std::string& email)
{
    auto projects = m_boinc_rpc.GetProjectAccounts();
    
    for (const auto& project : projects) {
        if (project.project_name == project_name) {
            return SyncCpids(projects, new_cpid, email).success;
        }
    }
    
    return false;
}

std::string CpidSynchronizer::GetClientStatePath()
{
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return "";
    }
    return (boinc_dir / "client_state.xml").string();
}

std::string CpidSynchronizer::GetProjectStatePath(const std::string& project_url)
{
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return "";
    }
    
    // Extract hostname from URL
    std::string hostname = project_url;
    size_t scheme_pos = hostname.find("://");
    if (scheme_pos != std::string::npos) {
        hostname = hostname.substr(scheme_pos + 3);
    }
    size_t path_pos = hostname.find("/");
    if (path_pos != std::string::npos) {
        hostname = hostname.substr(0, path_pos);
    }
    
    return (boinc_dir / "projects" / hostname / "project_state.xml").string();
}

std::vector<std::string> CpidSynchronizer::ReadClientStateXml()
{
    std::vector<std::string> projects;
    
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return projects;
    }
    
    fs::path client_state_file = boinc_dir / "client_state.xml";
    
    try {
        if (!fs::exists(client_state_file)) {
            return projects;
        }
        
        std::ifstream file(client_state_file.string());
        if (!file.is_open()) {
            return projects;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        // Split by <project> tags and extract content
        size_t pos = 0;
        while ((pos = content.find("<project>", pos)) != std::string::npos) {
            size_t start = pos + 9; // length of "<project>"
            size_t end = content.find("</project>", start);
            if (end != std::string::npos) {
                projects.push_back(content.substr(start, end - start));
                pos = end + 10; // length of "</project>"
            } else {
                break;
            }
        }
    } catch (const std::exception& e) {
        LogPrintf("CpidSynchronizer: Failed to read client_state.xml: %s", e.what());
    }
    
    return projects;
}

bool CpidSynchronizer::WriteClientStateXml(const std::string& content)
{
    fs::path boinc_dir = GetBoincDataDir();
    if (boinc_dir.empty()) {
        return false;
    }
    
    fs::path client_state_file = boinc_dir / "client_state.xml";
    
    try {
        // Create backup first
        fs::path backup_file = client_state_file;
        backup_file += ".backup";
        fs::copy_file(client_state_file, backup_file, fs::copy_options::overwrite_existing);
        
        // Write new content
        std::ofstream file(client_state_file.string());
        if (!file.is_open()) {
            return false;
        }
        
        file << content;
        file.close();
        
        LogPrintf("CpidSynchronizer: Updated client_state.xml");
        return true;
    } catch (const std::exception& e) {
        LogPrintf("CpidSynchronizer: Failed to write client_state.xml: %s", e.what());
        return false;
    }
}

std::string CpidSynchronizer::ComputeExternalCpid(
    const std::string& internal_cpid,
    const std::string& email)
{
    if (internal_cpid.empty() || email.empty()) {
        return "";
    }
    
    // External CPID is MD5(internal_cpid + email)
    // Note: internal_cpid should be the hex string, not decoded bytes
    std::string input = internal_cpid + email;
    
    unsigned char hash[16];
    GRC__MD5(reinterpret_cast<const unsigned char*>(input.data()),
             input.size(),
             hash);
    
    return HexStr(std::vector<unsigned char>(hash, hash + 16));
}
