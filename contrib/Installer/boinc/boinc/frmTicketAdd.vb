Imports System.Windows.Forms

Public Class frmTicketAdd
    Public myGuid As String
    Public Mode As String
    Public WithEvents cms As New ContextMenuStrip
    Private drHistory As GridcoinReader
    Public sHandle As String = ""

    Private sHistoryGuid As String

    Private Sub frmTicketAdd_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        sHandle = KeyValue("Handle")
        If sHandle = "" Then
            sHandle = InputBox("To use the ticket system you must have a handle (IE a nickname).  Please enter a handle >", "Please Enter Username", "")
            UpdateKey("Handle", sHandle)
            'Add the Handle to Users
        End If
        Dim sTicketPassword As String = ""
        sTicketPassword = KeyValue("TicketPassword")
        If sTicketPassword = "" Then
            sTicketPassword = InputBox("To use the ticket system you must have a password.  Please enter a password >", "Please Enter Password", "")
            UpdateKey("TicketPassword", sTicketPassword)
            'Add the Handle to Users
        End If


        cmbAssignedTo.Items.Clear()

        mInsertUser(sHandle, "", "", sTicketPassword)


        txtSubmittedBy.Text = sHandle

        Dim gr As GridcoinReader = mGetUsers()
        cmbAssignedTo.Items.Add("Gridcoin")
        cmbAssignedTo.Items.Add("All")
        If gr Is Nothing Then Exit Sub

        For y As Integer = 1 To gr.Rows
            cmbAssignedTo.Items.Add("" & gr.Value(y, "handle"))
        Next

        'Dispositions
        cmbDisposition.Items.Clear()

        cmbDisposition.Items.Add("Programming")
        cmbDisposition.Items.Add("Research")
        cmbDisposition.Items.Add("Code Review")
        cmbDisposition.Items.Add("Test")
        cmbDisposition.Items.Add("Release to Production")
        cmbDisposition.Items.Add("Customer Satisfaction")
        cmbDisposition.Items.Add("Escalate")
        cmbDisposition.Items.Add("Budget Approval")
        cmbDisposition.Items.Add("Closed")

        'Types
        cmbType.Items.Clear()

        cmbType.Items.Add("Code Change")
        cmbType.Items.Add("Customer Service")
        cmbType.Items.Add("Notice")
        'Add Handlers for Ticket History Select
        AddHandler tvTicketHistory.AfterSelect, AddressOf TicketHistorySelected
        AddHandler tvTicketHistory.MouseDown, AddressOf TicketHistoryRightClick



    End Sub

    Private Sub TicketHistoryRightClick(ByVal sender As Object, ByVal e As MouseEventArgs)

        If e.Button = Windows.Forms.MouseButtons.Left Then
            If tvTicketHistory.ContextMenuStrip Is Nothing Then Exit Sub
            tvTicketHistory.ContextMenuStrip.Visible = False
        End If

        If e.Button = Windows.Forms.MouseButtons.Right Then
            tvTicketHistory.SelectedNode = tvTicketHistory.GetNodeAt(e.X, e.Y)
            cms.Items.Clear()
            cms.Items.Add("Open Ticket")
            AddHandler cms.Items(0).Click, AddressOf TicketHistorySelected
            tvTicketHistory.ContextMenuStrip = cms
            tvTicketHistory.ContextMenuStrip.Show()
        End If

    End Sub

    Private Sub TicketHistorySelected(ByVal sender As Object, ByVal e As TreeViewEventArgs)
        Dim sID As String = tvTicketHistory.SelectedNode.Tag
        'Show the history for this selected row
        rtbNotes.Text = NormalizeNote(drHistory.Value(Val(sID), "Notes"))
        cmbAssignedTo.Text = drHistory.Value(Val(sID), "AssignedTo")
        cmbDisposition.Text = drHistory.Value(Val(sID), "Disposition")
        txtAttachment.Text = drHistory.Value(Val(sID), "BlobName")
        txtUpdatedBy.Text = drHistory.Value(Val(sID), "SubmittedBy")


    End Sub
    Public Function NormalizeNote(sData As String)
        Dim sOut As String
        sOut = Replace(sData, "<LF>", vbCrLf)
        Return sOut
    End Function
    Public Function NormalizeNoteRow(sData As String)
        Dim sOut As String
        sOut = Replace(sData, "<LF>", "; ")
        Return sOut
    End Function

    Public Sub ShowTicket(sId As String)
        txtTicketId.Text = sId
        Call frmTicketAdd_Load(Me, Nothing)
        PopulateHistory()

        Me.Show()

        Dim dr As GridcoinReader = mGetTicket(txtTicketId.Text)
        If dr Is Nothing Then Exit Sub
        cmbAssignedTo.Text = dr.Value(1, "AssignedTo")
        cmbDisposition.Text = dr.Value(1, "Disposition")
        cmbType.Text = dr.Value(1, "Type")
        txtSubmittedBy.Text = dr.Value(1, "SubmittedBy")

        txtTicketId.Text = sId
        txtDescription.Text = dr.Value(1, "Descript")
        Call PopulateHistory()
        SetViewMode()
        Me.TopMost = True
        Me.Refresh()
        Me.TopMost = False

    End Sub
    Private Sub SetViewMode()
        Mode = "View"
        txtDescription.ReadOnly = True
        cmbType.Enabled = False

    End Sub
    Public Sub AddTicket()
        Mode = "Add"

        If Mode = "Add" Then
            If Val(txtTicketId.Text) = 0 Then
                Dim maxTick As Double = P2PMax("TicketId", "Ticket", "", "")
                txtTicketId.Text = Trim(maxTick + 1)

            End If
        End If

    End Sub
    Private Sub btnSubmit_Click(sender As System.Object, e As System.EventArgs) Handles btnSubmit.Click
        ' Add the new ticket

        If Len(sHandle) = 0 Then
            MsgBox("Handle Empty", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If (Len(cmbAssignedTo.Text) = 0 Or Len(cmbDisposition.Text) = 0) Then
            MsgBox("Assigned to and Disposition must be populated", MsgBoxStyle.Critical)
            Exit Sub
        End If
        If Len(cmbType.Text) = 0 Then
            MsgBox("Type must be selected.", MsgBoxStyle.Critical)
            Exit Sub
        End If

        If Len(txtDescription.Text) = 0 Or Len(rtbNotes.Text) = 0 Then
            MsgBox("Description And Notes must be populated with data.", MsgBoxStyle.Critical)
            Exit Sub
        End If

        txtSubmittedBy.Text = KeyValue("handle")

        mInsertTicket(Mode, txtSubmittedBy.Text, txtTicketId.Text, cmbAssignedTo.Text, cmbDisposition.Text, txtDescription.Text, cmbType.Text, rtbNotes.Text, KeyValue("TicketPassword"))

        PopulateHistory()
        SetViewMode()
        mfrmTicketList.PopulateTickets()

        btnSubmit.Enabled = True
        System.Windows.Forms.Cursor.Current = Cursors.Default
     
    End Sub


    Public Sub PopulateHistory()
        tvTicketHistory.Nodes.Clear()
        drHistory = mGetTicketHistory(txtTicketId.Text)
        If drHistory Is Nothing Then Exit Sub
        Dim sAttach As String = ""
        Dim grcSecurity As New GRCSec.GRCSec

        For i As Integer = 1 To drHistory.Rows
            Dim sRow As String = drHistory.Value(i, "Disposition") + " - " _
                                 + Mid(NormalizeNoteRow(drHistory.Value(i, "notes")), 1, 80) _
                                + " - " + drHistory.Value(i, "AssignedTo") + " - " + drHistory.Value(i, "updated")
            sAttach = drHistory.Value(i, "BlobName")
            txtAttachment.Text = sAttach
            If Len(sAttach) > 1 Then sAttach = " - [" + sAttach + "]"
            sRow += sAttach

            Dim node As TreeNode = New TreeNode(sRow)
            node.Tag = i
            tvTicketHistory.Nodes.Add(node)
            rtbNotes.Text = NormalizeNote(drHistory.Value(i, "Notes"))
            cmbAssignedTo.Text = drHistory.Value(i, "AssignedTo")
            cmbDisposition.Text = drHistory.Value(i, "Disposition")
         
            'Verify security hash:
            Dim lAuthentic As Long = 0
            lAuthentic = grcSecurity.IsHashAuthentic("" & drHistory.Value(i, "SecurityGuid"), _
                                                     "" & drHistory.Value(i, "SecurityHash"), "" & drHistory.Value(i, "PasswordHash"))
          
            If lAuthentic <> 0 Then node.ForeColor = Drawing.Color.Red


        Next i



    End Sub

    Private Sub btnRefresh_Click(sender As System.Object, e As System.EventArgs)
        Call PopulateHistory()
    End Sub

    Private Sub rtbNotes_TextChanged(sender As System.Object, e As System.EventArgs) Handles rtbNotes.TextChanged

    End Sub

    Private Sub cmbType_Click(sender As Object, e As System.EventArgs) Handles cmbType.Click

    End Sub

    Private Sub cmbDisposition_SelectedIndexChanged(sender As System.Object, e As System.EventArgs) Handles cmbDisposition.SelectedIndexChanged

    End Sub

    Private Sub btnAddAttachment_Click(sender As System.Object, e As System.EventArgs) Handles btnAddAttachment.Click
        If tvTicketHistory.SelectedNode Is Nothing Then MsgBox("You must select a historical ticket history item before attaching.", MsgBoxStyle.Critical) : Exit Sub

        Dim sID As String = tvTicketHistory.SelectedNode.Tag
        If KeyValue("AttachmentPassword") = "" Then
            MsgBox("You must set attachmentpassword in your gridcoinresearch.conf to send attachments.", MsgBoxStyle.Critical)
            Exit Sub
        End If
        Dim sBlobGuid As String
        sBlobGuid = drHistory.Value(Val(sID), "id")
        If Len(sBlobGuid) < 5 Then MsgBox("You must select a historical ticket history item before attaching.", MsgBoxStyle.Critical) : Exit Sub

        Dim OFD As New OpenFileDialog()
        OFD.InitialDirectory = GetGridFolder()
        OFD.Filter = "Text files (*.txt)|*.txt|All files (*.*)|*.*|PDF files (*.pdf)|*.pdf"
        OFD.FilterIndex = 2
        OFD.RestoreDirectory = True
        Me.TopMost = False

        If OFD.ShowDialog() = System.Windows.Forms.DialogResult.OK Then
            Try
                Dim b As Byte()
                b = FileToBytes(OFD.FileName)
                'Encrypt
                b = AES512EncryptData(b, KeyValue("AttachmentPassword"))
                Dim sSuccess As String = mInsertAttachment(sBlobGuid, OFD.SafeFileName, b, KeyValue("TicketPassword"), KeyValue("handle"))
                PopulateHistory()
            Catch Ex As Exception
                MessageBox.Show("Cannot read file from disk. Original error: " & Ex.Message)
            Finally
            End Try
        End If




    End Sub
    Private Function DownloadAttachment() As String
        If KeyValue("AttachmentPassword") = "" Then
            MsgBox("You must set AttachmentPassword in your gridcoinresearch.conf to send attachments.", MsgBoxStyle.Critical)
            Exit Function
        End If

        If Len(txtAttachment.Text) > 1 Then
            'First does it exist already?
            Dim sDir As String = ""
            sDir = GetGridFolder() + "Attachments\"
            Dim sFullPath As String
            sFullPath = sDir + txtAttachment.Text
            If System.IO.Directory.Exists(sDir) = False Then MkDir(sDir)
            If Not System.IO.File.Exists(sFullPath) Then
                'Download from P2P
                Dim sID As String = tvTicketHistory.SelectedNode.Tag
                'Show the history for this selected row
                Dim sBlobGuid As String
                sBlobGuid = drHistory.Value(Val(sID), "BlobGuid")
                If Len(sBlobGuid) < 5 Then MsgBox("Attachment has been removed from the P2P server", MsgBoxStyle.Critical) : Exit Function
                sFullPath = mRetrieveAttachment(sBlobGuid, txtAttachment.Text, KeyValue("AttachmentPassword"))
            End If
            Return sFullPath
        End If

    End Function
    Private Function AttachmentSecurityScan() As Boolean
        If Len(txtAttachment.Text) = 0 Then Exit Function
        Try

        Dim sBlobGuid As String
        Dim sID As String = tvTicketHistory.SelectedNode.Tag
        sBlobGuid = drHistory.Value(Val(sID), "BlobGuid")
        Dim bSuccess = mAttachmentSecurityScan(sBlobGuid)
        If Not bSuccess Then MsgBox("! WARNING !   Attachment came from an unverifiable source.  Virus scan the file and use extreme caution before opening it.", MsgBoxStyle.Critical, "Authenticity Scan Failed")
            Return bSuccess
        Catch ex As Exception
            MsgBox("Document has been removed from the P2P server", MsgBoxStyle.Critical)

        End Try

    End Function
    Private Sub btnOpenAttachment_Click(sender As System.Object, e As System.EventArgs) Handles btnOpenAttachment.Click
       
        Try

        Dim sFullpath As String = DownloadAttachment()
        Dim bAuthScan As Boolean = AttachmentSecurityScan()
        If Not bAuthScan Then Exit Sub 'dont let the user open it
        If System.IO.File.Exists(sFullpath) Then
            'Launch
            Process.Start(sFullpath)
            End If
        Catch ex As Exception
            MsgBox("Document has been removed from the P2P server", MsgBoxStyle.Critical)

        End Try

    End Sub

    Private Sub btnOpenFolder_Click(sender As System.Object, e As System.EventArgs) Handles btnOpenFolder.Click
        Call AttachmentSecurityScan()

        Dim sFullpath As String = DownloadAttachment()
        If System.IO.File.Exists(sFullpath) Then
            Process.Start(GetGridFolder() + "Attachments\")
        Else
            MsgBox("File has been removed from the P2P Server", MsgBoxStyle.Critical)

        End If
    End Sub
End Class