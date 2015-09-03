Imports System.Windows.Forms
Imports System.Text
Imports System.Data
Imports System.Data.SqlClient

Public Class frmFAQ

    Public WithEvents cms As New ContextMenuStrip
    Public _GridRowIndex As Long = 0
    
    Public Sub RefreshFAQ()
        Call frmFAQ_Load(Me, Nothing)
    End Sub
    Private Sub frmFAQ_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        mGRCData = New GRCSec.GridcoinData

        'List the active Foundation Expenses
        Dim sHeading As String = "#;Question;Added;Answers;UpVotes;DownVotes;NetVotes"
        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.SelectionMode = DataGridViewSelectionMode.FullRowSelect
        dgv.EditingPanel.Visible = False
        dgv.AllowUserToAddRows = False
        dgv.ReadOnly = True

        Dim vHeading() As String = Split(sHeading, ";")

        PopulateHeadings(vHeading, dgv, True)

        Dim iRow As Long = 0
        Dim dr As SqlDataReader
        dr = mGRCData.mGetFAQQuestions()
        dgv.Columns(1).MinimumWidth = 500


        Try
            While dr.Read
                dgv.Rows.Add()
                dgv.Rows(iRow).Cells(0).Value = iRow + 1
                dgv.Rows(iRow).Cells(1).Value = dr("Question")
                dgv.Rows(iRow).Height = 60
                dgv.Rows(iRow).Cells(2).Value = Trim(dr("Added")) 'question added date
                dgv.Rows(iRow).Cells(3).Value = dr("AnswerCount")
                dgv.Rows(iRow).Cells(4).Value = dr("AnswerUpvotes")
                dgv.Rows(iRow).Cells(5).Value = dr("AnswerDownvotes")
                dgv.Rows(iRow).Cells(6).Value = dr("AnswerNetVotes")

                dgv.Rows(iRow).Tag = dr("id").ToString()
                iRow += 1
            End While

        Catch ex As Exception
            Log(ex.Message)

        End Try

    End Sub

    Private Sub dgv_CellContentDoubleClick(sender As Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentDoubleClick
        _GridRowIndex = e.RowIndex
        If _GridRowIndex < 0 Then Exit Sub
        Dim sID As String = dgv.Rows(_GridRowIndex).Tag
        Dim dCt As Double = Val(dgv.Rows(_GridRowIndex).Cells(3).Value)
        If Len(sID) > 0 And dCt > 0 Then
            Dim frmVQ As New frmFaqAnswerView
            frmVQ._id = sID
            frmVQ.Show()
        End If
    End Sub
    Private Sub dgv_CellMouseClick(sender As Object, e As System.Windows.Forms.DataGridViewCellMouseEventArgs) Handles dgv.CellMouseClick
        If e.Button = Windows.Forms.MouseButtons.Left Then
            If dgv.ContextMenuStrip Is Nothing Then Exit Sub
            dgv.ContextMenuStrip.Visible = False
        End If
        _GridRowIndex = e.RowIndex
        If _GridRowIndex < 0 Then Exit Sub

        Dim sID As String = dgv.Rows(_GridRowIndex).Tag

        Dim dCt As Double = Val(dgv.Rows(_GridRowIndex).Cells(3).Value)

        If e.Button = Windows.Forms.MouseButtons.Right Then
            If Len(sID) > 1 Then
                Dim _EventList As String = "Answer|View"
                cms.Items.Clear()
                Dim vEventList() As String
                vEventList = Split(_EventList, "|")
                dgv.Rows(e.RowIndex).Selected = True
                For x As Integer = 0 To UBound(vEventList)
                    cms.Items.Add(vEventList(x))
                    AddHandler cms.Items(x).Click, AddressOf ContextMenuSelected
                Next
                dgv.ContextMenuStrip = cms
                dgv.ContextMenuStrip.Show(Cursor.Position)
            End If

        End If
    End Sub
    Private Sub ContextMenuSelected(ByVal sender As Object, ByVal e As System.EventArgs)
        Dim tsmi As ToolStripMenuItem = sender
        If _GridRowIndex = -1 Then
            'User is clicking on the header
            dgv.ContextMenuStrip.Hide()
            Exit Sub
        End If
        Dim sID As String = dgv.Rows(_GridRowIndex).Tag
        Dim dCt As Double = Val(dgv.Rows(_GridRowIndex).Cells(3).Value)


        If tsmi.Text = "Answer" Then
            'Drill into the vote, and chart the vote:
            If Len(sID) > 0 Then
                Dim frmAA As New frmAddAnswer
                frmAA.myGuid = sID
                frmAA.Show()

            End If
        ElseIf tsmi.Text = "View" Then
            If Len(sID) > 0 And dCt > 0 Then
                Dim frmVQ As New frmFaqAnswerView
                frmVQ._id = sID
                frmVQ.Show()
            End If
        End If


    End Sub

    Private Sub dgv_CellContentClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentClick

    End Sub


    Private Sub LogOffToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles LogOffToolStripMenuItem.Click
        mGRCData.LogOff(GetSessionGuid)
    End Sub

    Private Sub ChangePasswordToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles ChangePasswordToolStripMenuItem.Click
        Dim frmP As New frmChangePassword
        frmP.Show()
    End Sub


    Private Sub LogOnToolStripMenuItem1_Click(sender As System.Object, e As System.EventArgs) Handles LogOnToolStripMenuItem1.Click
        Dim frmLogon As New frmLogin
        frmLogon.Show()
    End Sub

    Private Sub btnAdd_Click(sender As System.Object, e As System.EventArgs) Handles btnAdd.Click
        Dim fQ As New frmAddQuestion
        fQ.Show()
    End Sub
End Class