Imports System.Windows.Forms

Public Class frmTicketAdd
    Public myGuid As String
    Public Mode As String

    Private Sub frmTicketAdd_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        cmbAssignedTo.Items.Add("Rob Halford")
        cmbAssignedTo.Items.Add("Gridcoin")
        cmbAssignedTo.Items.Add("All")
        
        'Dispositions
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
        cmbType.Items.Add("Code Change")
        cmbType.Items.Add("Customer Service")
        cmbType.Items.Add("Notice")

        

    End Sub
    Public Sub ShowTicket(sId As String)
        txtTicketId.Text = sId
        Call frmTicketAdd_Load(Me, Nothing)
        PopulateHistory()

        Me.Show()

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
        mInsertTicket(txtTicketId.Text, cmbAssignedTo.Text, cmbDisposition.Text, txtDescription.Text, cmbType.Text, rtbNotes.Text)
        PopulateHistory()

    End Sub


    Public Sub PopulateHistory()

        Dim count As Integer
        tvTicketHistory.Nodes.Clear()

        Dim dr As GridcoinReader = mGetTicketHistory(txtTicketId.Text)
        If dr Is Nothing Then Exit Sub

        For i As Integer = 1 To dr.Rows
            'Dim Grr As GridcoinReader.GridcoinRow = dr.GetRow(i)


            Dim sRow As String = dr.Value(i, "Disposition") + " - " + Mid(dr.Value(i, "notes"), 1, 80) _
                                + " - " + dr.Value(i, "AssignedTo") + " - " + dr.Value(i, "updated")
            Dim node As TreeNode = New TreeNode(sRow)
            node.Tag = dr.Value(i, "id").ToString()

            tvTicketHistory.Nodes.Add(node)
        Next i

    End Sub

    Private Sub btnRefresh_Click(sender As System.Object, e As System.EventArgs) Handles btnRefresh.Click
        Call PopulateHistory()

    End Sub
End Class