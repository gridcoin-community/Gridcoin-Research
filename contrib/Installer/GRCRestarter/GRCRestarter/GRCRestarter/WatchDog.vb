Public Class WatchDog

    Private Sub WatchDog_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated

    End Sub

    Private Sub Timer1_Tick(sender As System.Object, e As System.EventArgs) Handles Timer1.Tick
        'Verify processes are running
        If Not IsRunning("gridcoinresearch*") Then
            DoggieBox.Items.Add("Gridcoin is down!")
            Timer1.Enabled = False

            Try

                KillProcess("gridcoinresearch*")
            'Restart Gridcoinresearch
            DoggieBox.Items.Add("Launching new instance..." + Trim(Now))
            Log("Launching new instance of Gridcoin")
            StartGridcoin()
            Catch ex As Exception

            End Try
            System.Threading.Thread.Sleep(60000)
            Timer1.Enabled = True


        End If
    End Sub


    Public Sub Log(sData As String)
        Try
            Dim sPath As String
            sPath = "watchdog.log"
            Dim sw As New System.IO.StreamWriter(sPath, True)
            sw.WriteLine(Trim(Now) + ", " + sData)
            sw.Close()
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
                    Log(sMetrics)
                    Return True
                End If
            Next
        Catch ex As Exception
        End Try
        Return False
    End Function

    Private Sub WatchDog_FormClosed(sender As Object, e As System.Windows.Forms.FormClosedEventArgs) Handles Me.FormClosed
        Environment.Exit(0)

        End

    End Sub


    Private Sub WatchDog_Load(sender As Object, e As System.EventArgs) Handles Me.Load
        DoggieBox.Items.Clear()
        DoggieBox.Items.Add("Starting WatchDog...")

    End Sub

    Private Sub DoggieBox_SelectedIndexChanged(sender As System.Object, e As System.EventArgs) Handles DoggieBox.SelectedIndexChanged

    End Sub
End Class