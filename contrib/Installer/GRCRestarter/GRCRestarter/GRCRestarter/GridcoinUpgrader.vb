Imports System.Net
Imports System.IO
Imports ICSharpCode.SharpZipLib.Zip
Imports ICSharpCode.SharpZipLib.Core



Public Class GridcoinUpgrader

    Private bDebug As Boolean = False


    Private prodURL As String = "http://download.gridcoin.us/download/downloadstake/"
    Private testURL As String = "http://download.gridcoin.us/download/downloadstaketestnet/"
   

    Private Sub RemoveBlocksDir(d As System.IO.DirectoryInfo)
        Try
            d.Delete(True)
        Catch ex As Exception
        End Try
    End Sub
   
    Private Function GetURL() As String
        If bTestNet Then
            Return testURL
        Else
            Return prodURL
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
    Private Function RefreshScreen()
        Me.Show()
        Me.BringToFront()



        Me.Update() : Me.Refresh() : Application.DoEvents()
    End Function

    Private Sub GridcoinUpgrader_Disposed(sender As Object, e As System.EventArgs) Handles Me.Disposed
        Environment.Exit(0)
        End

    End Sub
    Private Sub Form1_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load

        lblPercent.Text = ""

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

                DownloadBlocks("snapshot.zip", "signed/")
                While Not mbFinished
                    System.Threading.Thread.Sleep(800)

                    Dim p As Double = 0
                    p = Math.Round(GetFilePercent("snapshot.zip", 135993000) * 100, 2)


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
        '''''''''''''''''''RESTORE POINT (No need to Kill Miners)
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
                    KillProcess("gridcoinresearch*")
                    System.Threading.Thread.Sleep(1000)
                    StartGridcoin()
                    Environment.Exit(0)
                    End
                Catch ex As Exception
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

        ''''''''''''''''''''''REINDEX KILL MINERS
        If Environment.GetCommandLineArgs.Length > 0 Then
            If Environment.CommandLine.Contains("reindex") Then
                Try

                    KillProcess("gridcoinresearch*")
                    System.Threading.Thread.Sleep(1000)
                    RemoveGrcDataDir()
                    '6-6-2014 R Halford: Add a step to restore 120,000 blocks for a better user experience:

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

                ProgressBar1.Maximum = 1000
                ProgressBar1.Value = 1
                RefreshScreen()
                txtStatus.Text = "Waiting for Gridcoin Wallet to exit..."
                RefreshScreen()
                System.Threading.Thread.Sleep(8000)
                KillProcess("gridcoinresearch*")
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
                If Len(sMsg) > 0 Then
                    MsgBox("Upgrade Failed. " + sMsg, MsgBoxStyle.Critical)
                    Environment.Exit(0)
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
        '7-1-2014

        For x = 1 To 10
            Dim sDataDir As String = GRCDataDir()
            Dim sBlocks = sDataDir + "blocks"
            Dim sChain = sDataDir + "txleveldb"
            Dim sDatabase = sDataDir + "database"
            Dim dBlock As New System.IO.DirectoryInfo(sBlocks)
            RemoveBlocksDir(dBlock)
            Dim dChain As New System.IO.DirectoryInfo(sChain)
            RemoveBlocksDir(dChain)
            Dim dDatabase As New System.IO.DirectoryInfo(sDatabase)
            RemoveBlocksDir(dDatabase)
            Dim y As Integer
            'Delete the blocks File:
            For y = 1 To 5
                Dim sFile As String = "blk000" + Trim(y) + ".dat"
                Dim fi As New System.IO.FileInfo(sDataDir + sFile)
                Try
                    fi.Delete()

                Catch ex As Exception
                    Log("Unable to delete " + ex.Message)
                End Try

            Next

            Dim sSnapshotBlocks = sDataDir + "snapshot\blocks"
            Dim sSnapshotChain = sDataDir + "snapshot\chainstate"
            Dim sSnapshotDatabase = sDataDir + "snapshot\database"
            Dim sSnapshotBlocksIndex = sDataDir + "snapshot\blocks\index"
            Dim dSBlock As New System.IO.DirectoryInfo(sSnapshotBlocks)
            RemoveBlocksDir(dSBlock)
            Dim dsChain As New System.IO.DirectoryInfo(sSnapshotChain)
            RemoveBlocksDir(dsChain)
            Dim dsDatabase As New System.IO.DirectoryInfo(sSnapshotDatabase)
            RemoveBlocksDir(dsDatabase)

            If dSBlock.Exists = False And dsChain.Exists = False And dsDatabase.Exists = False And dDatabase.Exists = False And dChain.Exists = False And dBlock.Exists = False Then Exit For
            Threading.Thread.Sleep(1000)
        Next
    End Sub

    Public Sub Snapshot()
        'Not currently supported
        Exit Sub

        Dim sDataDir As String = GRCDataDir()
        Dim sBlocks = sDataDir + "blocks"
        Dim sChain = sDataDir + "chainstate"
        Dim sSnapshotBlocks = sDataDir + "snapshot\blocks"
        Dim sSnapshotChain = sDataDir + "snapshot\chainstate"
        Dim sSnapDir As String = sDataDir + "snapshot"
        Dim Dsnap As DirectoryInfo = New DirectoryInfo(sSnapDir)
        Try
            Dsnap.Delete(True)
        Catch ex As Exception
        End Try
        DirectorySnapshot(sBlocks, sSnapshotBlocks, True)
        DirectorySnapshot(sChain, sSnapshotChain, True)
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
        Try
            DirectorySnapshot(sSnapshotBlocks, sBlocks, True)
        Catch ex As Exception
        End Try
        Try
            DirectorySnapshot(sSnapshotChain, sChain, True)
        Catch ex As Exception
        End Try
        Try
        Catch ex As Exception

        End Try
    End Sub
    Public Sub ExtractZipFile(archiveFilenameIn As String, outFolder As String)

        Try

            Dim di As New DirectoryInfo(outFolder)

            di.Create()

            Try
                'create blocks
                Dim di2 As New DirectoryInfo(outFolder & "blocks")
                di2.Create()


                'create chainstate
                'create database

            Catch ex As Exception

            End Try
            Try
                Dim di2 As New DirectoryInfo(outFolder & "chainstate")
                di2.Create()

            Catch ex As Exception

            End Try
            Try
                Dim di2 As New DirectoryInfo(outFolder & "database")
                di2.Create()

            Catch ex As Exception

            End Try
        Catch ex As Exception

        End Try

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
                ' Ensure we release resources
                zf.Close()
            End If
        End Try
    End Sub

    Public Sub RestoreFromZipSnapshot()

        Dim sDataDir As String = GRCDataDir()

        Dim sBlocks = sDataDir + "blocks"
        Dim sBlocksIndex = sDataDir + "blocks\index"
        Dim sChain = sDataDir + "chainstate"
        Dim sDatabase = sDataDir + "database"
        Dim sAppDir As String = GetGRCAppDir()

        Dim sSnapshotBlocks = sDataDir + "snapshot\blocks"
        Dim sSnapshotChain = sDataDir + "snapshot\chainstate"
        Dim sSnapshotDatabase = sDataDir + "snapshot\database"
        Dim sSnapshotBlocksIndex = sDataDir + "snapshot\blocks\index"

        Try
            RemoveGrcDataDir()
        Catch ex As Exception

        End Try
        Try
            System.IO.Directory.CreateDirectory(sBlocksIndex)

        Catch ex As Exception

        End Try
        Try
            System.IO.Directory.CreateDirectory(sSnapshotBlocksIndex)

        Catch ex As Exception

        End Try
        Try
            System.IO.Directory.CreateDirectory(sBlocks)

        Catch ex As Exception

        End Try
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




        Try
            ExtractZipFile(sAppDir + "\snapshot.zip", sDataDir)

        Catch ex As Exception

        End Try
        Try
            ' DirectorySnapshot(sSnapshotBlocks, sBlocks, True)
        Catch ex As Exception
        End Try
        Try
            ' DirectorySnapshot(sSnapshotChain, sChain, True)
        Catch ex As Exception
        End Try

        Try
            ' DirectorySnapshot(sSnapshotDatabase, sDatabase, True)

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
        If Not dir.Exists Then
            'Throw New DirectoryNotFoundException( _                "Source directory does not exist or could not be found: " _                + sourceDirName)
        End If
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

    Private Function DownloadFileAsync()
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

    End Function

    Private Function DownloadFile(ByVal sFile As String, Optional sServerFolder As String = "")

        Dim sLocalPath As String = GetGRCAppDir()
        Dim sLocalFile As String = sFile

        If LCase(sLocalFile) = "grcrestarter.exe" Then sLocalFile = "grcrestarter_copy.exe"
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

    End Function
    Public Function GetGRCAppDir() As String
        Try
            If bDebug Then
                Dim fiAlt As New System.IO.FileInfo("c:\program files (x86)\gridcoinresearch\")
                Return fiAlt.DirectoryName

            End If
            Dim fi As New System.IO.FileInfo(Application.ExecutablePath)
            Return fi.DirectoryName
        Catch ex As Exception
        End Try
    End Function
   
    Public Function Base64File(sFileName As String)
        Dim sFilePath As String = GetGRCAppDir() + "\" + sFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Dim sBase64 As String = System.Convert.ToBase64String(b, 0, b.Length)
        b = System.Text.Encoding.ASCII.GetBytes(sBase64)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Function
    Public Function UnBase64File(sFileName As String)
        Dim sFilePath As String = GetGRCAppDir() + "\" + sFileName
        Dim b() As Byte
        b = System.IO.File.ReadAllBytes(sFilePath)
        Dim value As String = System.Text.ASCIIEncoding.ASCII.GetString(b)
        b = System.Convert.FromBase64String(value)
        System.IO.File.WriteAllBytes(sFilePath, b)
    End Function

    Public Function NeedsUpgrade() As Boolean
        Try

            Dim sMsg As String
            Dim sURL As String = GetURL()
            Dim w As New MyWebClient
            Dim sFiles As String
            sFiles = w.DownloadString(sURL)
            Dim vFiles() As String = Split(sFiles, "<br>")
            If UBound(vFiles) < 10 Then
                Return False
            End If

            sMsg = ""
            For iRow As Integer = 0 To UBound(vFiles)
                Dim sRow As String = vFiles(iRow)
                Dim sFile As String = ExtractFilename("<a", "</a>", sRow, 5)
                If Len(sFile) > 1 Then
                    If sFile = "boinc.dll" Then
                        Dim sDT As String
                        sDT = Mid(sRow, 1, 20)
                        sDT = Trim(sDT)

                        Dim dDt As DateTime

                        dDt = ParseDate(Trim(sDT))
                        dDt = TimeZoneInfo.ConvertTime(dDt, System.TimeZoneInfo.Utc)
                        'Hosting server is PST, so subtract Utc - 7 to achieve PST:
                        dDt = DateAdd(DateInterval.Hour, -24, dDt)
                        'local file time
                        Dim sLocalPath As String = GetGRCAppDir()
                        Dim sLocalFile As String = sFile
                        If LCase(sLocalFile) = "grcrestarter.exe" Then sLocalFile = "grcrestarter_copy.exe"
                        Dim sLocalPathFile As String = sLocalPath + "\" + sLocalFile
                        'R Halford - 6/7/2014 - People are upgrading in an endless loop
                        'Move to file version instead

                        '   FileVersionInfo.GetVersionInfo()
                        '   Dim myFileVersionInfo As FileVersionInfo = FileVersionInfo.GetVersionInfo(Environment.SystemDirectory + "\exe")

                        Dim dtLocal As DateTime

                        Try
                            dtLocal = System.IO.File.GetLastWriteTime(sLocalPathFile)
                            dtLocal = TimeZoneInfo.ConvertTime(dtLocal, System.TimeZoneInfo.Utc)

                            Log("Gridcoin.us boinc.dll timestamp (UTC) : " + Trim(dDt) + ", VS : Local boinc.dll timestamp (UTC) : " + Trim(dtLocal))
                            If dDt < dtLocal Then
                                Log("Not upgrading.")
                            End If

                        Catch ex As Exception
                            Return False
                        End Try

                        If dDt > dtLocal Then
                            Log("Upgrading")

                            Return True
                        End If

                    End If
                End If
            Next iRow
        Catch ex As Exception
            Return False

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
    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = "grcrestarter.log"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
        Catch ex As Exception
        End Try

    End Sub
    Public Function DynamicUpgradeWithManifest() As String
        Dim sMsg As String
        For iTry As Long = 1 To 5
            Dim sURL As String = GetURL()
            Dim w As New MyWebClient
            Dim sFiles As String
            sFiles = w.DownloadString(sURL)
            Dim vFiles() As String = Split(sFiles, "<br>")
            ProgressBar1.Maximum = vFiles.Length + 1
            If UBound(vFiles) < 10 Then Return "No mirror found, unable to upgrade."
            sMsg = ""
            ProgressBar1.Maximum = UBound(vFiles) + 1


            For iRow As Integer = 0 To UBound(vFiles)
                Dim sRow As String = vFiles(iRow)
                ProgressBar1.Value = iRow
                ProgressBar1.Update() : ProgressBar1.Refresh() : Me.Refresh() : Application.DoEvents()
                RefreshScreen()

                Dim sFile As String = ExtractFilename("<a", "</a>", sRow, 5)
                If Len(sFile) > 1 Then
                    txtStatus.Text = "Upgrading " + sFile + "..."
                    txtStatus.Width = Me.Width
                    txtStatus.Refresh()
                    txtStatus.Update()
                    Application.DoEvents()

                    Try
                        DownloadFile(sFile)
                    Catch ex As Exception
                        sMsg = sMsg + ex.Message + ".    "
                    End Try
                End If
            Next iRow
            If sMsg = "" Then Exit For
        Next iTry
        Return sMsg
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
            If sOut = "setup.exe" Or LCase(sOut) = "gridcoinresearch.msi" Then sOut = ""
            If InStr(1, sOut, "libstdc") > 0 Then sOut = ""
            If sOut = "testnet" Then sOut = ""
            If sOut = "bitcoin.png" Or sOut = "bitcoin.ico" Then sOut = ""
            If sOut = "qrc_bitcoin.cpp" Then sOut = ""
            If sExt = "xml" Then sOut = ""

            Return Trim(sOut)
        Catch ex As Exception
            Dim message As String = ex.Message


        End Try
    End Function
    Public Function UpgradeGrcRestarter(sParams As String)
        Dim p As Process = New Process()
        Dim pi As ProcessStartInfo = New ProcessStartInfo()
        pi.WorkingDirectory = GetGRCAppDir()
        pi.UseShellExecute = True
        pi.Arguments = sParams
        pi.FileName = Trim("GRCRestarter_copy.exe")
        p.StartInfo = pi
        p.Start()
    End Function




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
