Public Class frmLogin


    Private Sub btnLogin_Click(sender As System.Object, e As System.EventArgs) Handles btnLogin.Click
        txtMessage.Text = GetSessionGuid()
        
        Dim bLogged As Boolean = mGRCData.Authenticate(txtMessage.Text, txtUserName.Text, GetMd5String(txtPassword.Text))
        If bLogged Then
            Me.Hide()
            txtMessage.Text = ""
            Try
                mfrmTicketList.Show()
            Catch ex As Exception

            End Try
        Else
            txtMessage.Text = "Authentication Failed - " + GetSessionGuid()
        End If

    End Sub

    Private Sub btnLogOff_Click(sender As System.Object, e As System.EventArgs) Handles btnLogOff.Click
        txtMessage.Text = "Logged Out"
        mGRCData.LogOff(GetSessionGuid)
    End Sub

    Private Sub frmLogin_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
        txtUserName.Focus()
    End Sub

    Private Sub frmLogin_FormClosing(sender As Object, e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        e.Cancel = True
        Me.Hide()

    End Sub

    Private Sub frmLogin_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        Try

            txtMessage.Text = "" + GetSessionGuid()
            If mGRCData.IsAuthenticated(GetSessionGuid) Then
                mfrmTicketList.Show()
                Me.Dispose()

            End If
            If UserAgent() Like "*iphone*" Then
                Me.Width = Me.Width - 100
                btnRegister.Left = btnRegister.Left - 50
                btnLogOff.Left = btnLogOff.Left - 100
                txtUserName.Width = txtUserName.Width - 100
                txtPassword.Width = txtPassword.Width - 100
            End If

            Me.Top = 0
            txtUserName.Focus()

        Catch ex As Exception
            Dim sErr As String
            sErr = ex.Message
        End Try

    End Sub

    Private Sub btnRegister_Click(sender As System.Object, e As System.EventArgs) Handles btnRegister.Click
        Dim r As New frmRegister
        r.Show()

    End Sub

    Private Sub txtMessage_Click(sender As System.Object, e As System.EventArgs) Handles txtMessage.Click

    End Sub
End Class