Imports System.Windows.Forms
Imports System.Text
Imports System.Data
Imports System.Data.SqlClient

Public Class frmVoting

    Public WithEvents cms As New ContextMenuStrip
    Public _GridRowIndex As Long = 0
    Private Function GlobalCDate(sDate As String) As DateTime
        Try

        Dim year As Long = Val(Mid(sDate, 7, 4))
        Dim day As Long = Val(Mid(sDate, 4, 2))
        Dim m As Long = Val(Mid(sDate, 1, 2))
        Dim dt As DateTime = DateSerial(year, m, day)
        Return dt
        Catch ex As Exception
            Return CDate(Format(sDate, "mm-dd-yyyy"))
        End Try

    End Function


    Private Sub frmVoting_Load(sender As System.Object, e As System.EventArgs) Handles MyBase.Load

        If Not msGenericDictionary.ContainsKey("POLLS") Then
            MsgBox("No voting data exists.")
            Exit Sub
        End If
        Dim sVoting As String = msGenericDictionary("POLLS")
        If Len(sVoting) = 0 Then Exit Sub

        'List the active polls in windows
        Dim sHeading As String = "#;Title;Expiration;Share Type;Question;Answers;Total Participants;Total Shares;URL;Best Answer"
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
        'Autofit headings 7-28-2015


        For y As Integer = 0 To vPolls.Length - 1
            vPolls(y) = Replace(vPolls(y), "_", " ")
            Dim sTitle As String = ExtractXML(vPolls(y), "<TITLE>", "</TITLE>")
            Dim sExpiration As String = ExtractXML(vPolls(y), "<EXPIRATION>")
            Dim sShareType As String = ExtractXML(vPolls(y), "<SHARETYPE>")
            Dim sQuestion As String = ExtractXML(vPolls(y), "<QUESTION>")
            Dim sURL As String = ExtractXML(vPolls(y), "<URL>")

            Dim sAnswers As String = ExtractXML(vPolls(y), "<ANSWERS>")
            Dim bHide As Boolean = False
            Dim sId As String = GetFoundationGuid(sTitle)

            If Len(sTitle) > 0 And Len(sId) = 0 Then

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
                    dgv.Rows(iRow).Cells(8).Value = sURL
                    dgv.Rows(iRow).Cells(9).Value = sBestAnswer
                    iRow += 1
                End If
            End If

        Next
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
                Dim _EventList As String = "Chart|Vote"
                '  If lDateDiff < 0 Then _EventList = "Chart" Else _EventList = "Chart|Vote"

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

        End If

    End Sub

    Private Sub dgv_CellContentClick(sender As System.Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellContentClick
        If e.ColumnIndex = 8 Then
            Process.Start(dgv.Rows(e.RowIndex).Cells(e.ColumnIndex).Value)

        End If

    End Sub

    Private Sub dgv_CellMouseEnter(sender As Object, e As System.Windows.Forms.DataGridViewCellEventArgs) Handles dgv.CellMouseEnter

    End Sub

    Private Sub dgv_CellMouseMove(sender As Object, e As System.Windows.Forms.DataGridViewCellMouseEventArgs) Handles dgv.CellMouseMove
        If e.ColumnIndex = 8 Then
            Me.Cursor = Cursors.Hand
            Try
                dgv.Rows(e.RowIndex).Cells(e.ColumnIndex).Style.BackColor = Drawing.Color.LightPink

            Catch ex As Exception

            End Try
            
        Else
            Me.Cursor = Cursors.Default
            Try
                dgv.Rows(e.RowIndex).Cells(e.ColumnIndex).Style.BackColor = Drawing.Color.Black


            Catch ex As Exception

            End Try
            

        End If

    End Sub
End Class