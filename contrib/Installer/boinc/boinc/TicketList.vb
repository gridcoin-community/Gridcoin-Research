Imports System.Windows.Forms

Public Class frmTicketList
    Public myGuid As String
    Public WithEvents cms As New ContextMenuStrip
    Public sHandle As String

    Private Sub frmTicketList_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        cmbFilter.Items.Add("My Tickets")
        cmbFilter.Items.Add("Notices")
        cmbFilter.Items.Add("All Tickets")
        cmbFilter.Text = "My Tickets"
        sHandle = KeyValue("handle")

        'Add Handlers for Ticket History Select
        '  AddHandler tvTicketHistory.AfterSelect, AddressOf TicketSelected
        AddHandler tvTicketHistory.MouseDown, AddressOf TicketRightClick

        PopulateTickets()



    End Sub
    Private Sub TicketRightClick(ByVal sender As Object, ByVal e As MouseEventArgs)

        If e.Button = Windows.Forms.MouseButtons.Left Then
            If tvTicketHistory.ContextMenuStrip Is Nothing Then Exit Sub
            tvTicketHistory.ContextMenuStrip.Visible = False
        End If

        If e.Button = Windows.Forms.MouseButtons.Right Then
            tvTicketHistory.SelectedNode = tvTicketHistory.GetNodeAt(e.X, e.Y)
            cms.Items.Clear()
            cms.Items.Add("Open Ticket")
            AddHandler cms.Items(0).Click, AddressOf TicketSelected
            'c'ms.Items.Add("Forward Message")
            'AddHandler cms.Items(1).Click, AddressOf MenuForwardMessageClick
            tvTicketHistory.ContextMenuStrip = cms
            tvTicketHistory.ContextMenuStrip.Show()
        End If

    End Sub

    Private Sub TicketSelected(ByVal sender As Object, ByVal e As System.EventArgs)

        Dim sID As String = tvTicketHistory.SelectedNode.Tag
        Dim ta As New frmTicketAdd
        ta.ShowTicket(sID)


    End Sub
    Public Sub PopulateTickets()
        tvTicketHistory.Nodes.Clear()
        Dim sFilter As String = ""
        sHandle = KeyValue("handle")
        Dim sAssignedTo As String = ""

        Select Case cmbFilter.Text
            Case "My Tickets"
                sFilter = " where 1=1 "
                sAssignedTo = sHandle

            Case "All Tickets"
                sFilter = " where 1=1 "
            Case "Notices"
                sFilter = " where Type = 'Notice' "
        End Select
        Dim dr As GridcoinReader = mGetFilteredTickets(sFilter, sAssignedTo)

        If dr Is Nothing Then Exit Sub
        For i As Integer = 1 To dr.Rows
            'Dim Grr As GridcoinReader.GridcoinRow = dr.GetRow(i)
            Dim sRow As String = dr.Value(i, "TicketId").ToString() + " - " + dr.Value(i, "SubmittedBy") _
                                 + " - " + dr.Value(i, "Type") + " - " + dr.Value(i, "Disposition") _
                                 + " - " + Mid(dr.Value(i, "Descript"), 1, 80) _
                                 + " - " + dr.Value(i, "AssignedTo") + " - " + dr.Value(i, "Added")
            Dim node As TreeNode = New TreeNode(sRow)
            ' node.Tag = dr.Value(i, "id").ToString()
            node.Tag = dr.Value(i, "ticketId").ToString()

            tvTicketHistory.Nodes.Add(node)
        Next i

    End Sub
    Private Sub cmbFilter_SelectedIndexChanged(sender As System.Object, e As System.EventArgs) Handles cmbFilter.SelectedIndexChanged
        PopulateTickets()

    End Sub

    Private Sub btnSubmit_Click(sender As System.Object, e As System.EventArgs)

    End Sub

    Private Sub btnFind_Click(sender As System.Object, e As System.EventArgs) Handles btnAdd.Click
        Dim nt As New frmTicketAdd
        nt.AddTicket()
        nt.Show()

    End Sub

    Private Sub tvTicketHistory_AfterSelect(sender As System.Object, e As System.Windows.Forms.TreeViewEventArgs) Handles tvTicketHistory.AfterSelect

    End Sub

    Private Sub tvTicketHistory_DoubleClick(sender As Object, e As System.EventArgs) Handles tvTicketHistory.DoubleClick
        ' System.Windows.Forms.Cursor.Current = Cursors.WaitCursor
        Dim sID As String = tvTicketHistory.SelectedNode.Tag
        Dim ta As New frmTicketAdd
        ta.ShowTicket(sID)
        'System.Windows.Forms.Cursor.Current = Cursors.Default

    End Sub
End Class