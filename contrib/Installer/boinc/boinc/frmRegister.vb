Public Class frmRegister


    Private sql As String


    Private Sub frmRegister_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        Me.CenterToScreen()

        txtUserName.BringToFront()

    End Sub
    Private Function VerifyUnique(FieldName As String, FieldValue As String, txtError As String) As Boolean
        Dim ct As Long = mGRCData.CountOf(FieldName, FieldValue)

        If ct > 0 Then
            txtMessage.Text = txtError
            Return False
        End If

        If FieldValue = "" Then
            txtMessage.Text = "Field " + FieldName + " must be populated."
            Return False
        End If
        Return True

    End Function

    Private Sub btnRegister_Click(sender As System.Object, e As System.EventArgs) Handles btnRegister.Click
        Dim bSuccess As Boolean = VerifyUnique("Username", txtUserName.Text, "Sorry, Username is already taken.  Please choose a new username.")
        If Not bSuccess Then Exit Sub
        bSuccess = VerifyUnique("Email", txtEmail.Text, "Sorry E-mail address is in use already; Please choose another.")
        If Not bSuccess Then Exit Sub
        bSuccess = mGRCData.IsValidEmailFormat(txtEmail.Text)
        If Not bSuccess Then txtMessage.Text = "Invalid E-Mail address." : Exit Sub
        'Store
        If Len(txtPassword.Text) < 4 Then txtMessage.Text = "Password too short." : Exit Sub
        If txtPassword.Text <> txtPassword2.Text Then txtMessage.Text = "Passwords do not match, please correct." : Exit Sub

        bSuccess = VerifyUnique("Handle", txtHandle.Text, "Sorry, handle in use - please choose another.")
        If Not bSuccess Then Exit Sub

        Dim sOrg As String = "F0FB9E21-0EDE-4AD4-8F88-EFE3BB917481"
        Dim sMD5 As String = GetMd5String(LCase(txtPassword.Text))
        If Len(txtHandle.Text) = 0 Then
            txtMessage.Text = "Handle must be populated."
            Exit Sub
        End If
        Dim xml As String
        xml = "<FIELDS>Username,organization,firstname,lastname,handle,Email,passwordhash</FIELDS><VALUES>'" + _
            txtUserName.Text + "','" + sOrg _
            + "','" + txtFirstName.Text + "','" + txtLastname.Text + "','" + txtHandle.Text + "','" _
            + txtEmail.Text + "','" + sMD5 + "'</VALUES>"

        Try
            mGRCData.Insert("Users", xml)

        Catch ex As Exception
            txtMessage.Text = "Failed to save new user record, " + ex.Message + "." : Exit Sub

        End Try
        txtMessage.Text = "Saved!"

    End Sub
End Class