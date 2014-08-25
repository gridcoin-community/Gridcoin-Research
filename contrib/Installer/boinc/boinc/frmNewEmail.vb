Imports System.IO

Imports System.Windows.Forms


Public Class frmNewEmail
    Public DocumentTemplate As String = ""
    Dim lPass As Long
    Public attachFile As OpenFileDialog


    Private Sub frmNewEmail_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load
        txtFrom.Text = KeyValue("popfromemail")
        txtFrom.ReadOnly = True
        btnRichText.Checked = True
        btnHTML.Checked = False
        ShowBodyContainer(DocumentTemplate)

    End Sub
    Private Sub InitialBrowserLoader(ByVal sInitialValue As String)
        Dim doc As HtmlDocument
        ToFile(sInitialValue)
        If lPass = 0 Then WebBrowser1.Navigate(GetGridPath("Email") + "\temp.html")

    End Sub
    Private Sub ShowBodyContainer(ByVal sInitialValue As String)

        If btnRichText.Checked = True Then
            RichTextBox1.Width = WebBrowser1.Width
            RichTextBox1.Height = WebBrowser1.Height
            RichTextBox1.Visible = True
            RichTextBox1.Top = Panel1.Top


            WebBrowser1.Visible = False
            RichTextBox1.Text = sInitialValue
         
        Else
            RichTextBox1.Visible = False
            WebBrowser1.Visible = True
            lPass = 0
            InitialBrowserLoader(sInitialValue)

        End If

    End Sub
    Private Sub ToFile(ByVal sData As String)
        Dim oWriter As New System.IO.StreamWriter(GetGridPath("email") + "\temp.html")
        oWriter.Write(sData)
        oWriter.Close()
    End Sub
    Private Sub WebBrowser1_DocumentCompleted(ByVal sender As Object, ByVal e As System.Windows.Forms.WebBrowserDocumentCompletedEventArgs) Handles WebBrowser1.DocumentCompleted
        lPass = lPass + 1
        If lPass = 1 Then
            WebBrowser1.Document.DomDocument.GetType.GetProperty("designMode").SetValue(WebBrowser1.Document.DomDocument, "On", Nothing)
        End If
    End Sub
    Private Function GetBody() As String
        If btnRichText.Checked Then
            GetBody = RichTextBox1.Text

        Else
            GetBody = WebBrowser1.DocumentText
        End If
    End Function
    Private Sub SetBody(ByVal sBody As String)
        If btnRichText.Checked Then
            RichTextBox1.Text = sBody
        Else
            WebBrowser1.DocumentText = sBody
        End If
    End Sub
    Private Sub btnSend_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnSend.Click
        Dim client As Net.Mail.SmtpClient = New Net.Mail.SmtpClient(KeyValue("smtphost"))
        Dim n As New System.Net.NetworkCredential(KeyValue("popuser"), KeyValue("poppassword"))
        client.UseDefaultCredentials = False
        client.Credentials = n
        client.Port = 587
        client.EnableSsl = True
        Dim msg As Net.Mail.MailMessage = New Net.Mail.MailMessage
        Dim em As New System.Net.Mail.MailAddress(txtFrom.Text, txtFrom.Text)
        msg.From = em
        Dim emto As New System.Net.Mail.MailAddress(txtTo.Text, txtTo.Text)
        msg.To.Add(emto)
        msg.Body = GetBody()


        For x = 0 To listAttachments.Nodes.Count - 1
            Dim sFile As String
            sFile = listAttachments.Nodes(x).Tag
            If System.IO.File.Exists(sFile) Then
                Dim mat As New Net.Mail.Attachment(sFile)
                msg.Attachments.Add(mat)
            End If

        Next

        msg.Subject = txtSubject.Text
        If InStr(1, LCase(msg.Body), "html") > 0 Then msg.IsBodyHtml = True
        Dim sBody As String
        sBody = frmMail.Encrypt(msg.Body)
        msg.Body = sBody
        msg.Subject = msg.Subject + " - encrypted"
        Try
            client.Send(msg)

        Catch ex As Exception
            MsgBox(ex.Message, MsgBoxStyle.Critical, "Error occurred while Sending")
        End Try
        Me.Close()

    End Sub

    Private Sub btnRemove_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRemove.Click

        listAttachments.Nodes.Clear()


    End Sub
    Private Sub btnAdd_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnAdd.Click
        attachFile = New OpenFileDialog
        Dim result As DialogResult = attachFile.ShowDialog()
        If result <> DialogResult.OK Then
            Return
        End If
        Dim file As New FileInfo(attachFile.FileName)
        Dim na As TreeNode
        na = listAttachments.Nodes.Add(file.Name)
        na.Tag = file.FullName
    End Sub

    Private Sub btnHTML_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnHTML.CheckedChanged
        If btnHTML.Checked Then
            Dim sBody As String
            sBody = RichTextBox1.Text

            'SetBody(sBody)
            ShowBodyContainer(sBody)

        End If

    End Sub

    Private Sub btnRichText_CheckedChanged(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnRichText.CheckedChanged
        If btnRichText.Checked Then
            Dim sBody As String
            sBody = WebBrowser1.DocumentText
            RichTextBox1.Text = sBody
            ShowBodyContainer(sBody)
        End If


    End Sub
End Class