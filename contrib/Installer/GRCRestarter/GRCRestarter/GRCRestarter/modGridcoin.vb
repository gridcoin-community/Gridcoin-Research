Module modGridcoin
    Public bTestNet As Boolean
    Public Function KillProcess(ByVal sWildcard As String)
        Try
            For Each p As Process In Process.GetProcesses
                If p.ProcessName Like sWildcard Then
                    p.Kill()
                End If
            Next
        Catch ex As Exception
        End Try
    End Function

    Public Sub StartGridcoin()
        'Start the wallet.
        Try
            Dim p As Process = New Process()
            Dim pi As ProcessStartInfo = New ProcessStartInfo()
            Dim fi As New System.IO.FileInfo(Application.ExecutablePath)
            pi.WorkingDirectory = fi.DirectoryName
            pi.UseShellExecute = True
            pi.FileName = fi.DirectoryName + "\gridcoinresearch.exe"
            If bTestNet Then pi.Arguments = "-testnet"
            pi.WindowStyle = ProcessWindowStyle.Maximized
            pi.CreateNoWindow = False
            p.StartInfo = pi
            p.Start()
        Catch ex As Exception
        End Try
    End Sub


    Public Function IsRunning(ByVal sWildcard As String)
        Try
            For Each p As Process In Process.GetProcesses
                If p.ProcessName Like sWildcard Then
                    Dim sMetrics As String
                    sMetrics = Trim(p.ProcessName) + ";" + Trim(p.StartTime) + ";" + Trim(p.Handle)
                    'Log Memory use here
                    Return True
                End If
            Next
        Catch ex As Exception
        End Try
        Return False
    End Function



End Module
