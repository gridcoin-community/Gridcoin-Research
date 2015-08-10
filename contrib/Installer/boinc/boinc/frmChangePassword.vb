Public Class frmChangePassword




    Private Sub frmLogin_Activated(sender As Object, e As System.EventArgs) Handles Me.Activated
        txtNewPassword.Focus()
    End Sub


    Private Sub frmLogin_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        Try

            txtMessage.Text = "" + GetSessionGuid()
            If mGRCData.IsAuthenticated(GetSessionGuid) Then
                mfrmTicketList.Show()
                Me.Dispose()

            End If


            Me.Top = 0
            txtNewPassword.Focus()

        Catch ex As Exception
            Dim sErr As String
            sErr = ex.Message
        End Try

    End Sub

    Private Sub txtMessage_Click(sender As System.Object, e As System.EventArgs) Handles txtMessage.Click

    End Sub

    Private Sub btnRecoverPass_Click(sender As System.Object, e As System.EventArgs) Handles btnChangePassword.Click
        Dim bSuccess As Boolean
        mGRCData = New GRCSec.GridcoinData

        If txtNewPassword.Text <> txtReenter.Text Then
            MsgBox("Passwords do not match.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If Len(txtNewPassword.Text) < 4 Then
            MsgBox("Password must be longer.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        Dim sHashOnly = GetMd5String(txtNewPassword.Text) 'Dont save the password, just the hash

        bSuccess = mGRCData.UpdatePassword(GetSessionGuid(), sHashOnly)

        If bSuccess Then
            MsgBox("Your password has been changed.  ", MsgBoxStyle.Exclamation)
            Exit Sub
        Else
            MsgBox("Unable to change password. Possible reasons: Bad e-mail address on file or you may not be logged in.", MsgBoxStyle.Critical)
            Exit Sub

        End If
    End Sub
End Class