Imports System.Runtime.InteropServices
Imports System.Drawing
Imports System.Runtime.CompilerServices
Imports System.IO
Imports System.Reflection
Imports System.Net
Imports System.Text
Imports System.Security.Cryptography
Imports ICSharpCode.SharpZipLib.Zip
Imports ICSharpCode.SharpZipLib.Core
Imports System.Windows.Forms

Module modGRC

    Public Structure BatchJob
        Dim Value As String
        Dim Status As Boolean
        Dim OutputError As String
    End Structure
    Private prodURL As String = "http://download.gridcoin.us/download/downloadstake/"
    Private testURL As String = "http://download.gridcoin.us/download/downloadstaketestnet/"
    Public mRowIndex As Long = 0

    Public msGenericDictionary As New Dictionary(Of String, String)
    Public msRPCCommand As String = ""
    Public msRPCReply As String = ""
    Public mclsUtilization As Utilization
    Public mfrmMining As frmMining
    Public mfrmProjects As frmNewUserWizard
    Public mfrmSql As frmSQL
    Public mfrmFAQ As frmFAQ
    Public mfrmConfig As frmConfiguration

    Public mfrmTicketAdd As frmTicketAdd
    Public mfrmFoundation As frmFoundation
    Public mFrmDiagnostics As frmDiagnostics

    Public mfrmTicketList As frmTicketList
    Public mfrmLogin As frmLogin
    Public mfrmTicker As frmLiveTicker
    Public mfrmWireFrame As frmGRCWireFrameCanvas
    Public mfrmLeaderboard As frmLeaderboard
    Public MerkleRoot As String = "0xda43abf15a2fcd57ceae9ea0b4e0d872981e2c0b72244466650ce6010a14efb8"
    Public merkleroot2 As String = "0xda43abf15abcdefghjihjklmnopq872981e2c0b72244466650ce6010a14efb8"

    Structure xGridcoinMiningStructure
        Public Shared device As String = "0"
        Public Shared gpu_thread_concurrency As String = "8192"
        Public Shared worksize As String = "256"
        Public Shared intensity As String = "13"
        Public Shared lookup_gap As String = "2"
    End Structure
    Public Function GetFoundationGuid(sTitle As String) As String
        Dim lPos As Long = InStr(1, LCase(sTitle), "[foundation ")
        Dim sId As String = ""
        If lPos > 0 Then sId = Mid(sTitle, lPos + Len("[foundation "), 36)
        If Len(Trim(sId)) <> 36 Then Return ""
        Return sId
    End Function
    Public Function NeedsUpgrade() As Boolean
        Try
            Dim sLocalPath As String = GetGRCAppDir() + "\"
            Dim dr As SqlClient.SqlDataReader
            Dim oGrcData As New GRCSec.GridcoinData
            dr = oGrcData.mGetUpgradeFiles
            Dim bNeedsUpgraded As Boolean = False
            Do While dr.Read
                Dim sFile As String = LCase("" & dr("filename"))
                If sFile Like "*gridcoinresearch.exe*" Or sFile Like "*boincstake.dll*" Then
                    'Get local hash
                    Dim sLocalHash As String = GetMd5OfFile(sLocalPath + sFile)
                    Dim sRemoteHash As String = dr("Hash")
                    Dim bNeeds As Boolean = sLocalHash <> sRemoteHash
                    If bNeeds Then bNeedsUpgraded = True
                End If
            Loop
            Return bNeedsUpgraded
        Catch ex As Exception
            Return False
        End Try
    End Function

    Public Function ExecuteRPCCommand(sCommand As String, sArg1 As String, sArg2 As String, sArg3 As String, sArg4 As String, sArg5 As String, sURL As String) As String
        Dim sReply As String = ""
        Try
            Dim sPayload As String = "<COMMAND>" + sCommand + "</COMMAND><ARG1>" + sArg1 + "</ARG1><ARG2>" + sArg2 + "</ARG2><ARG3>" + sArg3 + "</ARG3><ARG4>" + sArg4 + "</ARG4><ARG5>" + sArg5 + "</ARG5>"
            If sCommand = "addpoll" Then sPayload += "<URL>" + sURL + "</URL>"

            SetRPCReply("")
            msRPCReply = ""
            msRPCCommand = sPayload
            'Busy Wait
            For x As Integer = 1 To 60
                sReply = msRPCReply
                If sReply <> "" Then Return sReply
                Threading.Thread.Sleep(250) '1/4 sec sleep
                Application.DoEvents()
            Next
            sReply = "Unable to reach Gridcoin RPC."
            Return sReply
        Catch ex As Exception
            Log("EXCEPTION: ExecuteRPCCommand : " + ex.Message)
            Return "Unable to execute vote, " + ex.Message
        End Try
    End Function
    Public Sub AddHeading(iPosition As Integer, sName As String, oDGV As DataGridView, bAutoFit As Boolean)

        Dim dc As New System.Windows.Forms.DataGridViewColumn
        dc.Name = sName
        Dim dgvct As New System.Windows.Forms.DataGridViewTextBoxCell
        dgvct.Style.BackColor = Drawing.Color.Black
        dgvct.Style.ForeColor = Drawing.Color.Lime
        dc.CellTemplate = dgvct
        oDGV.Columns.Add(dc)

        Dim dgcc As New DataGridViewCellStyle
        dgcc.ForeColor = System.Drawing.Color.SandyBrown
        oDGV.ColumnHeadersDefaultCellStyle = dgcc

        oDGV.Columns(iPosition).AutoSizeMode = DataGridViewAutoSizeColumnMode.AllCells
        oDGV.Columns(iPosition).SortMode = DataGridViewColumnSortMode.Automatic

    End Sub

    Public Sub PopulateHeadings(vHeading() As String, oDGV As DataGridView, bAutoFit As Boolean)

        For x = 0 To UBound(vHeading)
            Dim dc As New System.Windows.Forms.DataGridViewColumn
            dc.Name = vHeading(x)
            Dim dgvct As New System.Windows.Forms.DataGridViewTextBoxCell
            dgvct.Style.BackColor = Drawing.Color.Black
            dgvct.Style.ForeColor = Drawing.Color.Lime
            dc.CellTemplate = dgvct
            oDGV.Columns.Add(dc)
        Next x
        Dim dgcc As New DataGridViewCellStyle
        dgcc.ForeColor = System.Drawing.Color.SandyBrown
        oDGV.ColumnHeadersDefaultCellStyle = dgcc
    
        For x = 0 To UBound(vHeading)
            If bAutoFit Then
                oDGV.Columns(x).AutoSizeMode = DataGridViewAutoSizeColumnMode.AllCells
                oDGV.Columns(x).SortMode = DataGridViewColumnSortMode.Automatic
            Else
                oDGV.Columns(x).AutoSizeMode = DataGridViewAutoSizeColumnMode.None
                oDGV.Columns(x).SortMode = DataGridViewColumnSortMode.Automatic
            End If
        Next

    End Sub
    Public Sub SetAutoSizeMode2(vHeading() As String, oDGV As DataGridView)
        For x = 0 To UBound(vHeading)
            oDGV.Columns(x).AutoSizeMode = DataGridViewAutoSizeColumnMode.AllCells
        Next

    End Sub
    Private Function TruncateHash(ByVal key As String, ByVal length As Integer) As Byte()
        Dim sha1 As New SHA1CryptoServiceProvider
        ' Hash the key. 
        Dim keyBytes() As Byte =
            System.Text.Encoding.Unicode.GetBytes(key)
        Dim hash() As Byte = sha1.ComputeHash(keyBytes)
        ' Truncate or pad the hash. 
        ReDim Preserve hash(length - 1)
        Return hash
    End Function

    Private Function CreateAESEncryptor(ByVal sSalt As String) As ICryptoTransform

        Try
            Dim encryptor As ICryptoTransform = Aes.Create.CreateEncryptor(TruncateHash(MerkleRoot + Right(sSalt, 4), Aes.Create.KeySize \ 8), TruncateHash("", Aes.Create.BlockSize \ 8))
            Return encryptor
        Catch ex As Exception
            Throw ex
        End Try
    End Function

    Private Function CreateAESDecryptor(ByVal sSalt As String) As ICryptoTransform

        Try
            Dim decryptor As ICryptoTransform = Aes.Create.CreateDecryptor(TruncateHash(MerkleRoot + Right(sSalt, 4), Aes.Create.KeySize \ 8), TruncateHash("", Aes.Create.BlockSize \ 8))
            Return decryptor
        Catch ex As Exception
            Throw ex
        End Try
    End Function

    Public Function StringToByte(sData As String)
        Dim keyBytes() As Byte = System.Text.Encoding.Unicode.GetBytes(sData)
        Return keyBytes
    End Function
    Public Function xReturnMiningValue(iDeviceId As Integer, sKey As String, bUsePrefix As Boolean) As String
        Dim g As New xGridcoinMiningStructure
        Dim sOut As String = ""
        Dim sLookupKey As String = LCase("dev" + Trim(iDeviceId) + "_" + sKey)
        If Not bUsePrefix Then sLookupKey = LCase(sKey)
        sOut = KeyValue(sLookupKey)
        Return sOut
    End Function
    Public Function GetGRCAppDir() As String
        Try
            Dim fi As New System.IO.FileInfo(Assembly.GetExecutingAssembly().Location)
            Return fi.DirectoryName
        Catch ex As Exception
            Return ""
        End Try
    End Function
    Public Function BoincSetTeamID(sProjectURL As String, sAccountKey As String, sTeamID As String) As BatchJob
        Dim BJ As New BatchJob
        Dim myWebClient As New MyWebClient()
        Dim sFullURL As String = sProjectURL + "am_set_info.php?account_key=" + sAccountKey + "&teamid=" + sTeamID
        Dim sHttp As String

        Try
            sHttp = myWebClient.DownloadString(sFullURL)

        Catch ex As Exception

        End Try
        If sHTTP.Contains("success") Then
            BJ.Status = True
        Else
            BJ.Status = False
        End If
        Return BJ

    End Function

    Public Function BoincAttachProject(sProjectURL As String, sAccountKey As String) As BatchJob
        Dim sOut As String = BoincCommand("--project_attach " + sProjectURL + " " + sAccountKey)
        Dim BJ As New BatchJob
        If sOut Is Nothing Then
            BJ.Status = False
            BJ.Value = "command failed"
            Return BJ
        End If

        If sOut = "" Then
            BJ.Status = True
            BJ.Value = "success"
            Return BJ
        End If

        If sOut.Contains("account key:") Then
            BJ.Status = True
            Dim vSplit() As String
            vSplit = Split(sOut, "account key:")
            BJ.Value = Trim(vSplit(1))
        Else
            BJ.Status = False
            BJ.OutputError = sOut
            If sOut.Contains("already attached") Then BJ.OutputError = "already attached"

            If sOut.Contains("not unique") Then BJ.OutputError = "not unique"
        End If
        Return BJ
    End Function
    Public Sub StringToFile(sFN As String, sData As String)
        Dim objWriter As New System.IO.StreamWriter(sFN)
        objWriter.Write(sData)
        objWriter.Close()

    End Sub
    Public Function FileToString(sFN As String) As String
        Dim objReader As New System.IO.StreamReader(sFN)
        Dim sOut As String = objReader.ReadToEnd()
        objReader.Close()
        Return sOut
    End Function

    Public Function BoincCreateAccount(sProjectURL As String, sEmail As String, sBoincPassword As String, sUserName As String) As BatchJob
        Dim sOut As String = BoincCommand("--create_account " + sProjectURL + " " + sEmail + " " + sBoincPassword + " " + sUserName)
        Dim BJ As New BatchJob
        If sOut.Contains("account key:") Then
            BJ.Status = True
            Dim vSplit() As String
            vSplit = Split(sOut, "account key:")
            BJ.Value = Trim(vSplit(1))
        Else
            BJ.Status = False
            BJ.OutputError = sOut
            If sOut.Contains("not unique") Then BJ.OutputError = "not unique"
        End If
        Return BJ
    End Function
    Public Function BoincLookupAccount(sProjectURL As String, sEmail As String, sBoincPassword As String) As BatchJob
        Dim sOut As String = BoincCommand("--lookup_account " + sProjectURL + " " + sEmail + " " + sBoincPassword)
        Dim BJ As New BatchJob
        If sOut.Contains("account key:") Then
            BJ.Status = True
            Dim vSplit() As String
            vSplit = Split(sOut, "account key:")
            BJ.Value = Trim(vSplit(1))
        Else
            BJ.Status = False
            BJ.OutputError = sOut
            If sOut.Contains("no database rows") Then BJ.OutputError = "no database rows"
            If sOut.Contains("bad password") Then BJ.OutputError = "bad password"
        End If
        Return BJ
    End Function


    Public Function BoincCommand(sCommand As String) As String

        Try

            Dim sFullCommand As String = Chr(34) + GetBoincAppDir() + "boinccmd.exe" + Chr(34) + " " + sCommand + ">" + Chr(34) + GetBoincAppDir() + "boinccmd_out.txt" + Chr(34) + " 2>&1"
            StringToFile(GetBoincAppDir() + "boinc_command.bat", sFullCommand)
            If File.Exists(GetBoincAppDir() + "boinccmd_out.txt") Then System.IO.File.Delete(GetBoincAppDir() + "boinccmd_out.txt")

            Dim p As Process = New Process()
            Dim pi As ProcessStartInfo = New ProcessStartInfo()
            pi.WorkingDirectory = GetBoincAppDir()
            pi.UseShellExecute = True
            pi.Arguments = ""
            pi.WindowStyle = ProcessWindowStyle.Hidden

            pi.FileName = pi.WorkingDirectory + "\boinc_command.bat"
            p.StartInfo = pi
            p.Start()
            For x = 1 To 7
                System.Threading.Thread.Sleep(500)
                If p.HasExited Then Exit For
            Next

            Dim sOut As String = FileToString(GetBoincAppDir() + "boinccmd_out.txt")
            sOut = Replace(sOut, Chr(10), "")
            sOut = Replace(sOut, Chr(13), "")
            If sOut Is Nothing Then sOut = ""

            Return sOut
        Catch ex As Exception
            Return ""
        End Try


    End Function
    Public Function BoincRetrieveTeamID(sProjectURL) As String
        Dim myWebClient As New MyWebClient()
        Try

            Dim sFullURL As String = sProjectURL + "team_lookup.php?team_name=gridcoin&format=xml"
            Dim sHTTP As String
            Try
                sHTTP = myWebClient.DownloadString(sFullURL)

            Catch ex As Exception

            End Try
            
            Dim sTeamID As String
            sTeamID = ExtractXML(sHTTP, "<id>", "</id>")
            Return sTeamID
        Catch ex As Exception
            Return ""
        End Try

    End Function
    Public Function AttachProject(sURL As String, sEmail As String, sPass As String, sUserName As String) As String
        Dim sResult As String = ""
        Dim BJ As New BatchJob
        BJ = BoincLookupAccount(sURL, sEmail, sPass)
        Dim sKey As String
        sKey = BJ.Value
        If BJ.Status = False Then
            'Account does not exist
            sResult += "Account does not exist; Creating new account." + vbCrLf
            'Add an account to the project
            BJ = BoincCreateAccount(sURL, sEmail, sPass, sUserName)
            If BJ.Status = False Then
                sResult += "Failed to create a new account for project " + sURL + vbCrLf + "Process Failed." + vbCrLf
                Return sResult
            Else
                sResult += "Created account successfully." + vbCrLf
                sKey = BJ.Value
            End If
        End If
        'Attach project to boinc
        BJ = BoincAttachProject(sURL, sKey)
        If BJ.Status = True Then
            sResult += "Attached project " + sURL + " to boinc successfully." + vbCrLf
        Else
            sResult += "Failed to attach project : " + BJ.OutputError + vbCrLf
            'Dont fail here; maybe already attached
        End If
        'Get the Gridcoin team ID 
        Dim sTeam As String
        sTeam = BoincRetrieveTeamID(sURL)
        sResult += "Retrieved Team Gridcoin ID: " + sTeam + vbCrLf

        BJ = BoincSetTeamID(sURL, sKey, sTeam)

        If BJ.Status = True Then
            sResult += "Successfully added user to Team Gridcoin." + vbCrLf
        Else
            sResult += "Failed to add user to Team Gridcoin." + vbCrLf
        End If
        Return sResult
    End Function
    Public Function GetClientStateSize() As Double
        Dim sPath As String = ""
        Try
            sPath = GetBoincClientStatePath()
            Dim fiBoinc As New FileInfo(sPath)
            Return fiBoinc.Length
        Catch ex As Exception
            Log("Path " + sPath + " does not exist.")
            Return 0
        End Try
    End Function
    Public Function GetBoincClientStatePath()
        Dim sDataDir As String = GetBoincDataDir() + "client_state.xml"
        Return sDataDir
    End Function
    Public Function GetPeersSize() As Double
        Dim sPath As String = GetGridPath("") + "peers.dat"
        If Not File.Exists(sPath) Then Return 0
        Dim fiSz As New FileInfo(sPath)
        Return fiSz.Length
    End Function
    Public Function GetBoincDataDir() As String
        Dim sDir1 As String = "c:\programdata\BOINC\"
        If System.IO.Directory.Exists(sDir1) Then Return sDir1
        sDir1 = KeyValue("boincdatadir")
        If System.IO.Directory.Exists(sDir1) Then Return sDir1
        Return ""
    End Function
    Public Function ComputeLocalCPID() As String
        Dim sBPK As String = GetBoincPublicKey()
        Dim sCPID As String = ""
        Dim sLocalEmail As String = KeyValue("email")
        If sLocalEmail = "" Then Return "EMAIL_EMPTY"
        If sBPK = "" Then Return "PUB_KEY_EMPTY"
        sCPID = GetMd5String(LCase(sBPK) + LCase(sLocalEmail))
        Return sCPID
    End Function
    Public Function GetTotalCPIDRAC(sCPID As String, ByRef sError As String) As Double
        Dim sTable As String = ""
        Dim sLocalError As String = ""
        Dim dProjByteLen As Double = 0
        Dim dTotalRac As Double = GetUserRac(sCPID, sLocalError, sTable, dProjByteLen)
        sError += sLocalError
        If dTotalRac = 0 And dProjByteLen > 200 Then
            sError += "No projects on team Gridcoin."
        End If
        If dTotalRac = 0 And sLocalError = "" And dProjByteLen < 250 Then
            sError += "Invalid CPID"
        End If
        Return dTotalRac
    End Function

    Public Function GetUserRac(sCPID As String, ByRef errors As String, ByRef sTable As String, ByRef dProjectByteLength As Double) As Double
        Dim RACURL As String = "http://cpid.gridcoin.us:5000/"
        Dim sURL As String = RACURL + "get_user.php?cpid=" + sCPID
        Dim w As New MyWebClient
        Dim sRAC As String = ""
        Try
            sRAC = w.DownloadString(sURL)

        Catch ex As WebException
            errors += "Unable to Connect to cpid.gridcoin.us. "
            Return -1
        End Try
        If sRAC = "" Then sRAC = "0" : Return 0

        Dim vRAC() As String
        vRAC = Split(sRAC, "<project>")
        dProjectByteLength = Len(sRAC)
        Dim x As Integer = 0
        Dim rac As Double
        errors = ""
        Dim totalrac As Double
        Dim team As String
        Dim team_out As String = ""
        Dim projname As String = ""
        sTable = "<TABLE><TR><TD>Project</TD><TD>RAC</TD><TD>TEAM</TD></TR>"
        For x = 0 To UBound(vRAC)
            rac = ExtractXML(vRAC(x), "<expavg_credit>", "</expavg_credit>")
            team = ExtractXML(vRAC(x), "<team_name>", "</team_name>")
            projname = ExtractXML(vRAC(x), "<name>", "</name>")
            If Trim(LCase(team)) = "gridcoin" Then
                totalrac = totalrac + rac
                team_out = "gridcoin"
                sTable += "<TR><TD>" + projname + "</TD><TD>" + Trim(rac) + "</TD><TD>" + Trim(team_out) + "</TD></TR>"
            End If
        Next
        sTable += "</TABLE>"
        If team_out <> "gridcoin" Then
            errors = errors + "CPID not part of team gridcoin. "
        End If
        If totalrac < 100 Then
            errors = errors + "Total RAC for Team Gridcoin below 100."
        End If
        Return totalrac

    End Function

    Public Function GetBoincPublicKey() As String
        Dim sStatePath As String = GetBoincClientStatePath()
        If File.Exists(sStatePath) = False Then Return "Client State File does not exist: set boincappdir key."

        Dim fiIn As New StreamReader(sStatePath)
        While fiIn.EndOfStream = False
            Dim sTemp As String = fiIn.ReadLine
            Dim sPK As String = ExtractXML(sTemp, "<cross_project_id>")
            If Len(sPK) > 0 Then
                fiIn.Close()
                Return sPK
            End If
        End While
        Return ""
    End Function
    Public Function GetBoincAppDir() As String
        Dim sDir1 As String = "c:\program files\BOINC\"
        If System.IO.Directory.Exists(sDir1) Then Return sDir1
        sDir1 = "c:\program files (x86)\BOINC\"
        If System.IO.Directory.Exists(sDir1) Then Return sDir1
        sDir1 = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles) + "\BOINC\"
        If System.IO.Directory.Exists(sDir1) Then Return sDir1
        sDir1 = KeyValue("boincappdir")
        If System.IO.Directory.Exists(sDir1) Then Return sDir1
        Return ""
    End Function
    Public Function GetLocalLanIP1() As String
        Dim hostName = System.Net.Dns.GetHostName()
        Dim sIP As String = ""
        For Each hostAdr In System.Net.Dns.GetHostEntry(hostName).AddressList()
            sIP = hostAdr.ToString()

            If hostAdr.ToString().StartsWith("192.168.1.") Then

            End If
        Next

        Stop

    End Function
    Public Sub ThreadWireFrame()
        If Not mfrmWireFrame Is Nothing Then
            mfrmWireFrame.EndWireFrame()
        End If
        mfrmWireFrame = New frmGRCWireFrameCanvas
        mfrmWireFrame.Show()
    End Sub
    Public Sub StopWireFrame()
        If mfrmWireFrame Is Nothing Then
            mfrmWireFrame = New frmGRCWireFrameCanvas
        End If
    End Sub
    Public Function Base64File(sFileName As String)
        Dim sFilePath As String = GetGRCAppDir() + "\" + sFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Dim sBase64 As String = System.Convert.ToBase64String(b, 0, b.Length)
        b = System.Text.Encoding.ASCII.GetBytes(sBase64)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Function
    Public Function DecodeBase64(sData As String)
        Dim b() As Byte
        b = System.Convert.FromBase64String(sData)
        Return ByteToString(b)
    End Function

    Public Function UnBase64File(sSourceFileName As String, sTargetFileName As String)
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sSourceFileName)
        Dim value As String = System.Text.ASCIIEncoding.ASCII.GetString(b)
        b = System.Convert.FromBase64String(value)
        System.IO.File.WriteAllBytes(sTargetFileName, b)
    End Function
    Public Function FileToBase64String(sSourceFileName As String) As String
        Dim sFilePath As String = sSourceFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Dim sBase64 As String = System.Convert.ToBase64String(b, 0, b.Length)
        Return sBase64
    End Function
    Public Function WriteBase64StringToFile(sFileName As String, sData As String)
        Dim sFilePath As String = sFileName
        Dim b() As Byte
        b = System.Convert.FromBase64String(sData)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Function
    Public Function DecryptAES512AttachmentToFile(sFileName As String, sData As String, sPass As String)
        Dim sFilePath As String = sFileName
        Dim b() As Byte
        b = System.Convert.FromBase64String(sData)
        b = AES512DecryptData(b, sPass)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Function

    Public Function FileToBytes(sSourceFileName As String) As Byte()
        If sSourceFileName Is Nothing Then Exit Function

        Try

        Dim sFilePath As String = sSourceFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Return b
        Catch ex As Exception
            MsgBox("File may be open in another program, please close it first.", MsgBoxStyle.Critical)

        End Try

    End Function

    Public Function Base64StringToFile(sData As String, sFileName As String) As Boolean
        Dim b() As Byte
        b = System.Convert.FromBase64String(sData)
        System.IO.File.WriteAllBytes(sFileName, b)
        Return True
    End Function

    Public Function GetMd5String(b As Byte()) As String
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrHash() As Byte
            arrHash = objMD5.ComputeHash(b)
            objMD5 = Nothing
            Dim sOut As String = ByteArrayToHexString2(arrHash)
            Return sOut

        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function
    Public Function GetMd5OfFile(sFile As String) As String
         Dim b() As Byte
        b = FileToBytes(sFile)
        Dim bHash As String
        bHash = GetMd5String(b)
        Return bHash
    End Function
    Public Function GetMd5String(ByVal sData As String) As String
        Try
            Dim objMD5 As New System.Security.Cryptography.MD5CryptoServiceProvider
            Dim arrData() As Byte
            Dim arrHash() As Byte
            arrData = System.Text.Encoding.UTF8.GetBytes(sData)
            arrHash = objMD5.ComputeHash(arrData)
            objMD5 = Nothing
            Dim sOut As String = ByteArrayToHexString2(arrHash)
            Return sOut

        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function

    Private Function ByteArrayToHexString2(ByVal arrInput() As Byte) As String
        Dim strOutput As New System.Text.StringBuilder(arrInput.Length)
        For i As Integer = 0 To arrInput.Length - 1
            strOutput.Append(arrInput(i).ToString("X2"))
        Next
        Return strOutput.ToString().ToLower
    End Function



    Public Function ByteToString(b() As Byte)
        Dim sReq As String
        sReq = System.Text.Encoding.UTF8.GetString(b)
        Return sReq
    End Function
    Public Function IsBoincInstalled() As Boolean
        Dim sPath As String = GetBoincAppDir()
        If File.Exists(sPath + "boincmgr.exe") Then Return True Else Return False
    End Function
    Private Sub ExtractZipEntry(ze As ZipEntry, zf As ZipFile, outFolder As String)
        Dim entryFileName As [String] = ze.Name
        Dim buffer As Byte() = New Byte(4095) {}    ' 4K is optimum
        Dim zipStream As Stream = zf.GetInputStream(ze)
        ' Manipulate the output filename here as desired.
        Dim fullZipToPath As [String] = Path.Combine(outFolder, entryFileName)
        fullZipToPath = Replace(fullZipToPath, "/", "\")
        Try

            Using streamWriter As FileStream = File.Create(fullZipToPath)
                StreamUtils.Copy(zipStream, streamWriter, buffer)
            End Using
        Catch ex As Exception

            'Dont blow up here, or it will break the entire process

        End Try

    End Sub
    Public Sub ExtractZipFile(archiveFilenameIn As String, outFolder As String)
        Dim zf As ZipFile = Nothing

        Try
            MkDir(outFolder)

        Catch ex As Exception

        End Try
        Try
            Dim fs As FileStream = File.OpenRead(archiveFilenameIn)
            zf = New ZipFile(fs)
            For Each zipEntry As ZipEntry In zf
                If Not zipEntry.IsFile Then     ' Ignore directories

                    Try
                        MkDir(outFolder & "\" & zipEntry.Name)

                    Catch ex As Exception

                    End Try


                End If

                ExtractZipEntry(zipEntry, zf, outFolder)

            Next
        Catch ex As Exception
            Dim sErr As String = ex.Message

        Finally
            If zf IsNot Nothing Then
                zf.IsStreamOwner = True     ' Makes close also shut the underlying stream
                zf.Close()
            End If
        End Try
    End Sub
    Public Sub LaunchBoinc()
        Dim sPath As String = GetBoincAppDir()

        Try
            Dim p As Process = New Process()
            Dim pi As ProcessStartInfo = New ProcessStartInfo()
            Dim fi As New System.IO.FileInfo(sPath + "boincmgr.exe")
            pi.WorkingDirectory = fi.DirectoryName
            pi.UseShellExecute = True
            pi.FileName = sPath + "\boincmgr.exe"
            pi.WindowStyle = ProcessWindowStyle.Minimized
            pi.CreateNoWindow = False
            p.StartInfo = pi
            p.Start()
        Catch ex As Exception

        End Try

    End Sub
    Public Sub InstallBoinc()
        Dim sBoincDir As String = "c:\program files\Boinc\"
        Try
            Dim di As New DirectoryInfo(sBoincDir)
            di.Delete(True)
        Catch ex As Exception
        End Try
        Try
            ExtractZipFile(GetGRCAppDir() + "\boinc.zip", sBoincDir)
        Catch ex As Exception
        End Try
    End Sub
    Public Function GetURL() As String
        Return IIf(mbTestNet, testURL, prodURL)

    End Function
    Private Sub DownloadFile(ByVal sFile As String, Optional sServerFolder As String = "")
        Dim sLocalPath As String = GetGRCAppDir()
        Dim sLocalFile As String = sFile
        Dim sLocalPathFile As String = sLocalPath + "\" + sLocalFile
        Try
            Kill(sLocalPathFile)
        Catch ex As Exception
            EventLog.WriteEntry("DownloadFile", "Cant find " + sFile + " " + ex.Message)
        End Try
        Dim sURL As String = GetURL() + sServerFolder + sFile
        Dim myWebClient As New MyWebClient()
        myWebClient.DownloadFile(sURL, sLocalPathFile)
        System.Threading.Thread.Sleep(500)
    End Sub

    Public Sub InstallGalaza()
        Dim sDestDir As String = GetGRCAppDir() + "\Galaza\"
     
        Try
            System.IO.Directory.CreateDirectory(sDestDir)
        Catch ex As Exception

        End Try
        Try
            DownloadFile("galaza.zip", "modules/")
        Catch ex As Exception
            MsgBox("Unable to download Galaza " + ex.Message, MsgBoxStyle.Critical)
            Exit Sub
        End Try
        Try
            ExtractZipFile(GetGRCAppDir() + "\galaza.zip", sDestDir)
            UpdateKey("galazaenabled", "true")
            MsgBox("Installed Gridcoin Galaza Successfully! (Next time you restart gridcoin, Galaza will be on the Advanced menu)", MsgBoxStyle.Information)
        Catch ex As Exception
            Log("Error while installing Galaza " + ex.Message)
            MsgBox("Error while installing Galaza " + ex.Message, MsgBoxStyle.Critical)
        End Try
    End Sub

    Public Function RestartWallet1(sParams As String)
        Dim p As Process = New Process()
        Dim pi As ProcessStartInfo = New ProcessStartInfo()
        pi.WorkingDirectory = GetGRCAppDir()
        pi.UseShellExecute = True
        Log("Restarting wallet with params " + sParams)
        pi.Arguments = sParams
        pi.FileName = Trim("GRCRestarter.exe")
        p.StartInfo = pi
        p.Start()
    End Function
    Public Function ConfigPath() As String
        Dim sFolder As String
        sFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch"
        If mbTestNet Then sFolder += "\testnet"
        Dim sPath As String
        sPath = sFolder + "\gridcoinresearch.conf"
        Return sPath
    End Function
    Public Function GetGridPath(ByVal sType As String) As String
        Dim sTemp As String
        sTemp = GetGridFolder() + sType
        If System.IO.Directory.Exists(sTemp) = False Then
            Try
                System.IO.Directory.CreateDirectory(sTemp)
            Catch ex As Exception
                Log("Unable to create Gridcoin Path " + sTemp)
            End Try
        End If
        Return sTemp
    End Function
    Public Function GetGridFolder() As String
        Dim sTemp As String
        'Determine if user has overridden the %appdata% (datadir) folder first:
        Dim sOverridden As String = KeyValue("datadir")
        sTemp = IIf(Len(sOverridden) > 0, sOverridden, Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch\")
        If mbTestNet Then sTemp += "Testnet\"
        Return sTemp
    End Function
    Public Function KeyValue(ByVal sKey As String) As String
        Try
            Dim sPath As String = ConfigPath()
            Dim sr As New StreamReader(sPath)
            Dim sRow As String
            Dim vRow() As String
            Do While sr.EndOfStream = False
                sRow = sr.ReadLine
                vRow = Split(sRow, "=")
                If LCase(vRow(0)) = LCase(sKey) Then
                    sr.Close()
                    Return Trim(vRow(1) & "")
                End If
            Loop
            sr.Close()
            Return ""

        Catch ex As Exception
            Return ""
        End Try
    End Function
    Public Function UpdateKey(ByVal sKey As String, ByVal sValue As String)
        Dim sr As StreamReader
        Dim sw As StreamWriter
        Dim sInPath As String = ""
        Try
            sInPath = ConfigPath()
            Dim sOutPath As String = ConfigPath() + ".bak"

            Dim bFound As Boolean
            sr = New StreamReader(sInPath)
            sw = New StreamWriter(sOutPath, False)

            Dim sRow As String
            Dim vRow() As String
            Do While sr.EndOfStream = False
                sRow = sr.ReadLine
                vRow = Split(sRow, "=")
                If UBound(vRow) > 0 Then
                    If LCase(vRow(0)) = LCase(sKey) Then
                        sw.WriteLine(sKey + "=" + sValue)
                        bFound = True
                    Else
                        sw.WriteLine(sRow)
                    End If
                End If

            Loop
            If bFound = False Then
                sw.WriteLine(sKey + "=" + sValue)
            End If
            sr.Close()
            sw.Close()
            Kill(sInPath)
            FileCopy(sOutPath, sInPath)
            Kill(sOutPath)
        Catch ex As Exception
            Try
                sr.close()
                sw.close()
            Catch ex1 As Exception
                Log("Unable to close " + sInPath + " " + ex1.Message)
            End Try
            Return ""
        End Try
    End Function
    Public Function cBOO(data As Object) As Boolean
        Dim bOut As Boolean
        Try
            Dim sBoo As String = data.ToString
            bOut = CBool(sBoo)
        Catch ex As Exception
            Return False
        End Try
        Return bOut
    End Function

    Public Function DateStamp() As String
        Dim sTimeStamp As String
        sTimeStamp = Format(Now, "MMM d yyyy HH:mm")
        Return sTimeStamp
    End Function
    Public Function DateStamp(dt As Date) As String
        Return Format(dt, "MMM d yyyy HH:mm")

    End Function
    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = GetGridFolder() + "debug2.log"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(DateStamp) + ", " + sData)
            sw.Close()
        Catch ex As Exception

        End Try

    End Sub
    Public Sub PurgeLog()
        Try
            Dim sPath As String
            sPath = GetGridFolder() + "debug2.log"
            Kill(sPath)
        Catch ex As Exception

        End Try
    End Sub


    Public Function GetNewId(sTable As String, mData As Sql) As Long
        Try
            Dim sql As String
            sql = "Select max(id) as maxid from " + sTable
            Dim vID As Long
            vID = Val(mData.QueryFirstRow(sql, "maxid")) + 1
            Return vID
        Catch ex As Exception
            Log("getnewid:" + ex.Message)
        End Try
    End Function


    Public Function RetrieveSiteSecurityInformation(sURL As String) As String
        Dim u As New Uri(sURL)
        Dim sp As ServicePoint = ServicePointManager.FindServicePoint(u)
        Dim groupName As String = Guid.NewGuid().ToString()
        Dim req As HttpWebRequest = TryCast(HttpWebRequest.Create(u), HttpWebRequest)
        req.ConnectionGroupName = groupName
        Try

            Using resp As WebResponse = req.GetResponse()
            End Using
            sp.CloseConnectionGroup(groupName)
            Dim key As Byte() = sp.Certificate.GetPublicKey()
            Dim sOut As String
            sOut = ByteArrayToHexString(key)
            Return sOut
        Catch ex As Exception
            'Usually due to either HTTP, 501, Not Implemented...etc.
            Return ""
        End Try

    End Function

    Public Function ByteArrayToHexString(ByVal ba As Byte()) As String
        Dim hex As StringBuilder
        hex = New StringBuilder(ba.Length * 2)
        For Each b As Byte In ba
            hex.AppendFormat("{0:x2}", b)
        Next
        Return hex.ToString()
    End Function

    Public Function CalcMd5(sMd5 As String) As String

        Try
            Dim md5 As Object
            md5 = System.Security.Cryptography.MD5.Create()
            Dim b() As Byte
            b = StringToByte(sMd5)
            md5 = md5.ComputeHash(b)
            Dim sOut As String
            sOut = ByteArrayToHexString(md5)
            Return sOut
        Catch ex As Exception
            Return "MD5Error"
        End Try
    End Function


    Public Function ExtractFilename(ByVal sStartElement As String, ByVal sEndElement As String, ByVal sData As String, ByVal minOutLength As Integer) As String
        Try
            Dim sDataBackup As String
            sDataBackup = LCase(sData)
            Dim iStart As Integer
            Dim iEnd As Long
            Dim sOut As String
            iStart = InStr(1, sDataBackup, sStartElement) + Len(sStartElement) + 1
            iEnd = InStr(iStart + minOutLength, sDataBackup, sEndElement)
            sOut = Mid(sData, iStart, iEnd - iStart)
            sOut = Replace(sOut, ",", "")
            sOut = Replace(sOut, "br/>", "")
            sOut = Replace(sOut, "</a>", "")
            Dim iPrefix As Long
            iPrefix = InStr(1, sOut, ">")
            Dim sPrefix As String
            sPrefix = Mid(sOut, 1, iPrefix)
            sOut = Replace(sOut, sPrefix, "")
            Dim sExt As String
            sExt = LCase(Mid(sOut, Len(sOut) - 2, 3))
            sOut = LCase(sOut)
            If sExt = "pdf" Or LCase(sOut).Contains("to parent directory") Or sExt = "msi" Or sExt = "pdb" Or sExt = "xml" Or LCase(sOut).Contains("vshost") Or sExt = "txt" Or sOut = "gridcoin" Or sOut = "gridcoin_ro" Or sOut = "older" Or sExt = "cpp" Or sOut = "web.config" Then sOut = ""
            If sOut = "gridcoin.zip" Then sOut = ""
            If sOut = "gridcoinrdtestharness.exe.exe" Or sOut = "gridcoinrdtestharness.exe" Then sOut = ""
            If sOut = "cgminer_base64.zip" Then sOut = ""
            If sOut = "signed" Then sOut = ""
            If LCase(sOut) = "modules" Then sOut = ""

            Return Trim(sOut)
        Catch ex As Exception
            Dim message As String = ex.Message


        End Try
    End Function

    Public Function ParseDate(sDate As String)
        'parses microsofts IIS date to a date, globally
        Dim vDate() As String
        vDate = Split(sDate, " ")
        If UBound(vDate) > 0 Then
            Dim sEle1 As String = vDate(0)
            Dim vEle() As String
            vEle = Split(sEle1, "/")
            If UBound(vEle) > 1 Then
                Dim dt1 As Date
                dt1 = DateSerial(vEle(2), vEle(0), vEle(1))
                Return dt1

            End If
        End If
        Return CDate("1-1-2031")

    End Function
    Public Function GlobalCDate(sDate As String) As DateTime
        Try

            Dim year As Long = Val(Mid(sDate, 7, 4))
            Dim day As Long = Val(Mid(sDate, 4, 2))
            Dim m As Long = Val(Mid(sDate, 1, 2))
            Dim dt As DateTime = DateSerial(year, m, day)
            Return dt
        Catch ex As Exception
            Return CDate(Format(sDate, "mm-dd-yyyy"))
        End Try

    End Function

    Public Function AES512EncryptData(b() As Byte, Pass As String) As Byte()
        Try
            Dim encryptor As ICryptoTransform = CreateAESEncryptor(Pass)
            Dim ms As New System.IO.MemoryStream
            Dim encStream As New CryptoStream(ms, encryptor, System.Security.Cryptography.CryptoStreamMode.Write)
            encStream.Write(b, 0, b.Length)
            encStream.FlushFinalBlock()
            Try
                Return ms.ToArray
            Catch ex As Exception
                Log("Error while encrypting " + ex.Message)

            End Try
        Catch ex As Exception
            Log("Error while encrypting [2]" + ex.Message)

        End Try
    End Function

    Public Function AES512DecryptData(EncryptedBytes() As Byte, sPass As String) As Byte()
        Try
            Dim decryptor As ICryptoTransform = CreateAESDecryptor(sPass)
            Dim ms As New System.IO.MemoryStream
            Dim decStream As New CryptoStream(ms, decryptor, System.Security.Cryptography.CryptoStreamMode.Write)
            decStream.Write(EncryptedBytes, 0, EncryptedBytes.Length)
            decStream.FlushFinalBlock()
            Return ms.ToArray
        Catch ex As Exception
            Log("Error while decryption AES512 " + ex.Message)
        End Try
    End Function


    Public Function AES512EncryptData(ByVal plaintext As String) As String

        Try
            Dim encryptor As ICryptoTransform = CreateAESEncryptor("salt")
            Dim plaintextBytes() As Byte = System.Text.Encoding.Unicode.GetBytes(plaintext)
            Dim ms As New System.IO.MemoryStream
            ' Create the encoder to write to the stream. 
            Dim encStream As New CryptoStream(ms, encryptor, System.Security.Cryptography.CryptoStreamMode.Write)
            ' Use the crypto stream to write the byte array to the stream.
            encStream.Write(plaintextBytes, 0, plaintextBytes.Length)
            encStream.FlushFinalBlock()
            Try
                Return Convert.ToBase64String(ms.ToArray)
            Catch ex As Exception
            End Try
        Catch ex As Exception
            Log("Error while encrypting AES 512 " + ex.Message)
        End Try
    End Function

    Public Function AES512DecryptData(ByVal encryptedtext As String) As String
        Try
            Dim decryptor As ICryptoTransform = CreateAESDecryptor("salt")
            Dim encryptedBytes() As Byte = Convert.FromBase64String(encryptedtext)
            Dim ms As New System.IO.MemoryStream
            Dim decStream As New CryptoStream(ms, decryptor, System.Security.Cryptography.CryptoStreamMode.Write)
            ' Use the crypto stream to write the byte array to the stream.
            decStream.Write(encryptedBytes, 0, encryptedBytes.Length)
            decStream.FlushFinalBlock()
            ' Convert the plaintext stream to a string. 
            Return System.Text.Encoding.Unicode.GetString(ms.ToArray)
        Catch ex As Exception
            Return ex.Message
        End Try
    End Function
    Public Function xGetRPCReply(sType As String) As String
        Dim d As New Row
        d.Database = "RPC"
        d.Table = "RPC"
        d.PrimaryKey = sType
        d = Read(d)
        Return d.DataColumn1
    End Function
    Public Function SetRPCReply(sData As String) As Double
        Try

            msRPCReply = sData
            Log("Received QT RPC Reply: " + sData)
            Return 1
        Catch ex As Exception
            Return 0
        End Try

    End Function

End Module

Public Class GRCWebClient
    Inherits System.Net.WebClient
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = 3000
        Return w
    End Function
End Class

Public Class MyWebClient
    Inherits System.Net.WebClient
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = 7000
        Return w
    End Function
End Class


Namespace GridcoinRichUI
    Public Class DataGridViewRichTextBoxColumn
        Inherits DataGridViewColumn
        Public Sub New()

            MyBase.New(New DataGridViewRichTextBoxCell())
        End Sub
    End Class

    Public Class DataGridViewRichTextBoxCell
        Inherits DataGridViewTextBoxCell

        Public Overrides ReadOnly Property FormattedValueType() As Type
            Get
                Return GetType(String)
            End Get
        End Property

        Protected Overrides Sub Paint(graphics As Graphics, clipBounds As Rectangle, cellBounds As Rectangle, _
                                      rowIndex As Integer, cellState As DataGridViewElementStates, value As Object, _
         formattedValue As Object, errorText As String, cellStyle As DataGridViewCellStyle, _
         advancedBorderStyle As DataGridViewAdvancedBorderStyle, paintParts As DataGridViewPaintParts)
            MyBase.Paint(graphics, clipBounds, cellBounds, rowIndex, cellState, Nothing, _
             Nothing, errorText, cellStyle, advancedBorderStyle, paintParts)
            Dim rtb = New RichTextBox()

            'rtb.BackColor = Color.Black
            'rtb.ForeColor = Color.LimeGreen

            If value.ToString().StartsWith("{\rtf") Then
                rtb.Rtf = value.ToString()
            Else
                rtb.Text = value.ToString()
            End If
            Dim b As System.Drawing.Brush
            If rowIndex = mRowIndex Then
                b = Brushes.Yellow
            Else
                b = Brushes.Yellow
            End If
            If rtb.Text <> String.Empty Then
                ' graphics.DrawString(rtb.Text, DataGridView.DefaultFont, Brushes.LimeGreen, cellBounds.Left, cellBounds.Top)
                graphics.DrawString(rtb.Text, DataGridView.DefaultFont, b, cellBounds.Left, cellBounds.Top)

            End If
        End Sub
    End Class
End Namespace

