Public Class frmTicketAdd

    Private Sub frmTicketAdd_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        cmbAssignedTo.Items.Add("Rob Halford")
        cmbAssignedTo.Items.Add("Gridcoin")
        cmbAssignedTo.Items.Add("All")
        cmbAssignedTo.Items.Add("Notices")

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

        If Val(txtTicketId.Text) = 0 Then
            txtTicketId.Text = "1"
        End If

    End Sub
End Class