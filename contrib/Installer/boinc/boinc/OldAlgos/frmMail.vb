Imports OpenPop
Imports OpenPop.Mime
Imports OpenPop.Mime.Header
Imports OpenPop.Pop3
Imports OpenPop.Pop3.Exceptions
Imports OpenPop.Common.Logging
Imports System.Collections.Generic
Imports System.Data
Imports System.IO
Imports System.Text
Imports System.Object
Imports System.Security.Cryptography

Imports Message = OpenPop.Mime.Message
Imports GridcoinDotNet.Gridcoin
Imports System.Windows.Forms
Imports boinc.Gridcoin


Public Class frmMail
    Public WithEvents cms As New ContextMenuStrip
    Public saveFile As SaveFileDialog
    Public p3 As Pop3.Pop3Client


    'Dim messages As New Dictionary(Of Long, OpenPop.Mime.Message)

    Public Sub Login()
        Dim sHost As String = KeyValue("pophost")
        Dim sUser As String = KeyValue("popuser")
        Dim sPass As String = KeyValue("poppassword")
        Dim iPort As Long = Val(KeyValue("popport"))
        Dim bSSL As Boolean = IIf(Trim(LCase(KeyValue("popssl"))) = "true", True, False)

        p3 = New Pop3.Pop3Client

        'Add Handlers for Online Storage
        AddHandler listMessages.AfterSelect, AddressOf ListMessagesMessageSelected
        AddHandler listAttachments.AfterSelect, AddressOf ListAttachmentsAttachmentSelected
        AddHandler listMessages.MouseDown, AddressOf MessagesContextHandler
        'Add Handlers for Offline Storage
        AddHandler listOfflineStorage.AfterSelect, AddressOf OfflineMessageSelected
        AddHandler listOfflineStorage.AfterSelect, AddressOf OfflineAttachmentSelected
        AddHandler listOfflineStorage.MouseDown, AddressOf OfflineContextHandler
        If p3.Connected Then

            Windows.Forms.Application.DoEvents()
            p3.Disconnect()

            Windows.Forms.Application.DoEvents()
        End If
        p3.Connect(sHost, iPort, bSSL, 7000, 7000, Nothing)


        Windows.Forms.Application.DoEvents()
        Try
            p3.Authenticate(sUser, sPass)

        Catch ex As Exception

        End Try

        Windows.Forms.Application.DoEvents()
    End Sub
    Public Sub RetrievePop3Emails(Optional ByVal bDisconnect As Boolean = False)


        Try

            ProgressBar1.Visible = True
            listMessages.Height = 147

            Windows.Forms.Application.DoEvents()
            Dim count As Integer

            Try
                If bDisconnect Then If p3.Connected = True Then p3.Disconnect()

                count = p3.GetMessageCount()
                ProgressBar1.Maximum = 20
            Catch ex As Exception
                ProgressBar1.Value = 1

                Login()
                ProgressBar1.Value = 2
                count = p3.GetMessageCount()
                ProgressBar1.Value = 3
            End Try

            '   messages.Clear()

            Windows.Forms.Application.DoEvents()
            listMessages.Nodes.Clear()

            Windows.Forms.Application.DoEvents()
            listAttachments.Nodes.Clear()

            Windows.Forms.Application.DoEvents()
            Dim success As Integer = 0
            Dim fail As Integer = 0
            ProgressBar1.Maximum = count + 2

            For i As Integer = count To 1 Step -1
                If IsDisposed Then
                    Return
                End If
                ProgressBar1.Value = count - i + 2

                Windows.Forms.Application.DoEvents()
                Try
                    Dim h As MessageHeader
                    h = p3.GetMessageHeaders(i)

                    'Dim message As OpenPop.Mime.Message = p3.GetMessage(i)
                    'If the message is encrypted, decrypt it and move it:
                    If h.Subject.Contains("encrypted") Then
                        Dim sRawBody As String = ""
                        Dim sBody As String
                        Dim bIsHTML As Boolean
                        Dim m As Message
                        m = p3.GetMessage(i)

                        GetBodyOfMessage(m, sRawBody, bIsHTML)
                        Dim sDecrypted = Decrypt(sRawBody)
                        Serialize(m, sDecrypted, i, bIsHTML)
                        GoTo dontaddit
                    End If

                    Windows.Forms.Application.DoEvents()

                    ' Add the message to the dictionary using the messageNumber
                    'messages.Add(i, Message)

                    ' Create a TreeNode tree that mimics the Message hierarchy
                    Dim node As TreeNode = New TreeNode(h.Subject + " - " + h.From.ToString() + " - " + Trim(h.DateSent))

                    node.Tag = i
                    listMessages.Nodes.Add(node)
dontaddit:

                    Application.DoEvents()
                    success += 1
                Catch e As Exception
                    DefaultLogger.Log.LogError(("TestForm: Message fetching failed: " + e.Message & vbCr & vbLf & "Stack trace:" & vbCr & vbLf) + e.StackTrace)
                    fail += 1
                End Try

            Next
            PopulateOffLineMessages()
            ProgressBar1.Visible = False
            listMessages.Height = 157

            If fail > 0 Then
                'Add Logging
            End If
        Catch generatedExceptionName As InvalidLoginException
            MessageBox.Show(Me, "The server did not accept the user credentials!", "Authentication Failure")
        Catch generatedExceptionName As PopServerNotFoundException
            MessageBox.Show(Me, "The server could not be found", "POP3 Retrieval")
        Catch generatedExceptionName As PopServerLockedException
            MessageBox.Show(Me, "The mailbox is locked. It might be in use or under maintenance. Are you connected elsewhere?", "POP3 Account Locked")
        Catch generatedExceptionName As LoginDelayException
            MessageBox.Show(Me, "Login not allowed. Server enforces delay between logins. Have you connected recently?", "POP3 Account Login Delay")
        Catch e As Exception
            MessageBox.Show(Me, "Error occurred retrieving mail. " + e.Message, "POP3 Retrieval")
        Finally
            ' Enable the Gridcoin buttons again
        End Try
    End Sub


    Private Sub OfflineMessageSelected(ByVal sender As Object, ByVal e As TreeViewEventArgs)
        Dim fn As String
        fn = listOfflineStorage.SelectedNode.Tag
        Dim sBody As String
        sBody = GetOfflineBody(fn)
        WebBrowser1.DocumentText = sBody
        'Populate the Attachments:
        Dim sFolder As String = GetGridPath("MailAttachments")
        Dim sFG As String
        sFG = Mid(fn, 1, Len(fn) - 4)
        Dim di As New DirectoryInfo(sFolder)
        Dim fiArr As FileInfo() = di.GetFiles()
        Dim fri As FileInfo
        Dim sFGP As String
        listAttachments.Nodes.Clear()
        Dim eFI As New FileInfo(fn)
        For Each fri In fiArr
            If Mid(fri.Name, 1, 35) = Mid(eFI.Name, 1, 35) Then
                sFGP = Mid(fri.FullName, 1, 35)
                Dim zx As New TreeNode
                zx.Text = Mid(fri.FullName, 36, Len(fri.FullName))
                zx.Tag = fri.FullName
                listAttachments.Nodes.Add(zx)
            End If
        Next fri
    End Sub
    Private Sub OfflineAttachmentSelected(ByVal sender As Object, ByVal e As TreeViewEventArgs)

    End Sub
    Private Sub OfflineContextHandler(ByVal sender As Object, ByVal e As MouseEventArgs)

        If e.Button = Windows.Forms.MouseButtons.Left Then
            If listOfflineStorage.ContextMenuStrip Is Nothing Then Exit Sub
            listOfflineStorage.ContextMenuStrip.Visible = False
        End If

        If e.Button = Windows.Forms.MouseButtons.Right Then
            listOfflineStorage.SelectedNode = listOfflineStorage.GetNodeAt(e.X, e.Y)
            cms.Items.Clear()
            cms.Items.Add("Delete Message")
            AddHandler cms.Items(0).Click, AddressOf OfflineDeleteMessageClick
            'cms.Items.Add("Forward Message")
            'AddHandler cms.Items(1).Click, AddressOf MenuForwardMessageClick
            listOfflineStorage.ContextMenuStrip = cms
            listOfflineStorage.ContextMenuStrip.Show()
        End If

    End Sub
    Private Sub ListMessagesMessageSelected(ByVal sender As Object, ByVal e As TreeViewEventArgs)
        ' Fetch the selected message
        Dim message As OpenPop.Mime.Message

        Try
            message = p3.GetMessage(listMessages.SelectedNode.Tag)

        Catch ex As Exception
            Login()
            message = p3.GetMessage(listMessages.SelectedNode.Tag)

        End Try

        ' Clear the attachment list from any previus shown attachments
        listAttachments.Nodes.Clear()

        Dim attachments As List(Of MessagePart) = message.FindAllAttachments()

        For Each attachment As MessagePart In attachments
            ' Add the attachment to the list of attachments
            Dim addedNode As TreeNode = listAttachments.Nodes.Add((attachment.FileName))
            ' Keep a reference to the attachment in the Tag property
            addedNode.Tag = attachment
        Next
        ' Only show that attachmentPanel if there is attachments in the message
        Dim hadAttachments As Boolean = attachments.Count > 0
        ' Generate header table
        Dim dataSet As New DataSet()
        Dim table As DataTable = dataSet.Tables.Add("Headers")
        table.Columns.Add("Header")
        table.Columns.Add("Value")
        Dim rows As DataRowCollection = table.Rows
        ' Add all known headers

        'rows.Add(New Object() {"Content-Description", OpenPop.Mime.Message.Headers.ContentDescription})
        'rows.Add(New Object() {"Content-Id", OpenPop.Mime.Message.Headers.ContentId})
        'For Each keyword As String In OpenPop.Mime.Message.Headers.Keywords
        ' rows.Add(New Object() {"Keyword", keyword})
        ' Next
        'For Each dispositionNotificationTo As RfcMailAddress In OpenPop.Mime.Message.Headers.DispositionNotificationTo
        'rows.Add(New Object() {"Disposition-Notification-To", dispositionNotificationTo})
        'Next
        'For Each received As Received In OpenPop.Mime.Message.Headers.Received
        'rows.Add(New Object() {"Received", Received.Raw})
        'Next
        'rows.Add(New Object() {"Importance", OpenPop.Mime.Message.Headers.Importance})
        'rows.Add(New Object() {"Content-Transfer-Encoding", OpenPop.Mime.Message.Headers.ContentTransferEncoding})
        'For Each cc As RfcMailAddress In OpenPop.Mime.Message.Headers.Cc
        'rows.Add(New Object() {"Cc", cc})
        'Next
        'For Each bcc As RfcMailAddress In OpenPop.Mime.Message.Headers.Bcc
        'rows.Add(New Object() {"Bcc", bcc})
        'Next
        'For Each [to] As RfcMailAddress In OpenPop.Mime.Message.Headers.[To]
        ' rows.Add(New Object() {"To", [to]})
        ' Next
        'rows.Add(New Object() {"From", OpenPop.Mime.Message.Headers.From})
        'rows.Add(New Object() {"Reply-To", OpenPop.Mime.Message.Headers.ReplyTo})
        'For Each inReplyTo As String In OpenPop.Mime.Message.Headers.InReplyTo
        'rows.Add(New Object() {"In-Reply-To", inReplyTo})
        'Next
        'For Each reference As String In OpenPop.Mime.Message.Headers.References
        ' rows.Add(New Object() {"References", reference})
        'Next
        'rows.Add(New Object() {"Sender", OpenPop.Mime.Message.Headers.Sender})
        'rows.Add(New Object() {"Content-Type", OpenPop.Mime.Message.Headers.ContentType})
        'rows.Add(New Object() {"Content-Disposition", OpenPop.Mime.Message.Headers.ContentDisposition})
        'rows.Add(New Object() {"Date", OpenPop.Mime.Message.Headers.[Date]})
        'rows.Add(New Object() {"Date", OpenPop.Mime.Message.Headers.DateSent})
        'rows.Add(New Object() {"Message-Id", OpenPop.Mime.Message.Headers.MessageId})
        'rows.Add(New Object() {"Mime-Version", OpenPop.Mime.Message.Headers.MimeVersion})
        'rows.Add(New Object() {"Return-Path", OpenPop.Mime.Message.Headers.ReturnPath})
        'rows.Add(New Object() {"Subject", OpenPop.Mime.Message.Headers.Subject})
        ' Add all unknown headers
        'For Each key As String In OpenPop.Mime.Message.Headers.UnknownHeaders
        ' Dim values As String() = OpenPop.Mime.Message.Headers.UnknownHeaders.GetValues(key)
        ' If values IsNot Nothing Then
        ' For Each value As String In values
        ' rows.Add(New Object() {key, value})
        ' Next
        'End If
        'Next
        Dim sBody As String
        Dim sRawBody As String = ""
        Dim bIsHTML As Boolean
        sBody = GetBodyOfMessage(message, sRawBody, bIsHTML)
        WebBrowser1.DocumentText = sBody
    End Sub
    Public Function GetBodyOfMessage(ByVal m As OpenPop.Mime.Message, ByRef sRawBody As String, ByRef bIsHTML As Boolean) As String
        ' Find the first text/plain version
        Dim sHeader As String = "<PRE>"
        sHeader = sHeader + "From: " + m.Headers.From.ToString() + vbCrLf
        sHeader = sHeader + "To: " + MaToString(m.Headers.To) + vbCrLf
        sHeader = sHeader + "Date: " + m.Headers.DateSent.ToString() + vbCrLf
        sHeader = sHeader + "CC: " + MaToString(m.Headers.Cc) + vbCrLf
        sHeader = sHeader + "</PRE>" + vbCrLf

        Windows.Forms.Application.DoEvents()
        Dim plainTextPart As MessagePart = m.FindFirstPlainTextVersion()
        Dim htmlPart As MessagePart = m.FindFirstHtmlVersion()
        Dim sBody As String
        sRawBody = ""
        If htmlPart IsNot Nothing Then
            ' The message had a text/plain version - show that one
            sBody = sHeader + htmlPart.GetBodyAsText()
            sRawBody = htmlPart.GetBodyAsText
            bIsHTML = True
        Else
            If plainTextPart IsNot Nothing Then
                sBody = sHeader + "<pre>" + plainTextPart.GetBodyAsText()
                sRawBody = plainTextPart.GetBodyAsText
                bIsHTML = False
            Else

                Dim textVersions As List(Of MessagePart) = m.FindAllTextVersions()
                If textVersions.Count >= 1 Then
                    sBody = sHeader + "<pre>" + textVersions(0).GetBodyAsText()
                    sRawBody = textVersions(0).GetBodyAsText
                    bIsHTML = False
                End If

            End If

        End If
        Return sBody
    End Function
    Public Function Serialize(ByVal m As OpenPop.Mime.Message, ByVal sDecryptedBody As String, ByVal lMessageNumber As Long, ByVal bIsHTML As Boolean)
        'Store the message offline
        Dim sFN As String
        Dim sFG As String

        sFG = Guid.NewGuid.ToString
        sFG = "TEMP"

        sFN = sFG + ".eml"
        Dim sPath As String
        Dim sEmailFolder As String
        sEmailFolder = GetGridPath("Email")
        Try
            If Not System.IO.Directory.Exists(sEmailFolder) Then MkDir(sEmailFolder)

        Catch ex As Exception

        End Try

        Windows.Forms.Application.DoEvents()
        sPath = sEmailFolder + "\" + sFN
        Dim sw As New StreamWriter(sPath)
        sw.WriteLine("FROM: " + m.Headers.From.ToString())
        sw.WriteLine("TO: " + MaToString(m.Headers.To))
        sw.WriteLine("CC: " + MaToString(m.Headers.Cc))
        sw.WriteLine("SUBJECT: " + m.Headers.Subject)
        sw.WriteLine("SENT: " + m.Headers.DateSent)
        sw.WriteLine("TYPE: " + IIf(bIsHTML, "HTML", "PLAINTEXT"))
        sw.WriteLine("BODY: " + vbCrLf + sDecryptedBody)
        sw.Close()
        Dim sMD5 As String
        sMD5 = GetMd5(sPath)
        sFG = sMD5
        Dim sOldName As String
        sOldName = sPath

        sFN = sFG + ".eml"
        sPath = sEmailFolder + "\" + sFN

        Rename(sOldName, sPath)

        'Save attachments
        Dim attachments As List(Of MessagePart) = m.FindAllAttachments()
        For Each attachment As MessagePart In attachments
            ' Add the attachment to the list of attachments
            Dim sFile As String
            sFile = GetGridPath("MailAttachments") + "\" + sFG + "_" + attachment.FileName
            Dim file As New FileInfo(sFile)
            If file.Exists Then file.Delete()
            Try
                attachment.Save(file)
            Catch ex As Exception
            End Try
        Next

        p3.DeleteMessage(lMessageNumber)
        

    End Function
    Public Function GetMd5(ByVal sFN As String) As String
        Dim md5 As Object
        md5 = System.Security.Cryptography.MD5.Create()
        Dim fs As Stream
        fs = File.OpenRead(sFN)
        md5 = md5.ComputeHash(fs)
        fs.Close()
        Dim sOut As String
        sOut = ByteArrayToHexString(md5)
        Return sOut
    End Function

    Public Function ByteArrayToHexString(ByVal ba As Byte()) As String
        Dim hex As StringBuilder
        hex = New StringBuilder(ba.Length * 2)
        For Each b As Byte In ba
            hex.AppendFormat("{0:x2}", b)
        Next

        Return hex.ToString()


    End Function

    Public Sub PopulateOffLineMessages()
        Dim sFolder As String = GetGridPath("Email")
        Dim di As New DirectoryInfo(sFolder)
        Dim fiArr As FileInfo() = di.GetFiles()
        Dim fri As FileInfo
        Dim sSubject As String
        listOfflineStorage.Nodes.Clear()
        For Each fri In fiArr
            sSubject = GetOfflineSubject(fri.FullName)
            Dim zx As New TreeNode
            zx.Text = sSubject
            zx.Tag = fri.FullName
            listOfflineStorage.Nodes.Add(zx)
        Next fri
    End Sub
    Public Function GetOfflineSubject(ByVal sFileName As String) As String
        Dim sr As New StreamReader(sFileName)
        Dim sSubject As String
        For x = 1 To 4
            sSubject = sr.ReadLine()
        Next
        sr.Close()
        Return sSubject
    End Function
    Public Function GetOfflineBody(ByVal sFileName As String) As String
        Dim sr As New StreamReader(sFileName)
        Dim sSubject As String
        'From, To, Subject, Sent, Type
        Dim sHeader As String
        Dim sTemp As String
        sHeader = "<PRE>"
        For x = 1 To 6
            sTemp = sr.ReadLine()
            sHeader = sHeader + sTemp + vbCrLf
        Next
        sHeader = sHeader + vbCrLf + "</PRE>"
        sTemp = sr.ReadLine() 'Body
        Dim sBody As String = ""
        Dim sOut As String
        Do While sr.EndOfStream = False
            sBody = sBody + sr.ReadLine
        Loop
        sr.Close()
        Return sHeader + sBody
    End Function
    Public Shared Function Decrypt(ByVal sIn As String) As String
        Dim sChar As String
        Dim lAsc As Long
        For x = 1 To Len(sIn)
            sChar = Mid(sIn, x, 1)
            lAsc = Asc(sChar) - 1
            Mid(sIn, x, 1) = Chr(lAsc)
        Next
        Return sIn
    End Function
    Public Shared Function Encrypt(ByVal sIn As String) As String
        Dim sChar As String
        Dim lAsc As Long
        For x = 1 To Len(sIn)
            sChar = Mid(sIn, x, 1)
            lAsc = Asc(sChar) + 1
            Mid(sIn, x, 1) = Chr(lAsc)
        Next
        Return sIn
    End Function
    Public Function MaToString(ByVal c As System.Collections.Generic.List(Of OpenPop.Mime.Header.RfcMailAddress)) As String
        Dim sOut As String
        For x = 0 To c.Count - 1
            sOut = sOut + c(x).DisplayName + " [" + c(x).Address + "]; "
        Next
        Return sOut
    End Function
    Private Shared Function GetMessageNumberFromSelectedNode(ByVal node As TreeNode) As Integer
        If node Is Nothing Then
            Throw New ArgumentNullException("node")
        End If
        ' Check if we are at the root, by seeing if it has the Tag property set to an int
        If TypeOf node.Tag Is Integer Then
            Return CInt(node.Tag)
        End If
        ' Otherwise we are not at the root, move up the tree
        Return GetMessageNumberFromSelectedNode(node.Parent)
    End Function
    Private Sub ListAttachmentsAttachmentSelected(ByVal sender As Object, ByVal args As TreeViewEventArgs)
        ' Fetch the attachment part which is currently selected
        If TypeName(listAttachments.SelectedNode.Tag) = "String" Then
            Exit Sub
        End If
        Dim attachment As MessagePart = DirectCast(listAttachments.SelectedNode.Tag, MessagePart)
        saveFile = New SaveFileDialog
        If attachment IsNot Nothing Then
            saveFile.FileName = attachment.FileName
            Dim result As DialogResult = saveFile.ShowDialog()
            If result <> DialogResult.OK Then
                Return
            End If

            ' Now we want to save the attachment
            Dim file As New FileInfo(saveFile.FileName)
            If file.Exists Then
                file.Delete()
            End If
            Try
                attachment.Save(file)
                MessageBox.Show(Me, "Attachment saved successfully")
            Catch e As Exception
                MessageBox.Show(Me, "Attachment saving failed. Exception message: " + e.Message)
            End Try
        Else
            MessageBox.Show(Me, "Attachment object was null!")
        End If
    End Sub
    Private Sub MessagesContextHandler(ByVal sender As Object, ByVal e As MouseEventArgs)

        If e.Button = Windows.Forms.MouseButtons.Left Then
            If listMessages.ContextMenuStrip Is Nothing Then Exit Sub
            listMessages.ContextMenuStrip.Visible = False
        End If

        If e.Button = Windows.Forms.MouseButtons.Right Then
            listMessages.SelectedNode = listMessages.GetNodeAt(e.X, e.Y)
            cms.Items.Clear()
            cms.Items.Add("Delete Message")
            AddHandler cms.Items(0).Click, AddressOf MenuDeleteMessageClick
            cms.Items.Add("Forward Message")
            AddHandler cms.Items(1).Click, AddressOf MenuForwardMessageClick
            listMessages.ContextMenuStrip = cms
            listMessages.ContextMenuStrip.Show()
        End If

    End Sub

    Private Sub MenuForwardMessageClick(ByVal sender As Object, ByVal e As System.EventArgs)
        If listMessages.SelectedNode IsNot Nothing Then
            Dim f As New frmNewEmail
            f.DocumentTemplate = Me.WebBrowser1.DocumentText
            Dim messageNumber As Integer = GetMessageNumberFromSelectedNode(listMessages.SelectedNode)
            Dim m As OpenPop.Mime.Message

            Try

                m = p3.GetMessage(messageNumber)

            Catch ex As Exception
                Try
                    Login()
                    m = p3.GetMessage(messageNumber)

                Catch ex2 As Exception
                    MsgBox(ex.Message, MsgBoxStyle.Critical, "Unable to retrieve source message")
                    Exit Sub

                End Try


            End Try

            f.txtSubject.Text = m.Headers.Subject
            f.txtTo.Text = m.Headers.From.Address
            'Add the attachments
            Dim attachments As List(Of MessagePart) = m.FindAllAttachments()
            For Each attachment As MessagePart In attachments
                ' Add the attachment to the list of attachments
                Dim sPath As String
                '10-21-2013
                sPath = GetGridPath("Temp") + "\" + attachment.FileName
                Dim file As New FileInfo(sPath)
                If file.Exists Then
                    Try
                        file.Delete()

                    Catch ex As Exception

                    End Try
                End If
                Try
                    attachment.Save(file)
                Catch ex As Exception
                End Try
                Dim addedNode As TreeNode = f.listAttachments.Nodes.Add(sPath)
                ' Keep a reference to the attachment in the Tag property
                addedNode.Tag = sPath
                Try
                    f.listAttachments.Nodes.Add(addedNode)
                Catch ex As Exception

                End Try

            Next
            f.Show()

        End If

    End Sub

    Private Sub OfflineDeleteMessageClick(ByVal sender As Object, ByVal e As EventArgs)
        If listOfflineStorage.SelectedNode IsNot Nothing Then
            '   Dim drRet As DialogResult = MessageBox.Show(Me, "Are you sure to delete the email?", "Delete email", MessageBoxButtons.YesNo)
            If 1 = 1 Then
                Dim sFN As String = listOfflineStorage.SelectedNode.Tag

                Try

                    Try
                        Kill(sFN)

                    Catch ex As Exception

                    End Try

                    'Delete the attachments




                    Try
                        Dim fi As New FileInfo(sFN)
                        Dim sAtt As String = GetGridPath("MailAttachments") + "\" + Mid(fi.Name, 1, 34) + "*.*"


                        Kill(sAtt)

                    Catch ex As Exception

                    End Try


                    listOfflineStorage.SelectedNode.Remove()

                    listOfflineStorage.Refresh()
                Catch ex As Exception
                    MsgBox(ex.Message, MsgBoxStyle.Critical, "Error occurred while removing the message.")
                End Try
            End If
        End If
    End Sub

    Private Sub MenuDeleteMessageClick(ByVal sender As Object, ByVal e As EventArgs)
        If listMessages.SelectedNode IsNot Nothing Then
            Dim drRet As DialogResult = MessageBox.Show(Me, "Are you sure to delete the email?", "Delete email", MessageBoxButtons.YesNo)
            If drRet = DialogResult.Yes Then
                Dim messageNumber As Integer = GetMessageNumberFromSelectedNode(listMessages.SelectedNode)
                Try
                    Try

                        p3.DeleteMessage(messageNumber)

                    Catch ex As Exception
                        Login()
                        p3.DeleteMessage(messageNumber)

                    End Try
                    listMessages.SelectedNode.Remove()
                    listMessages.Refresh()
                    RetrievePop3Emails()

                Catch ex As Exception
                    MsgBox(ex.Message, MsgBoxStyle.Critical, "Error occurred while removing the message.")
                End Try
            End If
        End If
    End Sub
    Public Shared Function GetGridPath(ByVal sType As String) As String
        Dim sTemp As String
        sTemp = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData) + "\Gridcoin\" + sType
        If System.IO.Directory.Exists(sTemp) = False Then
            Try
                System.IO.Directory.CreateDirectory(sTemp)
            Catch ex As Exception

            End Try
        End If
        Return sTemp
    End Function
    
    Private Sub UidlButtonClick(ByVal sender As Object, ByVal e As EventArgs)
        Dim uids As List(Of String) = p3.GetMessageUids()

        Dim stringBuilder As New StringBuilder()
        stringBuilder.Append("UIDL:")
        stringBuilder.Append(vbCr & vbLf)
        For Each uid As String In uids
            stringBuilder.Append(uid)
            stringBuilder.Append(vbCr & vbLf)
        Next
        WebBrowser1.DocumentText = stringBuilder.ToString()

    End Sub

    Private Sub MenuViewSourceClick(ByVal sender As Object, ByVal e As EventArgs)
        If listMessages.SelectedNode IsNot Nothing Then
            Dim messageNumber As Integer = GetMessageNumberFromSelectedNode(listMessages.SelectedNode)
            '  Dim m As OpenPop.Mime.Message = messages(messageNumber)
            ' We do not know the encoding of the full message - and the parts could be differently
            ' encoded. Therefore we take a choice of simply using US-ASCII encoding on the raw bytes
            ' to get the source code for the message. Any bytes not in th US-ASCII encoding, will then be
            ' turned into question marks "?"
            '  Dim sourceForm As New ShowSourceForm(Encoding.ASCII.GetString(m.RawMessage))
        End If
    End Sub

    Private Sub MenuStrip1_ItemClicked(ByVal sender As System.Object, ByVal e As System.Windows.Forms.ToolStripItemClickedEventArgs) Handles MenuStrip1.ItemClicked

    End Sub

    Private Sub ComposeNewMessageToolStripMenuItem_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ComposeNewMessageToolStripMenuItem.Click
        Dim f As New frmNewEmail
        f.Show()

    End Sub

    Private Sub RefreshToolStripMenuItem_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles RefreshToolStripMenuItem.Click
        RetrievePop3Emails(True)
    End Sub

    Private Sub ExitToolStripMenuItem_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ExitToolStripMenuItem.Click

        Me.Close()

    End Sub

    Private Sub ProgressBar1_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ProgressBar1.Click

    End Sub
End Class