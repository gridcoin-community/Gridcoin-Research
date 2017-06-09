Imports ICSharpCode.SharpZipLib.Zip
Imports ICSharpCode.SharpZipLib.Core
Imports System.IO
Imports ICSharpCode.SharpZipLib.GZip


Public Class clsBoincProjectDownload

    'Public msSeedProjects As String = "Bitcoin Utopia;http://www.bitcoinutopia.net/bitcoinutopia/,Citizen Science Grid;"                                       & "http://csgrid.org/csg/stats/,enigma@home;http://www.enigmaathome.net/,denis@home;http://denis.usj.es/denisathome/,universe@home;http://universeathome.pl/universe/,wuprop@home;http://wuprop.boinc-af.org/,World Community Grid;http://www.worldcommunitygrid.org/boinc/,yafu;http://yafu.myfirewall.org/yafu/,Gridcoin Finance;"                                       & "http://finance.gridcoin.us/finance/,seti@home;http://setiathome.berkeley.edu/,Rosetta@Home;"                                       & "http://boinc.bakerlab.org/,"                                       & "Einstein@Home;http://einstein.phys.uwm.edu/,"                                       & "MilkyWay@home;http://milkyway.cs.rpi.edu/milkyway/,PrimeGrid;"                                       & "http://www.primegrid.com/,"                                       & "theSkyNet POGS;http://pogs.theskynet.org/pogs/,Asteroids@home;"                                       & "http://asteroidsathome.net/boinc/;http://www.enigmaathome.net/,"                                       & "SZTAKI Desktop Grid;http://szdg.lpds.sztaki.hu/szdg/,"                                       & "Climate Prediction;http://climateapps2.oerc.ox.ac.uk/cpdnboinc/,"                                       & "POEM@HOME;http://boinc.fzk.de/poem/,"                                       & "Malaria Control;http://malariacontrol.net/,LHC@Home Classic;http://lhcathomeclassic.cern.ch/sixtrack/,yoyo@home;http://www.rechenkraft.net/yoyo/,"                                       & "Cosmology@Home;http://www.cosmologyathome.org/,SAT@home;http://sat.isa.ru/pdsat/,"                                       & "CAS@HOME;http://casathome.ihep.ac.cn/,NFS@Home;http://escatter11.fullerton.edu/nfs/,"                                       & "NumberFields@home;http://numberfields.asu.edu/NumberFields/,Leiden Classical;http://boinc.gorlaeus.net/,"                                       & "GPUGRID;http://www.gpugrid.net/,"                                       & "DistributedDataMining;http://www.distributeddatamining.org/DistributedDataMining/,"                                       & "EDGeS@Home;http://home.edges-grid.eu/home/,"                                       & "Albert@Home;http://albert.phys.uwm.edu/,"                                       & "The Lattice Project;http://boinc.umiacs.umd.edu/,"                                       & "Collatz Conjecture;http://boinc.thesonntags.com/collatz/,"                                       & "MindModeling@Home;http://mindmodeling.org/,"                                       & "vLHCathome;http://lhcathome2.cern.ch/vLHCathome/,"                                       & "FiND@Home;http://findah.ucd.ie/,ATLAS@Home;http://atlasathome.cern.ch/,Moowrap;http://moowrap.net/,BURP;http://burp.renderfarming.net/

    Public Function DownloadGZipFiles() As Boolean

        Dim lAgeOfMaster = GetUnixFileAge(GetGridFolder() + "NeuralNetwork\db.dat")
        Log("UFA Timestamp: " + Trim(lAgeOfMaster))

        If lAgeOfMaster > SYNC_THRESHOLD Then
            ReconnectToNeuralNetwork()
            Dim bFail As Boolean = mGRCData.GetNeuralNetworkQuorumData(mbTestNet)
            lAgeOfMaster = GetUnixFileAge(GetGridFolder() + "NeuralNetwork\db.dat")
        End If
        If lAgeOfMaster < SYNC_THRESHOLD Then Return True
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
                ' If LCase(sProjectURL) Like "*http://www.distributeddatamining.org/distributeddatamining/*" Then
                'sProjectURL = "http://www.distributeddatamining.org/DistributedDataMining/"
                'End If
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
                    Dim sTeamPathUnzipped As String = Replace(sTeamPath, ".gz", ".xml")
                    Dim sGzipPathUnzipped As String = Replace(sPath, ".gz", ".xml")
                    'If older than 7 days, download the team files again:
                    GuiDoEvents()
                    If GetWindowsFileAge(sTeamPathUnzipped) > TEAM_SYNC_THRESHOLD Then
                        Dim w As New MyWebClient
                        For iRetry As Integer = 1 To 5
                            Try
                                'Find out what our team ID is
                                msNeuralDetail = "Gathering Team " + sProject
                                Log("Syncing Team " + sProject + " " + sTeamGzipURL)
                                w.DownloadFile(sTeamGzipURL, sTeamPath)
                                GuiDoEvents()
                                'un-gzip the file
                                ExtractGZipInnerArchive(sTeamPath, GetGridFolder() + "NeuralNetwork\")
                                Exit For
                            Catch ex As Exception
                                Log("Error while downloading master team gz file: " + ex.Message + ", Retrying.")
                            End Try
                        Next
                    End If

                    'Sync the main RAC gz file            

                    If GetWindowsFileAge(sGzipPathUnzipped) > PROJECT_SYNC_THRESHOLD Then
                        Dim w As New MyWebClient
                        For iRetry As Integer = 1 To 5
                            Try
                                'Find out what our team ID is
                                msNeuralDetail = "Gather Project " + sProject
                                Log("Syncing Project " + sProject + " " + sGzipURL)
                                w.DownloadFile(sGzipURL, sPath)
                                GuiDoEvents()
                                'un-gzip the file
                                ExtractGZipInnerArchive(sPath, GetGridFolder() + "NeuralNetwork\")
                                'Delete the Project master.dat file
                                If File.Exists(GetGridFolder() + "NeuralNetwork\" + sProject + ".master.dat") Then
                                    File.Delete(GetGridFolder() + "NeuralNetwork\" + sProject + ".master.dat")
                                End If
                                Exit For
                            Catch ex As Exception
                                Dim sMsg As String = ex.Message
                                Log("Error while downloading master project rac gz file : " + ex.Message + ", Retrying.")
                            End Try
                        Next
                    End If
                    'Scan for the Gridcoin team inside this project:
                    Dim lTeamID As Long = GetTeamID(sTeamPathUnzipped)

                    'Create the project master file
                    If File.Exists(GetGridFolder() + "NeuralNetwork\" + sProject + ".master.dat") = False Then
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
                Using oStream As New System.IO.FileStream(fi.FullName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)
                    Dim objReader As New System.IO.StreamReader(oStream)
                    While objReader.EndOfStream = False
                        Dim sTemp As String = objReader.ReadLine
                        sTemp = Replace(sTemp, "<name>", "<username>")
                        sTemp = Replace(sTemp, "</name>", "</username>")
                        sTemp = Replace(sTemp, "<user>", "<project><name>" + sProjectLocal + "</name><team_name>gridcoin</team_name>")
                        sTemp = Replace(sTemp, "</user>", "</project>")
                        'Dont bother writing timestamps older than 32 days since we base mag off of RAC
                        Dim lRowAgeInMins = GetRowAgeInMins(sTemp)
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
