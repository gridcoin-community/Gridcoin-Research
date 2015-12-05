Imports System.Windows.Forms
Imports System.Data.SqlClient

Public Class frmTicketAdd
    Public myGuid As String
    Public Mode As String
    Public WithEvents cms As New ContextMenuStrip
    Private drHistory As DataTable

    Public sHandle As String = ""

    Private sHistoryGuid As String

    Private Sub frmTicketAdd_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        
        sHandle = mGRCData.GetHandle(GetSessionGuid())

        Try
            If sHandle = "" Or mGRCData.IsAuthenticated(GetSessionGuid()) = False Then
                If mfrmLogin Is Nothing Then
                    mfrmLogin = New frmLogin
                End If
                mfrmLogin.Show()
                Me.Dispose()
                Exit Sub
            End If
            cmbAssignedTo.Items.Clear()
            ' mInsertUser(sHandle, "", "", "")
            txtSubmittedBy.Text = sHandle
            Dim gr As SqlDataReader = mGRCData.mGetUsers()
            cmbAssignedTo.Items.Add("Gridcoin")
            cmbAssignedTo.Items.Add("All")
            If gr Is Nothing Then Exit Sub
            If gr.HasRows Then
                Do While gr.Read
                    cmbAssignedTo.Items.Add("" & gr("handle"))
                Loop
            End If

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

        Catch ex As Exception
            Me.Dispose()
        End Try
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
        rtbNotes.Text = NormalizeNote(drHistory.Rows(Val(sID))("Notes"))

        cmbAssignedTo.Text = drHistory.Rows(Val(sID))("AssignedTo")

        cmbDisposition.Text = drHistory.Rows(Val(sID))("Disposition")


        txtAttachment.Text = "" & drHistory.Rows(Val(sID))("BlobName")

        txtUpdatedBy.Text = "" & drHistory.Rows(Val(sID))("SubmittedBy")

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
        Try

            PopulateHistory()
            Me.Show()
            Dim dr As DataTable = mGetTicket(txtTicketId.Text)
            If dr Is Nothing Then Exit Sub
            cmbAssignedTo.Text = dr.Rows(0)("AssignedTo")
            cmbDisposition.Text = dr.Rows(0)("Disposition")
            cmbType.Text = dr.Rows(0)("Type")
            txtSubmittedBy.Text = dr.Rows(0)("SubmittedBy")
            txtTicketId.Text = sId
            txtDescription.Text = dr.Rows(0)("Descript")
            Call PopulateHistory()
            SetViewMode()
            Me.TopMost = True
            Me.Refresh()
            Me.TopMost = False
        Catch ex As Exception
            Log(ex.Message)

        End Try

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
                Dim maxTick As Double = mGRCData.P2PMax("TicketId", "Ticket", "", "")
                txtTicketId.Text = Trim(Val(maxTick) + 1)
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

        If cmbAssignedTo.Text <> sHandle Then
            If tvTicketHistory.Nodes.Count > 0 Then
                '  MsgBox("Unable to submit changes unless ticket is assigned to you.", vbCritical)
                '  Exit Sub
            End If

        End If

        txtSubmittedBy.Text = sHandle
        mInsertTicket(Mode, txtSubmittedBy.Text, txtTicketId.Text, cmbAssignedTo.Text, cmbDisposition.Text, txtDescription.Text, cmbType.Text, rtbNotes.Text, _
                      GetSessionGuid())
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

        For i As Integer = 0 To drHistory.Rows.Count - 1
            Dim sRow As String = drHistory.Rows(i)("Disposition") + " - " _
                                 + Mid(NormalizeNoteRow(drHistory.Rows(i)("notes")), 1, 80) _
                                + " - " + drHistory.Rows(i)("AssignedTo") + " - " + drHistory.Rows(i)("updated")
            sAttach = "" & drHistory.Rows(i)("BlobName")
            txtAttachment.Text = sAttach
            If Len(sAttach) > 1 Then sAttach = " - [" + sAttach + "]"
            sRow += sAttach

            Dim node As TreeNode = New TreeNode(sRow)
            node.Tag = i
            tvTicketHistory.Nodes.Add(node)
            rtbNotes.Text = NormalizeNote(drHistory.Rows(i)("Notes"))
            cmbAssignedTo.Text = drHistory.Rows(i)("AssignedTo")
            cmbDisposition.Text = drHistory.Rows(i)("Disposition")

            'Verify security hash:
            Dim lAuthentic As Long = 0
            lAuthentic = grcSecurity.IsHashAuthentic("" & drHistory.Rows(i)("SecurityGuid"), _
                                                     "" & drHistory.Rows(i)("SecurityHash"), "" & drHistory.Rows(i)("PasswordHash"))
            'If lAuthentic <> 0 Then node.ForeColor = Drawing.Color.Red
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
        Dim sBlobGuid As String
        sBlobGuid = drHistory.Rows(Val(sID))("id").ToString()


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
                b = AES512EncryptData(b, MerkleRoot)
                Dim sSuccess As String = mGRCData.mInsertAttachment(sBlobGuid, OFD.SafeFileName, b, GetSessionGuid(), mGRCData.GetHandle(GetSessionGuid))
                PopulateHistory()
            Catch Ex As Exception
                MessageBox.Show("Cannot read file from disk. Original error: " & Ex.Message)
            Finally
            End Try
        End If
    End Sub
    Private Function DownloadAttachment() As String
        If tvTicketHistory.SelectedNode Is Nothing Then MsgBox("You must select a historical row.", vbCritical) : Exit Function

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
                sBlobGuid = drHistory.Rows(Val(sID))("BlobGuid").ToString()


                If Len(sBlobGuid) < 5 Then MsgBox("Attachment has been removed from the P2P server", MsgBoxStyle.Critical) : Exit Function
                sFullPath = mRetrieveAttachment(sBlobGuid, txtAttachment.Text, MerkleRoot)
            End If
            Return sFullPath
        End If

    End Function
    Private Function AttachmentSecurityScan() As Boolean
        If Len(txtAttachment.Text) = 0 Then Exit Function
        Try

        Dim sBlobGuid As String
        Dim sID As String = tvTicketHistory.SelectedNode.Tag
            sBlobGuid = drHistory.Rows(Val(sID))("BlobGuid").ToString()


        Dim bSuccess = mAttachmentSecurityScan(sBlobGuid)
            If Not bSuccess And False Then MsgBox("! WARNING !   Attachment came from an unverifiable source.  Virus scan the file and use extreme caution before opening it.", MsgBoxStyle.Critical, "Authenticity Scan Failed")
            Return bSuccess
        Catch ex As Exception
            MsgBox("Document has been removed from the P2P server", MsgBoxStyle.Critical)
        End Try
    End Function
    Private Sub btnOpenAttachment_Click(sender As System.Object, e As System.EventArgs) Handles btnOpenAttachment.Click
       
        Try
            Dim sFullpath As String = DownloadAttachment()
            'Dim bAuthScan As Boolean = AttachmentSecurityScan()
            'If Not bAuthScan And False Then Exit Sub 'dont let the user open it



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

    Private Sub btnVirusScan_Click(sender As System.Object, e As System.EventArgs) Handles btnVirusScan.Click
        '8-3-2015
        Dim sFullpath As String = DownloadAttachment()
        Dim sMd5 As String = GetMd5OfFile(sFullpath)
        Dim webAddress As String = "https://www.virustotal.com/latest-scan/" + sMd5

        Process.Start(webAddress)

    End Sub
End Class