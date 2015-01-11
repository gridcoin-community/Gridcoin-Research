Imports System.Windows.Forms

Public Class frmTicketList
    Public myGuid As String
    Public WithEvents cms As New ContextMenuStrip

    Private Sub frmTicketList_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        cmbFilter.Items.Add("My Tickets")
        cmbFilter.Items.Add("Notices")
        cmbFilter.Items.Add("All Tickets")

        'Add Handlers for Ticket History Select
        AddHandler tvTicketHistory.AfterSelect, AddressOf TicketSelected
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

    Private Sub TicketSelected(ByVal sender As Object, ByVal e As TreeViewEventArgs)

        Dim sID As String = tvTicketHistory.SelectedNode.Tag


        Dim ta As New frmTicketAdd
        ta.ShowTicket(sID)


    End Sub
    Public Sub PopulateTickets()
        tvTicketHistory.Nodes.Clear()
        Dim sFilter As String = ""
        Select Case cmbFilter.Text
            Case "My Tickets"
                sFilter = " where AssignedTo = 'Halford' "
            Case "All Tickets"
                sFilter = ""
            Case "Notices"
                sFilter = " where Type = 'Notices' "
        End Select
        Dim dr As GridcoinReader = mGetFilteredTickets(sFilter)
        If dr Is Nothing Then Exit Sub
        For i As Integer = 1 To dr.Rows
            'Dim Grr As GridcoinReader.GridcoinRow = dr.GetRow(i)
            Dim sRow As String = dr.Value(i, "Disposition") + " - " + Mid(dr.Value(i, "Descrip"), 1, 80) _
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
End Class