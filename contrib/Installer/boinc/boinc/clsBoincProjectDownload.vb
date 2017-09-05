Imports ICSharpCode.SharpZipLib.Zip
Imports ICSharpCode.SharpZipLib.Core
Imports System.IO
Imports ICSharpCode.SharpZipLib.GZip

Imports System.Net

Public Class clsBoincProjectDownload
    Public Shared Function GetHttpResponseHeaders(url As String) As Dictionary(Of String, String)
        Dim headers As New Dictionary(Of String, String)()
        Try
            Dim webRequest As WebRequest = HttpWebRequest.Create(url)
            webRequest.Method = "HEAD"
            webRequest.Timeout = 10000
            Using webResponse As WebResponse = webRequest.GetResponse()
                For Each header As String In webResponse.Headers
                    headers.Add(header, webResponse.Headers(header))
                Next
            End Using
            Return headers
        Catch ex As Exception
            If ex.Message.Contains("404") Then
                headers("ETag") = "404"
                headers("Content-Length") = 0
                headers("Last-Modified") = Trim(Now)
                Return headers
            Else
                headers("ETag") = ex.Message
                headers("Content-Length") = 0
                headers("Last-Modified") = Trim(Now)
                Return headers
            End If
        End Try
    End Function
    Public Function RetrieveCacheProjectFilename(sGzipUrl As String) As String
        Dim sOut As String = Replace(sGzipUrl, "/", "[fslash]")
        sOut = Replace(sOut, ":", "[colon]")
        Dim sDayOfYear = Trim(DateTime.UtcNow.DayOfYear)
        sOut = Replace(sOut, ".gz", sDayOfYear + "[!]gz")
        sOut = Replace(sOut, ".", "[period]")
        sOut = Replace(sOut, "[!]", ".")
        Return sOut
    End Function
    Public Function GetFileAge(sPath As String) As Double
        If File.Exists(sPath) = False Then Return 1000000
        Dim fi As New FileInfo(sPath)
        If fi.Length = 0 Then Return 1000000
        Dim iMins As Long = DateDiff(DateInterval.Minute, fi.LastWriteTime, Now)
        Return iMins
    End Function

    Private Function AnalyzeProjectHeader2(ByVal sGzipURL As String, ByRef sEtag As String, ByRef sEtagFilePath As String, ByVal sProjectName As String) As Integer
        Dim dStatus As Double = 0
        sEtag = GetMd5String2(sGzipURL)
        sEtagFilePath = ConstructTargetFileName(sEtag)

        Dim sGridcoinBaseUrl As String = "https://download.gridcoin.us/download/harvest/"
        Dim lAgeInMins As Long = GetFileAge(sEtagFilePath)
        If lAgeInMins < SYNC_THRESHOLD Then
            Return 1
        Else
            Dim sCacheFileName As String = RetrieveCacheProjectFilename(sGzipURL)
            Dim bStatus As Boolean = ResilientDownload(sGridcoinBaseUrl + sCacheFileName, sEtagFilePath, sGzipURL)
            If bStatus Then
                Return 2
            Else
                Return 3
            End If
        End If
    End Function

    Public Function DownloadFile(iAttemptNo As Integer, sSourceURL As String, sOutputPath As String) As Boolean
        Try
            Dim w As New MyWebClient2
            Log(" Downloading Attempt #" + Trim(iAttemptNo) + " for URL " + sSourceURL)
            w.DownloadFile(sSourceURL, sOutputPath)
            Return True
        Catch ex As Exception
            Return False
        End Try

    End Function
    Public Function ResilientDownload(sSourceUrl As String, sEtagFilePath As String, sBackupURL As String) As Boolean
        'Doing this just in case we ddos our own website and keep receiving 404 or 500 errors:
        For x As Integer = 1 To 5
            Dim dictHeads As Dictionary(Of String, String) = GetHttpResponseHeaders(sSourceUrl)
            Dim dLength1 As Double = dictHeads("Content-Length")
            Dim bDownloadStatus As Boolean = DownloadFile(x, sSourceUrl, sEtagFilePath)
            If bDownloadStatus And GetFileSize(sEtagFilePath) = dLength1 And dLength1 > 0 Then Return True
        Next
        'As a last resort, pull from the Project Site
        Dim dictHeads2 As Dictionary(Of String, String) = GetHttpResponseHeaders(sBackupURL)
        Dim dLength2 As Double = dictHeads2("Content-Length")
        Log(" Downloading BackupURL Attempt # " + Trim(1) + sBackupURL)
        DownloadFile(1, sBackupURL, sEtagFilePath)
        If GetFileSize(sEtagFilePath) = dLength2 And dLength2 > 0 Then Return True Else Return False
    End Function
    Public Function DownloadGZipFiles() As Boolean
        'Perform Housecleaning
        Dim sFolder As String = GetGridFolder() + "NeuralNetwork\"
        DeleteOlderThan(sFolder, 3, ".gz") 'Clean old gzip
        DeleteOlderThan(sFolder, 3, ".dat")
        DeleteOlderThan(sFolder, 3, ".xml")
        Dim rWhiteListedProjects As New Row
        rWhiteListedProjects.Database = "Whitelist"
        rWhiteListedProjects.Table = "Whitelist"
        Dim lstWhitelist As List(Of Row) = GetList(rWhiteListedProjects, "*")
        Dim sProjectURL As String = ""
        Dim sProject As String = ""
        Dim sNNFolder1 As String = GetGridFolder() + "NeuralNetwork\"
        If Directory.Exists(sNNFolder1) = False Then
            Directory.CreateDirectory(sNNFolder1)
        End If
        For Each rProject As Row In lstWhitelist
            sProject = LCase(Trim(rProject.PrimaryKey))
            sProjectURL = Trim(rProject.DataColumn1)

            If Right(sProjectURL, 1) = "@" Then
                sProjectURL = Mid(sProjectURL, 1, Len(sProjectURL) - 1)
                sProject = Replace(sProject, "_", " ")
                sProjectURL = Replace(sProjectURL, "_", " ")
                Try

                    'Gather GZ Files:
                    Dim vURLGzip As String()
                    vURLGzip = Split(sProjectURL, "/")
                    Dim sGzipURL As String = vURLGzip(0) + "//" + vURLGzip(1) + vURLGzip(2) + "/stats/user.gz"
                    'One Off Rules
                    sGzipURL = vURLGzip(0) + "//" + vURLGzip(1) + vURLGzip(2) + "/" + vURLGzip(3) + "/stats/user.gz"
                    If sGzipURL Like "*einstein*" Then sGzipURL = Replace(sGzipURL, "user.gz", "user_id.gz")
                    If sGzipURL Like "*burp*" Then sGzipURL = Replace(sGzipURL, "user.gz", "user_id.gz")
                    If sGzipURL Like "*gorlaeus*" Then sGzipURL = Replace(sGzipURL, "user.gz", "user.xml.gz")
                    If sGzipURL Like "*worldcommunitygrid*" Then sGzipURL = Replace(sGzipURL, "user.gz", "user.xml.gz")
                    'Download the Team file
                    Dim sPath As String = GetGridFolder() + "NeuralNetwork\" + sProject + ".gz"
                    Dim sTeamPath As String = Replace(sPath, ".gz", "team.gz")
                    Dim sTeamGzipURL As String = Replace(sGzipURL, "user.gz", "team.gz")
                    sTeamGzipURL = Replace(sTeamGzipURL, "user_id.gz", "team_id.gz")
                    sTeamGzipURL = Replace(sTeamGzipURL, "user.xml.gz", "team.xml.gz")
                    GuiDoEvents()
                    'If Etag has changed, download the file:
                    Dim sEtag As String = ""
                    Dim sTeamEtagFilePath As String = ""
                    Dim iStatus As Integer = AnalyzeProjectHeader2(sTeamGzipURL, sEtag, sTeamEtagFilePath, sProject)
                    Dim sProjectMasterFileName As String = GetGridFolder() + "NeuralNetwork\" + sProject + ".master.dat"
                    'store etag by project also

                    If iStatus <> 1 Or Not File.Exists(sProjectMasterFileName) Then
                        Try
                            'Find out what our team ID is
                            msNeuralDetail = "Gathering Team " + sProject
                            Log("Syncing Team " + sProject + " " + sTeamGzipURL)
                            GuiDoEvents()
                            'un-gzip the file
                            ExtractGZipInnerArchive(sTeamEtagFilePath, GetGridFolder() + "NeuralNetwork\")
                        Catch ex As Exception
                            Log("Error while downloading master team gz file: " + ex.Message + ", Retrying.")
                        End Try
                    End If


                    'Sync the main RAC gz file            
                    Dim sRacEtagFilePath As String = ""
                    iStatus = AnalyzeProjectHeader2(sGzipURL, sEtag, sRacEtagFilePath, sProject)
                    If iStatus <> 1 Or Not File.Exists(sProjectMasterFileName) Then
                        Try
                            'Find out what our team ID is
                            msNeuralDetail = "Gather Project " + sProject
                            Log("Syncing Project " + sProject + " " + sGzipURL)
                            ExtractGZipInnerArchive(sRacEtagFilePath, GetGridFolder() + "NeuralNetwork\")
                            'Delete the Project master.dat file
                            If File.Exists(sProjectMasterFileName) Then
                                File.Delete(sProjectMasterFileName)
                            End If
                        Catch ex As Exception
                            Dim sMsg As String = ex.Message
                            Log("Error while downloading master project rac gz file : " + ex.Message + ", Retrying.")
                        End Try
                    End If
                    'Scan for the Gridcoin team inside this project:
                    Dim sTeamPathUnzipped As String = Replace(sTeamEtagFilePath, ".gz", ".xml")
                    Dim sGzipPathUnzipped As String = Replace(sRacEtagFilePath, ".gz", ".xml")
                    Log("Determining Team ID for Project " + sProject + " " + sGzipURL)
                    Dim lTeamID As Long = GetTeamID(sTeamPathUnzipped)
                    'Create the project master file
                    If Not File.Exists(sProjectMasterFileName) Then
                        Log("Emitting Project File for Project " + sProject + " " + sGzipURL)
                        EmitProjectFile(sGzipPathUnzipped, GetGridFolder() + "NeuralNetwork\", sProject, lTeamID)
                    End If
                    Debug.Print(sProject)
                Catch ex As Exception
                    Log("Error while syncing " + sProject + ": " + ex.Message)
                End Try
            End If

        Next
        'Verify all the files exist
        Dim iTotalProjectsSynced As Integer = 0
        'Create master database
        Dim sNNFolder As String = GetGridFolder() + "NeuralNetwork\"
        Dim sDB As String = sNNFolder + "db.dat"
        Dim oSW As New StreamWriter(sDB)
        Dim di As New DirectoryInfo(sNNFolder)
        Dim fiArr As FileInfo() = di.GetFiles()
        Dim fi As FileInfo
        Dim sProjectLocal As String
        msNeuralDetail = "Combining Project Data"
        Log("Combining Project Data")

        For Each fi In fiArr
            If fi.Name Like "*.master.dat*" Then
                sProjectLocal = Replace(fi.Name, ".master.dat", "")
                iTotalProjectsSynced += 1
                'Dim sTimestamp As String = GetDataValue("etag", "timestamps", sProjectLocal).DataColumn1
                'Log the md5 of the master project file so we can determine regional differences
                Dim iRows As Long = 0
                Using oStream As New System.IO.FileStream(fi.FullName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)
                    Dim objReader As New System.IO.StreamReader(oStream)
                    While objReader.EndOfStream = False
                        Dim sTemp As String = objReader.ReadLine
                        sTemp = Replace(sTemp, "<name>", "<username>")
                        sTemp = Replace(sTemp, "</name>", "</username>")
                        sTemp = Replace(sTemp, "<user>", "<project><name>" + sProjectLocal + "</name><team_name>gridcoin</team_name>")
                        sTemp = Replace(sTemp, "</user>", "</project>")
                        'Dont bother writing timestamps older than 32 days since we base mag off of RAC (Filter RAC based on project header in UTC - RAC Updated UTC - BullShark)
                        iRows += 1
                        oSW.WriteLine(sTemp)
                    End While
                    objReader.Close()
                End Using
                Dim sHash As String = GetMd5OfFile(fi.FullName)
                Dim sOut As String = "***  COMBINING PROJECT FILE " + fi.Name + ", MD5 HASH: " + sHash + ", ROWS: " + Trim(iRows) + "**"
                Log(sOut)
            End If
        Next fi
        oSW.Close()
        msNeuralDetail = ""
        If iTotalProjectsSynced < lstWhitelist.Count Then
            Log("Total Projects Synced: " + Trim(iTotalProjectsSynced) + ", Project Count " + Trim(lstWhitelist.Count) + ": FAILURE")
            If File.Exists(GetGridFolder() + "NeuralNetwork\db.dat") Then
                'Conundrum here.  Although we would like to delete the database, what if one project site is down for the day
                'for all nodes?  If we do this, no one will be able to sync.  For robustness, let the network sync with a missing project.
                ' File.Delete(GetGridFolder() + "NeuralNetwork\db.dat")
            End If
            ' Log("Database deleted.")
        End If
        Return True

    End Function
    Private Sub AppendUser(swProj As StreamWriter, dTeamId As Double, vChunk() As String)
        For y As Integer = 0 To UBound(vChunk)
            Dim dInternalTeamID As Double = Val(ExtractXML(vChunk(y), "<teamid>", "</teamid>"))
            If dInternalTeamID = dTeamId Then
                swProj.Write("<user>" + vChunk(y) + vbCrLf)
            End If
        Next y
    End Sub
    Public Function EmitProjectFile(sPath As String, sNNPath As String, sProject As String, dTeamID As Double) As Boolean
        Dim srProj As New StreamReader(sPath)
        Dim sChunk As String = ""
        Dim sOutFile As String = sNNPath + sProject + ".master.dat"
        Dim swProj As New StreamWriter(sOutFile, False)
        While srProj.EndOfStream = False
            Dim sTemp As String = srProj.ReadLine()
            sChunk += sTemp
            If Len(sChunk) > 25000 And sTemp.Contains("</user>") Then
                Dim vChunk() As String
                vChunk = Split(sChunk, "<user>")
                AppendUser(swProj, dTeamID, vChunk)
                sChunk = ""
            End If
        End While
        Dim vChunk1() As String
        vChunk1 = Split(sChunk, "<user>")
        AppendUser(swProj, dTeamID, vChunk1)
        srProj.Close()
        swProj.Close()
        Return 0
    End Function
    Private Function HexStr(iByte As Integer) As String
        Dim sOut As String = "00" + Hex(iByte)
        Return sOut.Substring(Len(sOut) - 2, 2)
    End Function
    Private Function GetGZChecksum(sURL As String) As String
        Dim dictHeads As Dictionary(Of String, String) = GetHttpResponseHeaders(sURL)
        'Extract the checksum using an HTTP range request
        Dim request As WebRequest = WebRequest.Create(sURL)
        Dim hWRQ As HttpWebRequest = CType(request, HttpWebRequest)
        Dim lStart As Long = Val(dictHeads("Content-Length")) - 8 '2byte magic is in header, 4 byte CRC-32 Checksum is at offset 0 in header, then 4 byte uncompressed data size
        hWRQ.AddRange(lStart, lStart + 8)
        Dim response As WebResponse = request.GetResponse
        Dim dataStream As Stream = response.GetResponseStream()
        Dim sPath As String = "c:\path.gz"
        '6a d3 6d cc checksum
        Dim fs As New FileStream(sPath, FileMode.Create)
        dataStream.CopyTo(fs)
        fs.Close()
        Dim mBytes As Byte() = File.ReadAllBytes(sPath)
        Dim ckLenBytes(3) As Byte
        Array.Copy(mBytes, 4, ckLenBytes, 0, 4)
        Dim sCRC As String = HexStr(mBytes(3)) + HexStr(mBytes(2)) + HexStr(mBytes(1)) + HexStr(mBytes(0))
        Return sCRC
    End Function
    Public Function SlimifyBoincFile(sSourceUserFile As String, sDestUserFile As String, dtSyncTime As DateTime) As Boolean
        Dim srProj As New StreamReader(sSourceUserFile)
        Dim swProj As New StreamWriter(sDestUserFile, False)
        Dim sUser As String = ""
        While srProj.EndOfStream = False
            Dim sTemp As String = srProj.ReadLine()
            sUser += sTemp
            If (sTemp.Contains("</user>") Or srProj.EndOfStream) Then
                Dim sCpid As String = ExtractXML(sUser, "<cpid>")
                Dim sRac As String = ExtractXML(sUser, "<expavg_credit>")
                Dim sTime As String = ExtractXML(sUser, "<expavg_time>")
                'Dim lAge As Double = GetRowAgeInMins(sUser, dtSyncTime)
                If Val(sRac) > 10 Then
                    Dim sOut = sCpid + "," + sRac
                    swProj.WriteLine(sOut)
                End If
                sUser = ""
            End If
        End While
        srProj.Close()
        swProj.Close()
        Return True
    End Function
    'These functions are used to decentralize the Neural Network and therefore in the future it will not be necessary to have a centralized cache for boinc project files on gridcoin.us, but the dev team is still testing these new features currently; They are approximately 50% complete.
    Public Function GetBoincProjectHash(sBaseURL As String) As String
        Dim sTeamUrl = sBaseURL + "team.gz"
        Dim sGzFN As String = GetGridFolder() + "NeuralNetwork\team_temp.gz"
        Dim bStatus As Boolean = ResilientDownload(sTeamUrl, sGzFN, "")
        ExtractGZipInnerArchive(sGzFN, GetGridFolder() + "NeuralNetwork\")
        Dim sTPUZ As String = GetGridFolder() + "NeuralNetwork\" + "team_temp.xml"
        Dim lTeamID As Long = GetTeamID(sTPUZ)
        Dim sUserUrl As String = sBaseURL + "user.gz"
        Dim sUsFn As String = GetGridFolder() + "NeuralNetwork\user_temp.gz"
        bStatus = ResilientDownload(sUserUrl, sUsFn, "")
        ExtractGZipInnerArchive(sUsFn, GetGridFolder() + "NeuralNetwork\")
        Dim sUZ As String = GetGridFolder() + "NeuralNetwork\" + "user_temp.xml"
        'Slmify the user file, then gzip it, then get hash
        Dim sOutFile As String = GetGridFolder() + "NeuralNetwork\" + "slim1.xml"
        Dim dictHeads As Dictionary(Of String, String) = GetHttpResponseHeaders(sUserUrl)
        Dim sTimestamp As String = dictHeads("Last-Modified")
        Dim dtUTC As DateTime = GetUtcDateTime(CDate(sTimestamp))
        SlimifyBoincFile(sUZ, sOutFile, dtUTC)
        'ReGzip this, then return the concatenated GZ hash + MD5 (uncompressed) + GZ Hash
        Dim sNewGzFile As String = GetGridFolder() + "NeuralNetwork\" + "slim2.gz"
        CreateGZ(sNewGzFile, sOutFile)
        Dim sComplexHash As String = "MD5 Component " + " " + "GzComponentHash"
        Return sComplexHash
    End Function
    Public Function GetTeamID(sPath As String) As Double
        Dim swTeam As New StreamReader(sPath)
        Dim sChunk As String = ""
        While swTeam.EndOfStream = False
            Dim sTemp As String = swTeam.ReadLine()
            sChunk += sTemp
            If Len(sChunk) > 25000 And sTemp.Contains("</team>") Then
                Dim vChunk() As String
                vChunk = Split(sChunk, "<team>")
                If LCase(sChunk.Contains("gridcoin")) Then
                    For y As Integer = 0 To UBound(vChunk)
                        Dim sTeamName As String = ExtractXML(vChunk(y), "<name>", "</name>")
                        Dim sID As String = ExtractXML(vChunk(y), "<id>", "</id>")
                        If LCase(sTeamName) = "gridcoin" Then
                            swTeam.Close()
                            Return Val(sID)
                        End If
                    Next
                End If
                sChunk = ""
            End If
        End While
        swTeam.Close()
        Dim vChunk2() As String
        vChunk2 = Split(sChunk, "<team>")
        If LCase(sChunk.Contains("gridcoin")) Then
            For y As Integer = 0 To UBound(vChunk2)
                Dim sTeamName As String = ExtractXML(vChunk2(y), "<name>", "</name>")
                Dim sID As String = ExtractXML(vChunk2(y), "<id>", "</id>")
                If LCase(sTeamName) = "gridcoin" Then
                    swTeam.Close()
                    Return Val(sID)
                End If
            Next
        End If
        Return 0
    End Function
    Public Sub ExtractGZipInnerArchive(gzipFileName As String, targetDir As String)
        ' Use a 4K buffer. Any larger is a complete and total waste of time and memory :)
        Dim dataBuffer As Byte() = New Byte(4095) {}
        Using fs As System.IO.Stream = New FileStream(gzipFileName, FileMode.Open, FileAccess.Read)
            Using gzipStream As New GZipInputStream(fs)
                Dim fnOut As String = Path.Combine(targetDir, Path.GetFileNameWithoutExtension(gzipFileName))
                Using fsOut As FileStream = File.Create(fnOut + ".xml")
                    StreamUtils.Copy(gzipStream, fsOut, dataBuffer)
                End Using
            End Using
        End Using
    End Sub
    Private Sub CreateGZ(gzFilename As String, sourceFile As String)
        Dim srcFile As System.IO.FileStream = File.OpenRead(sourceFile)
        Dim zipFile As GZipOutputStream = New GZipOutputStream(File.Open(gzFilename, FileMode.Create))
        Dim fileData As Byte() = New Byte(srcFile.Length) {}
        srcFile.Read(fileData, 0, srcFile.Length)
        zipFile.Write(fileData, 0, fileData.Length)
        srcFile.Close()
        zipFile.Close()
    End Sub

    Public Sub ExtractZipFile(archiveFilenameIn As String, outFolder As String, bCreateSkeletonDirs As Boolean, bKillOriginal As Boolean)
        Dim zf As ZipFile = Nothing
        Try
            Dim fs As FileStream = File.OpenRead(archiveFilenameIn)
            zf = New ZipFile(fs)
            For Each zipEntry As ZipEntry In zf
                If Not zipEntry.IsFile Then     ' Ignore directories
                    Continue For
                End If
                Dim entryFileName As [String] = zipEntry.Name
                Dim buffer As Byte() = New Byte(4095) {}    ' 4K is optimum
                Dim zipStream As Stream = zf.GetInputStream(zipEntry)
                ' Manipulate the output filename here as desired.
                Dim fullZipToPath As [String] = Path.Combine(outFolder, entryFileName)
                Using streamWriter As FileStream = File.Create(fullZipToPath)
                    StreamUtils.Copy(zipStream, streamWriter, buffer)
                End Using
            Next
        Catch ex As Exception
            Dim sErr As String = ex.Message
        Finally
            If zf IsNot Nothing Then
                zf.IsStreamOwner = True     ' Makes close also shut the underlying stream
                zf.Close()
            End If
        End Try

        If bKillOriginal Then
            Try
                Kill(archiveFilenameIn)
            Catch ex As Exception

            End Try
        End If

    End Sub
End Class
