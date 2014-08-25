Public Class frmProjects

    Private Sub btnSave_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnSave.Click
        For x = 1 To 5
            Dim sUserId As String
            Dim c() As Windows.Forms.Control
            c = Me.Controls.Find("txtProject" + Trim(x), True)
            sUserId = c(0).Text
            If IsNumeric(sUserId) Then
                UpdateKey("Project" + Trim(x), sUserId)
            End If
        Next
    End Sub
    Private Sub Refresh()
        For x = 1 To 5
            Dim sUserId As String
            Dim txt As System.Windows.Forms.TextBox = GetProjTextBox(x)
            sUserId = KeyValue("Project" + Trim(x))
            txt.Text = Trim(sUserId)
        Next
    End Sub

    Private Sub frmProjects_Activated(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Activated
        Refresh()
        btnQuery_Click(Nothing, Nothing)

    End Sub

    Private Sub GroupBox1_Enter(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles GroupBox1.Enter

    End Sub

    Private Sub frmProjects_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        Me.Hide()
        e.Cancel = True

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
    Private Sub btnQuery_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnQuery.Click
        For x = 1 To 5
            Dim sUserId As String

            sUserId = KeyValue("Project" + Trim(x))

            Dim dCredits As Double
            Dim sTeam As String
            Dim sStruct As String
            sTeam = "N/A"



            ' dCredits = clsGVM.BoincCreditsByProject2(x, Val(sUserId), sStruct, sTeam)

            Windows.Forms.Application.DoEvents()
            System.Threading.Thread.Sleep(100)
            Dim txt As System.Windows.Forms.Label = GetProjResultsLabel(x)

            txt.Text = Trim(dCredits)
            Dim txtTeam As System.Windows.Forms.Label = GetProjLabel(x, "lblTeam")

            txtTeam.Text = Trim(sTeam)

        Next
    End Sub

    Private Sub lnkProject1_LinkClicked(sender As System.Object, e As System.Windows.Forms.LinkLabelLinkClickedEventArgs) _
         Handles lnkProject1.LinkClicked, lnkProject2.LinkClicked, lnkProject3.LinkClicked, lnkProject4.LinkClicked, lnkProject5.LinkClicked
        NavigateToProjectURL(sender)
    End Sub
    Private Sub NavigateToProjectURL(oLink As Object)
        Dim oLinkLabel As Windows.Forms.LinkLabel = DirectCast(oLink, Windows.Forms.LinkLabel)
        Dim lProjId As Long = Val(Mid(oLinkLabel.Name, Len(oLinkLabel.Name), 1))
        Dim lUserId As Long
        lUserId = UserId(lProjId)
        ' Dim sProjURL As String = clsGVM.CalcFriendlyUrl(lProjId, lUserId)

        'System.Diagnostics.Process.Start("iexplore.exe", sProjURL)

    End Sub
End Class
