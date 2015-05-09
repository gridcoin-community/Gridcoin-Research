Public Class frmNewUserWizard

   
    Private Sub frmProjects_Activated(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Activated
    End Sub


    Private Function GetProjTextBox(lProjectId As Long) As System.Windows.Forms.TextBox
        Dim c() As Windows.Forms.Control
        c = Me.Controls.Find("txtProject" + Trim(lProjectId), True)
        Dim txt As System.Windows.Forms.TextBox
        txt = DirectCast(c(0), System.Windows.Forms.TextBox)
        Return txt
    End Function

    Private Function GetProjResultsLabel(lProjectId As Long) As System.Windows.Forms.Label
        Dim c() As Windows.Forms.Control
        c = Me.Controls.Find("lblCredits" + Trim(lProjectId), True)
        Dim txt As System.Windows.Forms.Label
        txt = DirectCast(c(0), System.Windows.Forms.Label)
        Return txt
    End Function

    Private Function GetProjLabel(lProjectId As Long, sName As String) As System.Windows.Forms.Label
        Dim c() As Windows.Forms.Control
        c = Me.Controls.Find(sName + Trim(lProjectId), True)
        Dim txt As System.Windows.Forms.Label
        txt = DirectCast(c(0), System.Windows.Forms.Label)
        Return txt
    End Function

    Private Function UserId(lProject As Long) As Double
        Dim sUserId As String
        Dim txt As System.Windows.Forms.TextBox = GetProjTextBox(lProject)
        sUserId = txt.Text
        Return Val(sUserId)
    End Function
    Private Sub NavigateToProjectURL(oLink As Object)
        Dim oLinkLabel As Windows.Forms.LinkLabel = DirectCast(oLink, Windows.Forms.LinkLabel)
        Dim lProjId As Long = Val(Mid(oLinkLabel.Name, Len(oLinkLabel.Name), 1))
        Dim lUserId As Long
        lUserId = UserId(lProjId)
    
        'System.Diagnostics.Process.Start("iexplore.exe", sProjURL)

    End Sub

    Private Sub frmNewUserWizard_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load

        cmbProjects.Items.Clear()
        cmbProjects.Items.Add("Rosetta@home [http://boinc.bakerlab.org/rosetta/]")

        'cmbProjects.Items.Add("World Comunity Grid  [http://www.worldcommunitygrid.org/]")  (WCG has no standard boinc API so it does not work)

        cmbProjects.Items.Add("RNA World    [http://www.rnaworld.de/rnaworld/]")
        cmbProjects.Items.Add("Prime Grid   [http://www.primegrid.com/]")
        cmbProjects.Items.Add("MilkyWay@home    [http://milkyway.cs.rpi.edu/milkyway/]")
        cmbProjects.Items.Add("Malariacontrol.net    [http://www.malariacontrol.net/]")
        cmbProjects.Items.Add("GPUGrid.net  [http://www.gpugrid.net/]")
        'cmbProjects.Items.Add("Climateprediction.net    [http://climateprediction.net/]")  (Climate prediction is missing the Team API - disabling)

        txtEmail.Text = KeyValue("email")
        txtPassword.Text = KeyValue("boincpassword")
        txtUsername.Text = KeyValue("boincusername")
        cmbProjects.SelectedIndex = 0

    End Sub

    Private Sub btnAutomatic_Click(sender As System.Object, e As System.EventArgs) Handles btnAutomatic.Click

        If Len(txtEmail.Text) < 4 Then
            MsgBox("Boinc E-mail address must be populated", vbCritical)
            Exit Sub
        End If

        If Len(txtPassword.Text) < 4 Then
            MsgBox("Boinc password must be populated", vbCritical)
            Exit Sub
        End If

        If Len(cmbProjects.Text) < 4 Then
            MsgBox("A project must be selected.", MsgBoxStyle.Critical)
            Exit Sub
        End If

        If Len(txtUsername.Text) < 4 Then
            MsgBox("Boinc username must be populated", vbCritical)
            Exit Sub
        End If

        txtMessages.Text = ""
        txtUsername.Text = Replace(txtUsername.Text, " ", "_")
        UpdateKey("email", txtEmail.Text)
        UpdateKey("boincpassword", txtPassword.Text)
        UpdateKey("boincusername", txtUsername.Text)
        'Verify boinc is installed:
        If Not IsBoincInstalled() Then
            AppendMsg("Boinc is not installed.  Installing.")
            InstallBoinc()
            'First Time Launch
            If IsBoincInstalled() Then
                AppendMsg("Boinc was successfully installed.")
                'Launch for first time:
                LaunchBoinc()
                AppendMsg("Initializing Boinc Manager.")
                System.Threading.Thread.Sleep(5000)
            Else
                AppendMsg("Boinc was NOT successfully installed.")
            End If
        Else
            AppendMsg("Boinc is already installed.")
        End If
        Dim vURL() As String
        vURL = Split(cmbProjects.Text, "[")
        Dim sURL As String
        sURL = vURL(1)
        sURL = Replace(sURL, "]", "")
        sURL = Replace(sURL, "[", "")

        'Create account
        Try
            System.Windows.Forms.Cursor.Current = System.Windows.Forms.Cursors.WaitCursor
            AppendMsg("Enabling project " + sURL + ".")
            Dim sResult As String = AttachProject(sURL, txtEmail.Text, txtPassword.Text, txtUsername.Text)
            AppendMsg(sResult)
        Catch ex As Exception
            AppendMsg(ex.Message)
        End Try
        
        System.Windows.Forms.Cursor.Current = System.Windows.Forms.Cursors.Default

    End Sub
    Public Sub AppendMsg(sData As String)
        txtMessages.Text = txtMessages.Text + sData + vbCrLf
    End Sub
End Class
