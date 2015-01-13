Imports System.Windows.Forms

Public Class frmTicketAdd
    Public myGuid As String
    Public Mode As String
    Public WithEvents cms As New ContextMenuStrip
    Private drHistory As GridcoinReader
    Public sHandle As String = ""


    Private Sub frmTicketAdd_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        sHandle = KeyValue("Handle")
        If sHandle = "" Then
            sHandle = InputBox("To use the ticket system you must have a handle (IE a nickname).  Please enter a handle >", "Please Enter Username", "")
            UpdateKey("Handle", sHandle)
            'Add the Handle to Users
            mInsertUser(sHandle, "")

        End If
        cmbAssignedTo.Items.Clear()

        txtSubmittedBy.Text = sHandle

        Dim gr As GridcoinReader = mGetUsers()
        cmbAssignedTo.Items.Add("Gridcoin")
        cmbAssignedTo.Items.Add("All")
        For y As Integer = 1 To gr.Rows
            cmbAssignedTo.Items.Add(gr.Value(y, "handle"))
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
        txtTicketId.Text = sId
        txtDescription.Text = dr.Value(1, "Descript")
        
        Call PopulateHistory()
        SetViewMode()

        Me.TopMost = True

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
        'Me.BackColor = Drawing.Color.Transparent
        btnSubmit.Enabled = False
        System.Windows.Forms.Cursor.Current = Cursors.WaitCursor

        'Me.Refresh()

        mInsertTicket(Mode, txtSubmittedBy.Text, txtTicketId.Text, cmbAssignedTo.Text, cmbDisposition.Text, txtDescription.Text, cmbType.Text, rtbNotes.Text)
        PopulateHistory()
        SetViewMode()
        ' Me.BackColor = Drawing.Color.Black
        btnSubmit.Enabled = True
        System.Windows.Forms.Cursor.Current = Cursors.Default


    End Sub


    Public Sub PopulateHistory()

        Dim count As Integer
        tvTicketHistory.Nodes.Clear()
        drHistory = mGetTicketHistory(txtTicketId.Text)
        If drHistory Is Nothing Then Exit Sub
        For i As Integer = 1 To drHistory.Rows
            Dim sRow As String = drHistory.Value(i, "Disposition") + " - " + Mid(NormalizeNoteRow(drHistory.Value(i, "notes")), 1, 80) _
                                + " - " + drHistory.Value(i, "AssignedTo") + " - " + drHistory.Value(i, "updated")
            Dim node As TreeNode = New TreeNode(sRow)
            'node.Tag = drHistory.Value(i, "id").ToString()
            node.Tag = i

            tvTicketHistory.Nodes.Add(node)
            rtbNotes.Text = NormalizeNote(drHistory.Value(i, "Notes"))

            cmbAssignedTo.Text = drHistory.Value(1, "AssignedTo")
            cmbDisposition.Text = drHistory.Value(1, "Disposition")
            '            cmbType.Text = dr.Value(1, "Type")
            '            txtTicketId.Text = sId
            '            txtDescription.Text = dr.Value(1, "Descript")



        Next i
    End Sub

    Private Sub btnRefresh_Click(sender As System.Object, e As System.EventArgs)
        Call PopulateHistory()
    End Sub

    Private Sub rtbNotes_TextChanged(sender As System.Object, e As System.EventArgs) Handles rtbNotes.TextChanged

    End Sub

    Private Sub cmbType_Click(sender As Object, e As System.EventArgs) Handles cmbType.Click

    End Sub

    

    Private Sub cmbType_Validating(sender As Object, e As System.ComponentModel.CancelEventArgs) Handles cmbType.Validating
        
    End Sub
End Class