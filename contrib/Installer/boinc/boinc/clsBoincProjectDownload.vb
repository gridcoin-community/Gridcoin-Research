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

    Private Function AnalyzeProjectHeader(ByVal sGzipURL As String, ByRef sEtag As String, ByRef sEtagFilePath As String, ByVal sProjectName As String) As Integer
        'Output
        '1 = We already have the official etag version downloaded
        '2 = We downloaded a new version
        '3 = Download failed
        Dim dictHeads As Dictionary(Of String, String) = GetHttpResponseHeaders(sGzipURL)
        sEtag = dictHeads("ETag")
        Dim sTimestamp As String
        sTimestamp = dictHeads("Last-Modified")

        sEtag = Replace(sEtag, Chr(34), "")
        sEtag = Replace(sEtag, "W", "")
        Dim dStatus As Double = 0
        Dim sGridcoinURL As String = "https://download.gridcoin.us/download/harvest/" + sEtag + ".gz"
        Dim dictHeadsGRC As Dictionary(Of String, String) = GetHttpResponseHeaders(sGridcoinURL)
        Dim dLength1 As Double = dictHeads("Content-Length")
        Dim dLength2 As Double = dictHeadsGRC("Content-Length")
        ' If Etag matches GRC's cache, and the filesize is the same use the GRCCache version to prevent DDosing the boinc servers (Gridcoin Foundation copies the files from the project sites to the Gridcoin.US web site once every 4 hours to help avoid stressing the project servers from the massive hit by our network)
        Dim bCacheSiteHasSameFile As Boolean = (dLength2 > 0)
        Dim sEtagOnFile As String = GetDataValue("etag", "tbetags", sGzipURL).DataColumn1
        sEtagFilePath = ConstructTargetFileName(sEtag)

        If sEtagOnFile = sEtag Then
            'We already have this file
            Return 1
        Else
            'We do not have this file
            'Only update Stored Value after we retrieve the file
            Dim sSourceURL As String = IIf(bCacheSiteHasSameFile, sGridcoinURL, sGzipURL)
            Dim bStatus As Boolean = ResilientDownload(sSourceURL, sEtagFilePath, sGzipURL)
            'If all bytes downloaded then store the etag in the local table
            If bStatus Then
                StoreValue("etag", "tbetags", sGzipURL, sEtag)
                StoreValue("etag", "timestamps", sProjectName, sTimestamp)


                'Save the Project Site GZ creation time
                Return 2
            Else
                Return 3
            End If
        End If

    End Function
    Public Function ResilientDownload(sSourceUrl As String, sEtagFilePath As String, sBackupURL As String) As Boolean
        'Doing this just in case we ddos our own website and keep receiving 404 or 500 errors:
        For x As Integer = 1 To 5
            Dim dictHeads As Dictionary(Of String, String) = GetHttpResponseHeaders(sSourceUrl)
            Dim dLength1 As Double = dictHeads("Content-Length")
            Dim w As New MyWebClient2
            Log(" Downloading Attempt # " + Trim(x) + sSourceUrl)
            w.DownloadFile(sSourceUrl, sEtagFilePath)
            If GetFileSize(sEtagFilePath) = dLength1 And dLength1 > 0 Then Return True
        Next
        'As a last resort, pull from the Project Site
        Dim dictHeads2 As Dictionary(Of String, String) = GetHttpResponseHeaders(sBackupURL)
        Dim dLength2 As Double = dictHeads2("Content-Length")
        Dim w2 As New MyWebClient2
        Log(" Downloading BackupURL Attempt # " + Trim(1) + sBackupURL)
        w2.DownloadFile(sBackupURL, sEtagFilePath)
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
                    Dim iStatus As Integer = AnalyzeProjectHeader(sTeamGzipURL, sEtag, sTeamEtagFilePath, sProject)
                    Dim sProjectMasterFileName As String = GetGridFolder() + "NeuralNetwork\" + sProject + ".master.dat"

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
                    iStatus = AnalyzeProjectHeader(sGzipURL, sEtag, sRacEtagFilePath, sProject)
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

                    Dim lTeamID As Long = GetTeamID(sTeamPathUnzipped)
                    'Create the project master file
                    If Not File.Exists(sProjectMasterFileName) Then
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
        For Each fi In fiArr
            If fi.Name Like "*.master.dat*" Then
                sProjectLocal = Replace(fi.Name, ".master.dat", "")
                iTotalProjectsSynced += 1
                Dim sTimestamp As String = GetDataValue("etag", "timestamps", sProjectLocal).DataColumn1
                Dim dtUTC As DateTime = GetUtcDateTime(CDate(sTimestamp))
                Using oStream As New System.IO.FileStream(fi.FullName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)
                    Dim objReader As New System.IO.StreamReader(oStream)
                    While objReader.EndOfStream = False
                        Dim sTemp As String = objReader.ReadLine
                        sTemp = Replace(sTemp, "<name>", "<username>")
                        sTemp = Replace(sTemp, "</name>", "</username>")
                        sTemp = Replace(sTemp, "<user>", "<project><name>" + sProjectLocal + "</name><team_name>gridcoin</team_name>")
                        sTemp = Replace(sTemp, "</user>", "</project>")
                        'Dont bother writing timestamps older than 32 days since we base mag off of RAC (Filter RAC based on project header in UTC - RAC Updated UTC - BullShark)
                        Dim lRowAgeInMins = GetRowAgeInMins(sTemp, dtUTC)
                        If lRowAgeInMins < (60 * 24 * 32) Then
                            oSW.WriteLine(sTemp)
                        End If
                    End While
                    objReader.Close()
                End Using
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
