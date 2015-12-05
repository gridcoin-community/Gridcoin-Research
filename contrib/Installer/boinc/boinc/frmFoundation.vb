Imports System.Windows.Forms
Imports System.Text
Imports System.Data
Imports System.Data.SqlClient

Public Class frmFoundation

    Public WithEvents cms As New ContextMenuStrip
    Public _GridRowIndex As Long = 0
  
    Private Sub frmFoundation_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load
        mGRCData = New GRCSec.GridcoinData

        Dim sVoting As String = msGenericDictionary("POLLS")
        Log(sVoting)
        mGRCData = New GRCSec.GridcoinData

        If Len(sVoting) = 0 Then Exit Sub

        'List the active Foundation Expenses
        Dim sHeading As String = "#;Title;Expiration;Share Type;Question;Answers;Total Participants;Total Shares;Best Answer;Type;Amount"
        dgv.Rows.Clear()
        dgv.Columns.Clear()
        dgv.SelectionMode = DataGridViewSelectionMode.FullRowSelect
        dgv.EditingPanel.Visible = False
        dgv.AllowUserToAddRows = False
        dgv.ReadOnly = True

        Dim vHeading() As String = Split(sHeading, ";")

        PopulateHeadings(vHeading, dgv, True)

        Dim iRow As Long = 0
        Dim vPolls() As String = Split(sVoting, "<POLL>")

        Try

            For y As Integer = 0 To UBound(vPolls)
                vPolls(y) = Replace(vPolls(y), "_", " ")
                Dim sTitle As String = ExtractXML(vPolls(y), "<TITLE>", "</TITLE>")
                Dim sExpiration As String = ExtractXML(vPolls(y), "<EXPIRATION>")
                Dim sShareType As String = ExtractXML(vPolls(y), "<SHARETYPE>")
                Dim sQuestion As String = ExtractXML(vPolls(y), "<QUESTION>")
                Dim sAnswers As String = ExtractXML(vPolls(y), "<ANSWERS>")
                Dim sId As String = GetFoundationGuid(sTitle)

                If Len(sId) > 0 Then

                    Dim lDateDiff As Long = DateDiff(DateInterval.Day, Now, GlobalCDate(sExpiration))


                    If Len(sTitle) > 0 And lDateDiff > -7 Then
                        'Array of answers
                        Dim sArrayOfAnswers As String = ExtractXML(vPolls(y), "<ARRAYANSWERS>")
                        Dim vAnswers() As String = Split(sArrayOfAnswers, "<RESERVED>")
                        For subY As Integer = 0 To vAnswers.Length - 1
                            Dim sAnswerName As String = ExtractXML(vAnswers(subY), "<ANSWERNAME>")
                            Dim sParticipants As String = ExtractXML(vAnswers(subY), "<PARTICIPANTS>")
                            Dim sShares As String = ExtractXML(vAnswers(subY), "<SHARES>")

                        Next
                        Dim sTotalParticipants As String = ExtractXML(vPolls(y), "<TOTALPARTICIPANTS>")
                        Dim sTotalShares As String = ExtractXML(vPolls(y), "<TOTALSHARES>")
                        Dim sBestAnswer As String = ExtractXML(vPolls(y), "<BESTANSWER>")
                        dgv.Rows.Add()

                        dgv.Rows(iRow).Cells(0).Value = iRow + 1
                        dgv.Rows(iRow).Cells(1).Value = sTitle
                        dgv.Rows(iRow).Cells(2).Value = sExpiration
                        If lDateDiff < 0 Then dgv.Rows(iRow).Cells(2).Style.BackColor = Drawing.Color.Red

                        dgv.Rows(iRow).Cells(3).Value = sShareType
                        dgv.Rows(iRow).Cells(4).Value = sQuestion
                        If Len(sAnswers) > 81 Then sAnswers = Mid(sAnswers, 1, 81) + "..."
                        dgv.Rows(iRow).Cells(5).Value = sAnswers
                        dgv.Rows(iRow).Cells(6).Value = sTotalParticipants
                        dgv.Rows(iRow).Cells(7).Value = sTotalShares
                        dgv.Rows(iRow).Cells(8).Value = sBestAnswer
                        'Retrieve the Campaign from GFS
                        Dim oExpense As SqlDataReader = mGRCData.GetBusinessObject("Expense", sId, "ExpenseId")
                        If Not oExpense Is Nothing Then
                            If oExpense.HasRows Then
                                dgv.Rows(iRow).Cells(9).Value = oExpense("Type")
                                dgv.Rows(iRow).Cells(10).Value = oExpense("Amount")
                            End If

                        End If

                        iRow += 1
                    End If
                End If

            Next
        Catch ex As Exception
            Log(ex.Message)

        End Try

    End Sub

    Private Sub dgv_CellContentDoubleClick(sender As Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentDoubleClick
        _GridRowIndex = e.RowIndex
        If _GridRowIndex < 0 Then Exit Sub
        Dim sTitle As String = dgv.Rows(_GridRowIndex).Cells(1).Value
        If Len(sTitle) > 0 Then
            Dim frmChart As New frmChartVotes
            frmChart.Show()
            frmChart.ChartPoll(sTitle)
        End If
    End Sub
    Private Sub dgv_CellMouseClick(sender As Object, e As System.Windows.Forms.DataGridViewCellMouseEventArgs) Handles dgv.CellMouseClick
        If e.Button = Windows.Forms.MouseButtons.Left Then
            If dgv.ContextMenuStrip Is Nothing Then Exit Sub
            dgv.ContextMenuStrip.Visible = False
        End If
        _GridRowIndex = e.RowIndex
        If _GridRowIndex < 0 Then Exit Sub

        Dim sTitle As String = dgv.Rows(_GridRowIndex).Cells(1).Value

        If e.Button = Windows.Forms.MouseButtons.Right Then
            If Len(sTitle) > 1 Then
                Dim _EventList As String = "Chart|Vote|View"
               
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
        Dim sTitle As String = dgv.Rows(_GridRowIndex).Cells(1).Value
        Dim sExpiration As String = dgv.Rows(_GridRowIndex).Cells(2).Value
        Dim lDateDiff As Long = DateDiff(DateInterval.Day, Now, GlobalCDate(sExpiration))

        If tsmi.Text = "Chart" Then
            'Drill into the vote, and chart the vote:
            If Len(sTitle) > 0 Then
                Dim frmChart As New frmChartVotes
                frmChart.Show()
                frmChart.ChartPoll(sTitle)
            End If

        ElseIf tsmi.Text = "Vote" Then
            If lDateDiff < 0 Then MsgBox("You may not vote on an expired poll.", MsgBoxStyle.Critical, "Gridcoin Voting System") : Exit Sub

            Dim frmVote As New frmPlaceVote
            frmVote.Show()
            frmVote.PlaceVote(sTitle)
        ElseIf tsmi.Text = "View" Then
            If Len(sTitle) > 0 Then
                Dim frmE As New frmAddExpense
                frmE.Mode = "View"
                frmE.myGuid = GetFoundationGuid(sTitle)
                frmE.Show()

            End If

        End If


    End Sub

    Private Sub dgv_CellContentClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentClick

    End Sub

    Private Sub SubmitExpenseToolStripMenuItem_Click(sender As System.Object, e As System.EventArgs) Handles SubmitExpenseToolStripMenuItem.Click
        Dim fExp As New frmAddExpense
        fExp.Mode = "Add"
        fExp.Show()

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
End Class