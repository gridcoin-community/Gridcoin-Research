Imports System.Net
Imports System.IO
Imports ICSharpCode.SharpZipLib.Zip
Imports ICSharpCode.SharpZipLib.Core

Public Class GridcoinUpgrader
    Public oGRCData As GRCSec.GridcoinData

    Public oGRCSec As GRCSec.GRCSec

    Public iErrorCount As Long = 0


    Private bDebug As Boolean = False
    Private prodURL As String = "http://download.gridcoin.us/download/downloadstake/"
    Private testURL As String = "http://download.gridcoin.us/download/downloadstaketestnet/"
    Private b404 As Integer = 0

    Private Sub RemoveBlocksDir(d As System.IO.DirectoryInfo)
        Try
            d.Delete(True)
        Catch ex As Exception
        End Try
    End Sub

    Public Function GetURL() As String
        If b404 = 0 Then
            'Unchecked - Check state of Anti-DDOS host:
            Try
                Dim sFile As String = "openpopstake.dll"
                Dim sData As String = ""
                Dim myWebClient As New MyWebClient()
                sData = myWebClient.DownloadString(prodURL + sFile)
                Log("Downloaded openpop " + Trim(Len(sData)))
                If Len(sData) > 30000 Then
                    b404 = 1
                Else
                    b404 = -1
                End If

            Catch ex As Exception
                Log("Download error " + Trim(ex.Message))
                b404 = -1
            End Try
        End If

        If b404 = 1 Then
            If bTestNet Then
                Return testURL
            Else
                Return prodURL
            End If
        Else
            If bTestNet Then
                Return testURL
            Else
                Return "http://finance.gridcoin.us/downloads/"
            End If
        End If

    End Function
    Private Function GetFilePercent(sName As String, Sz As Double) As Double
        Dim sPath As String
        sPath = GetGRCAppDir() + "\" + sName
        Dim p As Double = 50
        Try
            Dim fi As New System.IO.FileInfo(sPath)
            Dim dSz As Double = fi.Length
            p = dSz / Sz
        Catch ex As Exception
        End Try
        Return p
    End Function
    Private Sub RefreshScreen()
        Me.Show()
        Me.BringToFront()
        Me.Update() : Me.Refresh() : Application.DoEvents()
    End Sub
    Private Sub GridcoinUpgrader_Disposed(sender As Object, e As System.EventArgs) Handles Me.Disposed
        Environment.Exit(0)
        End
    End Sub
    Public Sub BusyWaitLoop(sWaitForProcessName As String, lSecs As Long, bKill As Boolean)
        For x As Integer = 1 To lSecs
            If Not IsRunning("gridcoinresearch*") Then Exit For
            System.Threading.Thread.Sleep(1000)
        Next
        If bKill Then
            KillProcess("gridcoinresearch*")
        End If
    End Sub
    Private Sub Form1_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load
        lblPercent.Text = ""

        Try
            oGRCData = New GRCSec.GridcoinData
            oGRCSec = New GRCSec.GRCSec

        Catch ex As Exception
            MsgBox("Initialization failed.  Upgrade Failed.  Please delete *.config and *.manifest files in your application directory.  This must be done by the user.  Then restart with 'grcrestarter upgrade'.  ", MsgBoxStyle.Critical)
            End

        End Try
       

        '''''''''''''''''''''RESTORE SNAPSHOT
        If Environment.GetCommandLineArgs.Length > 0 Then
            If Environment.CommandLine.Contains("testnet") Then
                bTestNet = True
            End If
            If Environment.CommandLine.Contains("restoresnapshot") Then
                Try
                    KillProcess("gridcoinresearch*")
                    RefreshScreen()
                    txtStatus.Text = "Restoring block chain from snapshot..."
                    RefreshScreen()
                    System.Threading.Thread.Sleep(7000)
                    Dim bErr As Boolean
                    For x = 1 To 5
                        Try
                            Call RestoreFromSnapshot()
                        Catch ex As Exception
                            System.Threading.Thread.Sleep(3000)
                            bErr = True
                        End Try
                        If Not bErr Then Exit For
                    Next x
                    StartGridcoin()
                    Environment.Exit(0)
                    End
                Catch ex As Exception
                End Try
            End If
        End If
        If Environment.CommandLine.Contains("downloadblocks") Then
            Try

                KillProcess("gridcoinresearch*")
                txtStatus.Text = "Downloading Blocks File..."
                RefreshScreen()
                ProgressBar1.Maximum = 100
                TimerUE.Enabled = True
                Dim sz As Long
                sz = GetWebFileSize("snapshot.zip", "signed")
                If sz = 0 Then sz = 23993000
                'This is an asynchronous process:
                DownloadBlocks("snapshot.zip", "signed/")
                While Not mbFinished
                    System.Threading.Thread.Sleep(800)
                    Dim p As Double = 0
                    p = Math.Round(GetFilePercent("snapshot.zip", sz) * 100, 2)
                    lblPercent.Text = Trim(p) + "%"
                    RefreshScreen()
                End While
                TimerUE.Enabled = False
                txtStatus.Text = "Unzipping Blocks File..."
                RefreshScreen()
                RestoreFromZipSnapshot()
            Catch ex As Exception

            End Try
        End If
        '''''''''''''''''''RESTORE POINT
        If Environment.GetCommandLineArgs.Length > 0 Then
            If Environment.CommandLine.Contains("restorepoint") Then
                Call Snapshot()
                Environment.Exit(0)
                End
            End If
        End If
        ''''''''''''''''''''''REBOOT
        If Environment.GetCommandLineArgs.Length > 0 Then
            If Environment.CommandLine.Contains("reboot") Then
                Try
                    Log("restarting")
                    Me.Show()
                    txtStatus.Text = "Waiting for Gridcoin Wallet to exit..."
                    Me.Update() : Me.Refresh()
                    BusyWaitLoop("gridcoinresearch*", 10, True)
                    StartGridcoin()
                    Environment.Exit(0)
                    End
                Catch ex As Exception
                    Log("Cant restart " + ex.Message)
                End Try
            End If
        End If
        '''''''''''''''''''''''WATCHDOG
        If Environment.GetCommandLineArgs.Length > 0 Then
            If Environment.CommandLine.Contains("watchdog") Then
                Try
                    Dim frmWatchDog As Form = New WatchDog
                    Me.Hide()
                    Me.Width = 1
                    Me.Height = 1
                    Me.WindowState = FormWindowState.Minimized
                    Me.ShowInTaskbar = False
                    Me.Refresh()
                    Me.Update()
                    Me.Visible = False
                    Me.Update()
                    frmWatchDog.Show()
                    Exit Sub
                Catch ex As Exception
                    Log("unable to instantiate watchdog " + ex.Message)
                    End
                End Try
            End If
        End If
        ''''''''''''''''''''''REINDEX
        If Environment.GetCommandLineArgs.Length > 0 Then
            If Environment.CommandLine.Contains("reindex") Then
                Try
                    BusyWaitLoop("gridcoinresearch*", 5, True)
                    RemoveGrcDataDir()
                    'R Halford: Add a step to restore 120,000 blocks for a better user experience:
                    Try
                        Call RestoreFromSnapshot()
                    Catch ex As Exception
                    End Try
                    StartGridcoin()
                    Environment.Exit(0)
                    End
                Catch ex As Exception
                End Try
            End If
        End If
        ''''''''''''''''''''''''Upgrade the Upgrader Program
        If Environment.CommandLine.Contains("restoregrcrestarter") Then
            Try
                System.Threading.Thread.Sleep(3000)
                Dim sSource As String = GetGRCAppDir() + "\" + "grcrestarter_copy.exe"
                Dim sTarget As String = GetGRCAppDir() + "\" + "grcrestarter.exe"
                FileCopy(sSource, sTarget)
                Environment.Exit(0)
            Catch ex As Exception

            End Try
        End If
        '''''''''''''''''''''''''''''''''UPGRADE'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''
        If Environment.CommandLine.Contains("testnetupgrade") Then
            bTestNet = True
        End If
        If Environment.CommandLine.Contains("upgrade") Then
            Try
                DeleteManifest()

                ProgressBar1.Maximum = 1000
                ProgressBar1.Value = 1
                RefreshScreen()
                txtStatus.Text = "Waiting for Gridcoin Wallet to exit..."
                RefreshScreen()
                BusyWaitLoop("gridcoinresearch*", 9, True)
                For x = 1 To 9
                    If IsRunning("gridcoinresearch*") Then
                        KillProcess("gridcoinresearch*")
                        System.Threading.Thread.Sleep(1000)
                    Else
                        Exit For
                    End If
                    System.Threading.Thread.Sleep(50)
                Next
                'Test permissions in target folder first
                Try
                    Dim sLocalPath As String = GetGRCAppDir() + "\permissiontest.dat"
                    Dim sw As New System.IO.StreamWriter(sLocalPath, True)
                    sw.WriteLine(Trim(Now) + ", " + "Permission Test")
                    sw.Close()
                Catch ex As Exception
                    MsgBox("Upgrade failed.  Unable to write files to Application Directory.  Please take ownership of the application directory " + GetGRCAppDir() + ".", MsgBoxStyle.Critical)
                    Environment.Exit(0)
                End Try
                'Upgrade the wallet before continuing
                Dim sMsg As String = DynamicUpgradeWithManifest()
                If sMsg = "FAIL" Then
                    End
                End If
                If Len(sMsg) > 0 Then
                    MsgBox("Upgrade Failed. " + sMsg, MsgBoxStyle.Critical)
                    End
                End If
                txtStatus.Text = "Upgrade Successful."
                For x = ProgressBar1.Value To ProgressBar1.Maximum
                    ProgressBar1.Value = x
                    RefreshScreen()
                Next
                StartGridcoin()
                UpgradeGrcRestarter("restoregrcrestarter")
                Environment.Exit(0)
                End
            Catch ex As Exception
                MsgBox("Upgrade Failed. " + ex.Message, MsgBoxStyle.Critical)
            End Try
        End If

        If Environment.CommandLine.Contains("unbase64file") Then
            UnBase64File("cgminer.zip")
            Environment.Exit(0)
        End If

        If Environment.CommandLine.Contains("base64file") Then
            Base64File("cgminer.zip")
            Environment.Exit(0)
        End If
        ''''''''''''''''''''''''''''''''''''''Else....
        If Environment.CommandLine.Contains("120") Then
            System.Threading.Thread.Sleep(120000)
        End If

        StartGridcoin()
        Environment.Exit(0)
        End
    End Sub

    Public Sub RemoveGrcDataDir()
        Try
            For x = 1 To 10
                Dim sDataDir As String = GRCDataDir()
                Dim sChainstate = sDataDir + "chainstate"
                Dim sChain = sDataDir + "txleveldb"
                Dim sDatabase = sDataDir + "database"
                Dim dBlock As New System.IO.DirectoryInfo(sChainstate)
                RemoveBlocksDir(dBlock)
                Dim dChain As New System.IO.DirectoryInfo(sChain)
                RemoveBlocksDir(dChain)
                Dim dDatabase As New System.IO.DirectoryInfo(sDatabase)
                RemoveBlocksDir(dDatabase)
                Dim y As Integer
                'Delete the blocks File:
                Try
                    Dim f_i As New System.IO.FileInfo(sDataDir + "\blk0001.dat")
                    f_i.Delete()
                Catch ex As Exception
                    Log("Unable to delete blocks file " + sDataDir + "\blk0001.dat")
                End Try
                For y = 1 To 5
                    Dim sFile As String = "blk000" + Trim(y) + ".dat"
                    Try
                        Dim fi As New System.IO.FileInfo(sDataDir + "\" + sFile)
                        fi.Delete()

                    Catch ex As Exception
                        Log("Unable to delete " + ex.Message)
                    End Try
                Next
                If dChain.Exists = False And dBlock.Exists = False And dDatabase.Exists = False Then Exit For
                Threading.Thread.Sleep(1000)
            Next
        Catch ex As Exception
            Log("Error while removing grc data dir " + ex.Message)
        End Try

    End Sub

    Public Sub Snapshot()
        'Not currently supported
        Exit Sub
    End Sub
    Public Sub RestoreFromSnapshot()
        Dim sDataDir As String = GRCDataDir()
        Dim sBlocks = sDataDir + "blocks"
        Dim sChain = sDataDir + "chainstate"
        Dim sDatabase = sDataDir + "database"
        Dim sSnapshotBlocks = sDataDir + "snapshot\blocks"
        Dim sSnapshotChain = sDataDir + "snapshot\chainstate"
        Dim sSnapshotDatabase = sDataDir + "snapshot\database"
        RemoveGrcDataDir()
        Try
            System.IO.Directory.CreateDirectory(sSnapshotBlocks)
        Catch ex As Exception
        End Try
        Try
            System.IO.Directory.CreateDirectory(sSnapshotChain)
        Catch ex As Exception
        End Try
        Try
            System.IO.Directory.CreateDirectory(sSnapshotDatabase)
        Catch ex As Exception
        End Try
    End Sub
    Private Sub CreateSkeletonDirsForGRC(sDir As String)
        Try
            Try
                Dim di2 As New DirectoryInfo(sDir & "\chainstate")
                di2.Create()
            Catch ex As Exception
            End Try
            Try
                Dim di2 As New DirectoryInfo(sDir & "\database")
                di2.Create()
                Dim di3 As New DirectoryInfo(sDir & "\txleveldb")
                di3.Create()
            Catch ex As Exception
            End Try
        Catch ex As Exception
        End Try
    End Sub
    Public Sub ExtractZipFile(archiveFilenameIn As String, outFolder As String, bCreateSkeletonDirs As Boolean)

        If bCreateSkeletonDirs Then CreateSkeletonDirsForGRC(outFolder)

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
    End Sub

    Public Sub RestoreFromZipSnapshot()
        Dim sDataDir As String = GRCDataDir()
        Dim sTxLevelDb = sDataDir + "txleveldb"
        Dim sChain = sDataDir + "chainstate"
        Dim sDatabase = sDataDir + "database"
        Dim sAppDir As String = GetGRCAppDir()
        Try
            RemoveGrcDataDir()
        Catch ex As Exception
        End Try
        Try
            System.IO.Directory.CreateDirectory(sTxLevelDb)
        Catch ex As Exception
        End Try
        Try
            System.IO.Directory.CreateDirectory(sChain)
        Catch ex As Exception
        End Try
        Try
            System.IO.Directory.CreateDirectory(sDatabase)
        Catch ex As Exception
        End Try
        Try
            ExtractZipFile(sAppDir + "\snapshot.zip", sDataDir, True)
        Catch ex As Exception
        End Try
    End Sub

    Private Sub DirectorySnapshot( _
        ByVal sourceDirName As String, _
        ByVal destDirName As String, _
        ByVal copySubDirs As Boolean)
        ' Get the subdirectories for the specified directory. 
        Dim dir As DirectoryInfo = New DirectoryInfo(sourceDirName)
        Dim dirs As DirectoryInfo() = dir.GetDirectories()
        'Remove the destination directory
        Dim dDestination As New System.IO.DirectoryInfo(destDirName)
        Try
            RemoveBlocksDir(dDestination)
        Catch ex As Exception
        End Try
        ' If the destination directory doesn't exist, create it. 
        If Not Directory.Exists(destDirName) Then
            Directory.CreateDirectory(destDirName)
        End If
        ' Get the files in the directory and copy them to the new location. 
        Dim files As FileInfo() = dir.GetFiles()
        For Each file In files
            Dim temppath As String = Path.Combine(destDirName, file.Name)
            Try
                file.CopyTo(temppath, False)
            Catch ex As Exception
            End Try
        Next file
        ' If copying subdirectories, copy them and their contents to new location. 
        If copySubDirs Then
            For Each subdir In dirs
                Dim temppath As String = Path.Combine(destDirName, subdir.Name)
                DirectorySnapshot(subdir.FullName, temppath, copySubDirs)
            Next subdir
        End If
    End Sub
    Public Function GRCDataDir() As String
        Dim sFolder As String
        sFolder = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\gridcoinresearch\"
        If bTestNet Then sFolder = sFolder + "testnet\"
        Return sFolder
    End Function
    Private msFile As String = ""
    Private msServerFolder As String = ""
    Private mbFinished As Boolean = False

    Private Sub DownloadBlocks(sFile As String, sServerFolder As String)
        msFile = sFile
        msServerFolder = sServerFolder
        mbFinished = False
        Try
            Kill(sServerFolder & "\" & sFile)
        Catch ex As Exception
        End Try
        Dim t As New System.Threading.Thread(AddressOf DownloadFileAsync)
        t.Priority = Threading.ThreadPriority.AboveNormal
        t.Start()
    End Sub

    Private Sub DownloadFileAsync()
        Dim sFile As String = msFile
        Dim sServerFolder As String = msServerFolder
        Dim sLocalPath As String = GetGRCAppDir()
        Dim sLocalFile As String = sFile
        If LCase(sLocalFile) = "grcrestarter.exe" Then sLocalFile = "grcrestarter_copy.exe"
        Dim sLocalPathFile As String = sLocalPath + "\" + sLocalFile
        Try
            Kill(sLocalPathFile)
        Catch ex As Exception
            EventLog.WriteEntry("DownloadFile", "Cant find " + sFile + " " + ex.Message)
        End Try
        Dim sURL As String = GetURL() + sServerFolder + sFile
        Dim myWebClient As New MyWebClient()
        Try
            myWebClient.DownloadFile(sURL, sLocalPathFile)
        Catch ex As Exception
        End Try
        mbFinished = True
    End Sub
    Private Sub xDownloadFile(ByVal sFile As String, Optional sServerFolder As String = "")
        Dim sLocalPath As String = GetGRCAppDir()
        Dim sLocalFile As String = sFile
        Dim sLocalPathFile As String = sLocalPath + "\" + sLocalFile
        txtStatus.Text = "Upgrading file " + sFile + "..."
        Try
            Kill(sLocalPathFile)
        Catch ex As Exception
            EventLog.WriteEntry("DownloadFile", "Cant find " + sFile + " " + ex.Message)
        End Try
        Dim sURL As String = GetURL() + sServerFolder + sFile
        Dim myWebClient As New MyWebClient()
        myWebClient.DownloadFile(sURL, sLocalPathFile)
        Me.Refresh()
        System.Threading.Thread.Sleep(500)
    End Sub
    Public Function GetGRCAppDir() As String
        Try
            If bDebug Then
                Dim fiAlt As New System.IO.FileInfo("c:\program files (x86)\gridcoinresearch\")
                Return fiAlt.DirectoryName
            End If
            Dim fi As New System.IO.FileInfo(Application.ExecutablePath)
            Return fi.DirectoryName
        Catch ex As Exception
            Return ""
        End Try
    End Function
    Public Sub Base64File(sFileName As String)
        Dim sFilePath As String = GetGRCAppDir() + "\" + sFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Dim sBase64 As String = System.Convert.ToBase64String(b, 0, b.Length)
        b = System.Text.Encoding.ASCII.GetBytes(sBase64)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Sub
    Public Sub UnBase64File(sFileName As String)
        Dim sFilePath As String = GetGRCAppDir() + "\" + sFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Dim value As String = System.Text.ASCIIEncoding.ASCII.GetString(b)
        b = System.Convert.FromBase64String(value)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Sub
    
    Public Function GetWebFileSize(sName As String, sDir As String) As Long
        Try
            Dim sMsg As String
            Dim sURL As String = GetURL() + sDir + "/"
            Dim wc As New WebClient
            Dim sFiles As String
            sFiles = wc.DownloadString(sURL)
            Dim vFiles() As String = Split(sFiles, "<br>")
            If UBound(vFiles) < 3 Then
                Return 0
            End If
            sMsg = ""
            For iRow As Integer = 0 To UBound(vFiles)
                Dim sRow As String = vFiles(iRow)
                Dim sFile As String = ExtractFilename("<a", "</a>", sRow, 5)
                If Len(sFile) > 1 Then
                    If LCase(sFile) = LCase(sName) Then
                        Dim sSz As String
                        sSz = Trim(Mid(sRow, 22, 12)) 'size portion
                        Return Val(sSz)
                    End If
                End If
            Next iRow
        Catch ex As Exception
            Return 0
        End Try
        Return 0
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
                'Handle IIS PST Date format:
                Dim sTr As String
                Dim sTime1 As String
                Dim sTime2 As String
                sTime1 = vDate(UBound(vDate) - 1)
                sTime2 = vDate(UBound(vDate) - 0)
                sTr = Trim(DateSerial(vEle(2), vEle(0), vEle(1))) + " " + Trim(sTime1) + " " + Trim(sTime2)
                If IsDate(sTr) Then Return CDate(sTr) Else Return CDate("1-1-2031")
            End If
        End If
        Return CDate("1-1-2031")
    End Function
   
    Public Function DynamicUpgradeWithManifest() As String
        Dim sMsg As String = ""
        Dim iRow As Long = 1
        Dim dr As SqlClient.SqlDataReader
        Dim bCleanIteration As Boolean = True
        Dim iIterationErrorCount As Long = 0
        For iTry As Long = 1 To 5
            bCleanIteration = True
            Try
                dr = oGRCData.mGetUpgradeFiles
                If dr.HasRows = False Then
                    MsgBox("Upgrade failed; No files stored in Gridcoin Research mirror.", MsgBoxStyle.Critical)
                    Return "FAIL"
                End If
                Dim sLocalPath As String = GetGRCAppDir() + "\"
                ProgressBar1.Maximum = 50
                Do While dr.Read
                    iRow += 1
                    If iRow > ProgressBar1.Maximum - 1 Then ProgressBar1.Maximum += 1
                    Dim sFile As String = dr("filename")
                    If Not ExcludedFiles(sFile) Then
                        'Get local hash
                        Dim sLocalHash As String = GetMd5OfFile(sLocalPath + sFile)
                        Dim sRemoteHash As String = dr("Hash")
                        Dim bNeedsUpgraded As Boolean = sLocalHash <> sRemoteHash
                        Dim iSignatureOK As Integer = oGRCSec.IsHashAuthentic(sRemoteHash, dr("SecurityHash"), dr("PublicKey"))
                        UpdateUI(iRow, sFile, dr("comments"), True, False)

                        If bNeedsUpgraded And iSignatureOK = 0 Then
                            UpdateUI(iRow, sFile, dr("comments"), True, True)
                            If LCase(sFile) = "grcrestarter.exe" Then sFile = "grcrestarter_copy.exe"
                            If LCase(sFile) = "grcsec.dll" Then
                                If File.Exists(sLocalPath + sFile) Then
                                    If File.Exists(sLocalPath + "grcsec_old.dll") Then Kill(sLocalPath + "grcsec_old.dll")
                                    Rename(sLocalPath + sFile, "grcsec_old.dll")
                                End If
                            End If
                  
                            mRetrieveUpgrade(oGRCData, dr("id").ToString(), sLocalPath, sFile, MerkleRoot)
                            Application.DoEvents()
                            Threading.Thread.Sleep(2000) 'Wait until file is closed (this is supposedly synchronous, but some users must have a delayed write)
                            Application.DoEvents()

                            'Verify the downloaded blob matches record hash
                            Dim sVerifiedHash As String = GetMd5OfFile(sLocalPath + sFile)
                            Dim sNarr As String = "Remote hash " + sRemoteHash + ", Downloaded hash " + sVerifiedHash + ", Download path : " + sLocalPath + sFile + "."

                            If sVerifiedHash <> sRemoteHash Then
                                Threading.Thread.Sleep(4000)
                                Dim sVerified2 As String = GetMd5OfFile(sLocalPath + sFile)
                                If Not Command() Like "*nodelete*" Then
                                    If File.Exists(sLocalPath + sFile) Then Kill(sLocalPath + sFile)
                                End If

                                Dim sLongNarr As String = "Downloaded File " + sFile + " security hash does not match signed remote hash.  " + sNarr + ".  After four seconds, hash of file is : " + sVerified2 + ", Discarding File.  Upgrade failed.  Please report this to Gridcoin: contact@gridcoin.us"
                                Log(sLongNarr)
                                MsgBox(sLongNarr, MsgBoxStyle.Critical)
                                Return "-9"
                            End If
                        End If
                        If iSignatureOK <> 0 Then
                            Dim sNarr As String = "Remote hash " + sRemoteHash + ", Security Hash " + dr("SecurityHash") + ",Download path : " + sLocalPath + sFile + "."
                            Dim sLongNarr As String = "File " + sFile + " signature invalid!  " + sNarr + ", Please report this to Gridcoin immediately: contact@gridcoin.us"
                            Log(sLongNarr)
                            MsgBox(sLongNarr, MsgBoxStyle.Critical)
                            Return "-8"
                        End If
                        Threading.Thread.Sleep(30)
                    End If
                Loop
            Catch ex As Exception
                iErrorCount += 1 : iIterationErrorCount += 1
                Stop

                If bCleanIteration Then MsgBox("Error -11: " + ex.Message + "; Retrying " + Trim(5 - iIterationErrorCount) + " more times before failing.")
                bCleanIteration = False
            End Try
            If iErrorCount = 0 Then Exit For
        Next iTry
        If iErrorCount = 0 Then
            UpdateUI(50, "Success.", "Success", False, False)
            Threading.Thread.Sleep(1000)
        Else
            Return "FAIL"
        End If
        Return ""
    End Function
    Public Sub DeleteManifest()
        'Manifest files cause an error when downloading from the cluster... try to delete first
        Dim sPath As String = GetGRCAppDir() + "\"
        DelFile(sPath + "grcrestarter.exe.config")

        DelFile(sPath + "grcrestarter.config")
        DelFile(sPath + "grcrestarter.vshost.exe.config")
        DelFile(sPath + "grcrestarter.exe.manifest")
        DelFile(sPath + "grcrestarter.vshost.exe.manifest")

        DelFile(sPath + "grcrestarter.manifest")
        Try
            Rename(sPath + "grcrestarter.config", sPath + "grcrestarter.confold")

        Catch ex As Exception

        End Try
        Try
            Rename(sPath + "grcrestarter.exe.config", sPath + "grcrestarter.execonfold")

        Catch ex As Exception

        End Try
    End Sub
    Public Sub DelFile(sFN As String)
        Try
            Kill(sFN)
        Catch ex As Exception

        End Try
    End Sub
    Public Function ExcludedFiles(sFN As String) As Boolean
        Dim sExt As String
        sExt = LCase(Mid(sFN, Len(sFN) - 2, 3))
        sFN = LCase(sFN)
        Dim bExcluded As Boolean = False
        If sExt = "pdf" Or sExt = "msi" Or sExt = "pdb" Or sExt = "xml" Or sFN.Contains("vshost") _
             Or sExt = "cpp" Or sFN = "web.config" Then bExcluded = True
        If sFN = "gridcoin.zip" Then bExcluded = True 'The legacy gridcoin.zip is a separate download
        If sFN = "gridcoinrdtestharness.exe.exe" Or sFN = "gridcoinrdtestharness.exe" Then bExcluded = True
        If sFN = "cgminer_base64.zip" Then bExcluded = True
        If sFN = "setup.exe" Or sFN = "gridcoinresearch.msi" Then bExcluded = True
        If InStr(1, sFN, "libstdc") > 0 Then bExcluded = True
        If sFN = "bitcoin.png" Or sFN = "bitcoin.ico" Then bExcluded = True
        If sFN = "qrc_bitcoin.cpp" Then bExcluded = True

        Return bExcluded

    End Function
    Public Function UpdateUI(iRow As Long, sFN As String, sComments As String, bShowComments As Boolean, bUpgrading As Boolean)

        ProgressBar1.Value = iRow : ProgressBar1.Update() : ProgressBar1.Refresh() : Me.Refresh() : Application.DoEvents() : RefreshScreen()
        If Len(sFN) > 1 Then
            Dim sPrefix As String = IIf(bUpgrading, "Upgrading", "Verifying")
            If LCase(sFN) = "success" Then sPrefix = ""
            txtStatus.Text = sPrefix + " " + sFN + "..."
            txtStatus.Width = Me.Width
            txtStatus.Refresh() : txtStatus.Update() : Application.DoEvents()
        End If
        If bShowComments Then rtbNotes.Text = sComments
        Threading.Thread.Sleep(200)
        Application.DoEvents()

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

            If sOut = "setup.exe" Or LCase(sOut) = "gridcoinresearch.msi" Then sOut = ""
            If InStr(1, sOut, "libstdc") > 0 Then sOut = ""
            If sOut = "testnet" Then sOut = ""
            If sOut = "bitcoin.png" Or sOut = "bitcoin.ico" Then sOut = ""
            If sOut = "qrc_bitcoin.cpp" Then sOut = ""
            If sExt = "xml" Then sOut = ""
            Return Trim(sOut)
        Catch ex As Exception
            Dim message As String = ex.Message
            Return ""
        End Try
    End Function
    Public Sub UpgradeGrcRestarter(sParams As String)
        Try

        Dim p As Process = New Process()
        Dim pi As ProcessStartInfo = New ProcessStartInfo()
        pi.WorkingDirectory = GetGRCAppDir()
        pi.UseShellExecute = True
        pi.Arguments = sParams
        pi.FileName = Trim("GRCRestarter_copy.exe")
        p.StartInfo = pi
            p.Start()
        Catch ex As Exception

        End Try

    End Sub
    Private Sub TimerUE_Tick(sender As System.Object, e As System.EventArgs) Handles TimerUE.Tick
        ProgressBar1.Maximum = ProgressBar1.Maximum + 1
        Dim v As Long = ProgressBar1.Value
        If v > ProgressBar1.Maximum Then v = ProgressBar1.Maximum
        ProgressBar1.Value = v
        RefreshScreen()
    End Sub
End Class

Public Class MyWebClient
    Inherits System.Net.WebClient
    Private timeout As Long = 125000
    Protected Overrides Function GetWebRequest(ByVal uri As Uri) As System.Net.WebRequest
        Dim w As System.Net.WebRequest = MyBase.GetWebRequest(uri)
        w.Timeout = timeout
        Return (w)
    End Function
End Class
